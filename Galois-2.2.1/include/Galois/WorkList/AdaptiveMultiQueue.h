#ifndef ADAPTIVE_MULTIQUEUE_H
#define ADAPTIVE_MULTIQUEUE_H

#include <atomic>
#include <memory>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <thread>
#include <random>
#include <iostream>
#include "Heap.h"

namespace Galois {
namespace WorkList {

/**
 * Lockable heap structure.
 *
 * @tparam T type of stored elements
 * @tparam Comparer callable defining ordering for objects of type `T`.
 * Its `operator()` returns `true` iff the first argument should follow the second one.
 */
template <typename  T, typename Comparer>
struct LockableHeap {
  DAryHeap<T, Comparer, 8> heap;
  // todo: use atomic
  T min;

  //! Non-blocking lock.
  inline bool try_lock() {
    bool expected = false;
    return _lock.compare_exchange_strong(expected, true);
  }

  //! Blocking lock.
  inline void lock() {
    bool expected = false;
    while (!_lock.compare_exchange_strong(expected, true)) {
      expected = false;
    }
  }

  //! Unlocks the queue.
  inline void unlock() {
    _lock = false;
  }

private:
  std::atomic<bool> _lock;
};

// Probability P / Q
template <size_t PV, size_t QV>
struct Prob {
  static const size_t P = PV;
  static const size_t Q = QV;
};

Prob<1, 1> oneProb;

/**
 * Basic implementation. Provides effective pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam DecreaseKey if auto decrease key should be supported
 * @tparam DecreaseKeyIndexer indexer for decrease key operation
 * @tparam C parameter for queues number
 * @tparam Concurrent if the implementation should be concurrent
 */
template<typename T,
         typename Comparer,
         size_t C = 2,
         bool DecreaseKey = false,
         typename DecreaseKeyIndexer = void,
         bool Concurrent = true,
         bool Blocking = false,
         typename PushChange = Prob<1, 1>,
         typename PopChange = Prob<1, 1>>
class AdaptiveMultiQueue {
private:
  typedef T value_t;
  typedef LockableHeap<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;
  //! Maximum element of type `T`.
  T maxT;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random(); // todo
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }

  size_t generate_random() {
    const auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    static std::mt19937 generator(seed);
    static thread_local std::uniform_int_distribution<size_t> distribution(0, 1024);
    return distribution(generator);
  }

  inline size_t rand_heap() {
    return random() % nQ;
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap) {
    auto result = getMin(heap);
    heap->min = heap->heap.min();
    if constexpr (Blocking) {
      if (heap->heap.size() == 1)
        empty_queues.fetch_add(1);
    }
    heap->unlock();
    return result;
  }

  //! Gets minimum from locked heap which depends on AMQ flags.
  value_t getMin(Heap* heap) {
    if constexpr (DecreaseKey) {
      static DecreaseKeyIndexer indexer;
      return heap->heap.extractMin(indexer);
    } else {
      return heap->heap.extractMin();
    }
  }

  /////////// BLOCKING //////////////////

  struct CondNode {
    enum State {
      FREE,
      SUSPENDED,
      RESUMED
    };

    std::condition_variable cond_var;
    std::atomic<State> state = { FREE };
    std::mutex cond_mutex;
  };

  //! Number of locked queues.
  std::atomic<int> suspended = {0};
  //! All the queues are empty. Rarely changed.
  std::atomic<bool> no_work = {false};
  //! Number of empty queues.
  std::atomic<size_t> empty_queues = { nQ };
  //! Array for threads to suspend.
  std::unique_ptr<CondNode[]> suspend_array;
  //! Suspend array size if CondC * nQ.
  static const size_t CondC = 8;

  //! Size of suspend array.
  inline size_t suspend_size() {
    return nT * CondC;
  }

  inline size_t rand_suspend_cell() {
    return random() % suspend_size();
  }

  //! Suspends if the thread is not the last.
  void suspend() {
    while (empty_queues == nQ) {
      size_t cell_id = rand_suspend_cell();
      if (try_suspend(cell_id)) {
        return;
      }
      for (size_t i = 1; i < 8 && i + cell_id < suspend_size(); i++) {
        if (try_suspend(i + cell_id))
          return;
      }
    }
  }

  //! Resumes at most cnt threads.
  void resume(size_t cnt = 1) {
    if (suspended <= 0) return;
    for (size_t i = 0; i < suspend_size() && cnt != 0; i++) {
      if (try_resume(i))
        cnt--;
    }
  }

  //! Resumes all the threads. Blocking.
  void resume_all() {
    no_work = true;
    for (size_t i = 0; i < suspend_size(); i++) {
      blocking_resume(i);
    }
  }

  //! Locks the cell if it's free.
  bool try_suspend(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::FREE)
      return false;
    if (!node.cond_mutex.try_lock())
      return false;
    if (node.state != CondNode::FREE) {
      node.cond_mutex.unlock();
      return false;
    }
    auto suspended_num = suspended.fetch_add(1);
    if (suspended_num + 1 == nT) {
      node.cond_mutex.unlock();
      suspended.fetch_sub(1);
      if (empty_queues < nQ)
        return true;
      resume_all();
      return true;
    }
    node.state = CondNode::SUSPENDED;
    std::unique_lock<std::mutex> lk(node.cond_mutex, std::adopt_lock);
    node.cond_var.wait(lk, [&node] { return node.state == CondNode::RESUMED; });
    node.state = CondNode::FREE;
    lk.unlock();
    return true;
  }

  //! Resumes if the cell is busy with a suspended thread. Non-blocking.
  bool try_resume(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::SUSPENDED)
      return false;
    if (!node.cond_mutex.try_lock())
      return false;
    if (node.state != CondNode::SUSPENDED) {
      node.cond_mutex.unlock();
      return false;
    }
    std::lock_guard<std::mutex> guard(node.cond_mutex, std::adopt_lock);
    node.state = CondNode::RESUMED;
    node.cond_var.notify_one();
    suspended.fetch_sub(1);
    return true;
  }

  //! Resumes if the cell is busy with a suspended thread. Blocking.
  void blocking_resume(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::SUSPENDED)
      return;
    std::lock_guard<std::mutex> guard(node.cond_mutex);
    if (node.state == CondNode::SUSPENDED) {
      suspended.fetch_sub(1);
      node.state = CondNode::RESUMED;
      node.cond_var.notify_all();
    }
  }

  //! Active waiting to avoid using shared data.
  void active_waiting(int iters) {
    static thread_local std::atomic<int32_t> consumedCPU = { random() % 92001};
    int32_t t = consumedCPU;
    for (size_t i = 0; i < iters; i++)
      t += int32_t(t * 0x5DEECE66DLL + 0xBLL + (uint64_t)i and 0xFFFFFFFFFFFFLL);
    if (t == 42)
      consumedCPU += t;
  }

  //! Number of threads to resume by number of pushed.
  size_t resume_num(size_t n) {
    if (n == 0) return 0;
    static const size_t RESUME_PROB = 16;
    return random() % (n * RESUME_PROB) / RESUME_PROB;
  }

  //////////// DECREASE KEY ///////////////////

  //! Checks whether the index is a valid index of a queue.
  inline bool valid_index(int ind) {
    return ind >= 0 && ind < nQ;
  }

  //! Update element if the new value is smaller that the value in the heap.
  //! The element *must* be in the heap.
  //! Heap *must* be locked.
  inline void update_elem(const value_t& val, Heap* heap) {
    static DecreaseKeyIndexer indexer;
    heap->heap.decrease_key(indexer, val);
  }

  //! Add element to the locked heap with index q_ind.
  //! The element may be in another heap.
  inline void push_elem(const value_t& val, Heap* heap, size_t q_ind) {
    static DecreaseKeyIndexer indexer;
    indexer.set_queue(val, -1, q_ind); // fails if the element was added to another queue
    heap->heap.push(indexer, val);
  }

  std::vector<size_t> local_push;
  std::vector<size_t> local_pop;
  size_t local_threads = 32;

  size_t thread_id = 0;
  size_t inc_thread_id() {
    thread_id = (thread_id + 1) % local_threads;
    return thread_id;
  }

public: // todo: threads
  AdaptiveMultiQueue() : nT(Galois::getActiveThreads()), nQ(C * nT * local_threads), suspend_array(std::make_unique<CondNode[]>(nT * CondC)),
  local_push(local_threads, nQ), local_pop(local_threads, nQ) {
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);

    std::cout << "Queues: " << nQ << std::endl;

    memset(reinterpret_cast<void *>(&maxT), 0xff, sizeof(maxT));
    for (int i = 0; i < nQ; i++) {
      heaps[i].data.heap.set_index(i);
      heaps[i].data.heap.set_max_val(maxT);
      heaps[i].data.min = maxT;
      heaps[i].data.heap.push(heaps[i].data.min);
    }
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef AdaptiveMultiQueue<T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, _concurrent, Blocking, PushChange, PopChange> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<_T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, Concurrent, Blocking, PushChange, PopChange> type;
  };

  //! Push a value onto the queue.
  void push(const value_type &val) {
    Heap* heap;
    int q_ind;

    do {
      q_ind = rand_heap();
      heap = &heaps[q_ind].data;
    } while (!heap->try_lock());

    heap->heap.push(val);
    heap->min = heap->heap.min();
    heap->unlock();
  }

  size_t get_push_local(size_t old_local) {
    size_t change = random() % PushChange::Q;
    if (old_local >= nQ || change < PushChange::P)
      return rand_heap();
    return old_local;
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    if constexpr (Blocking) {
      // Adapt to Galois abort policy.
      if (no_work)
        no_work = false;
    }
    const size_t chunk_size = 8;

    // local queue
    //static thread_local size_t local_q = rand_heap();
    size_t& local_q = local_push[thread_id];

    size_t q_ind = 0;
    int npush = 0;
    Heap* heap = nullptr;

    while (b != e) {
      if constexpr (DecreaseKey) {
        q_ind = DecreaseKeyIndexer::get_queue(*b);
        local_q = valid_index(q_ind) ? q_ind : get_push_local(local_q);
      } else {
        local_q = get_push_local(local_q);
      }
      heap = &heaps[local_q].data;

      while (!heap->try_lock()) {
        if constexpr (DecreaseKey) {
          q_ind = DecreaseKeyIndexer::get_queue(*b);
          local_q = valid_index(q_ind) ? q_ind : rand_heap();
        } else {
          local_q = rand_heap();
        }
        heap = &heaps[local_q].data;
      }

      for (size_t cnt = 0; cnt < chunk_size && b != e; cnt++, npush++) {
        if constexpr (DecreaseKey) {
          auto index = DecreaseKeyIndexer::get_queue(*b);
          if (index == local_q) {
            // the element is in the heap
            update_elem(*b++, heap);
          } else if (index == -1) {
            // no heaps contain the element
            push_elem(*b++, heap, local_q);
          } else {
            // need to change heap
            break;
          }
        } else {
          heap->heap.push(*b++);
        }
      }
      if constexpr (Blocking) {
        if (heap->heap.size() == 2)
          empty_queues.fetch_sub(1);
      }
      heap->min = heap->heap.min();
      heap->unlock();
    }
    if constexpr (Blocking)
      resume(resume_num(npush));
    return npush;
  }

  //! Push initial range onto the queue.
  //! Called with the same b and e on each thread.
  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static const size_t SLEEPING_ATTEMPTS = 8;
    static const size_t RANDOM_ATTEMPTS = 16;

    //static thread_local size_t local_q = rand_heap();
    size_t& local_q = local_pop[thread_id];

    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

    size_t change = random() % PopChange::Q;

    if (local_q < nQ && change >= PopChange::P) {
      heap_i = &heaps[local_q].data;
      if (heap_i->try_lock()) {
        if (heap_i->heap.size() != 1)
          return extract_min(heap_i);
        heap_i->unlock();
      }
    }

    while (true) {
      if constexpr (Blocking) {
        if (no_work) return result;
      }
      do {
        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          do {
            i_ind = rand_heap();
            heap_i = &heaps[i_ind].data;

            j_ind = rand_heap();
            heap_j = &heaps[j_ind].data;

            if (i_ind == j_ind && nQ > 1)
              continue;

            if (compare(heap_i->min, heap_j->min)) {
              heap_i = heap_j;
              local_q = j_ind;
            } else {
              local_q = i_ind;
            }
          } while (!heap_i->try_lock());

          if (heap_i->heap.size() != 1) {
            return extract_min(heap_i);
          } else {
            heap_i->unlock();
          }
        }
        for (size_t k = 1; k < nQ; k++) {
          heap_i = &heaps[(i_ind + k) % nQ].data;
          if (heap_i->min == maxT) continue;
          if (!heap_i->try_lock()) continue;
          if (heap_i->heap.size() > 1) {
            local_q = (i_ind + k) % nQ;
            return extract_min(heap_i);
          }
          heap_i->unlock();
        }
      } while (Blocking && empty_queues != nQ);

      if constexpr (!Blocking)
        return result;
      for (size_t k = 0, iters = 32; k < SLEEPING_ATTEMPTS; k++, iters *= 2) {
        if (empty_queues != nQ || suspended + 1 == nT)
          break;
        active_waiting(iters);
      }
      suspend();
    }
  }
};

} // namespace WorkList
} // namespace Galois

#endif // ADAPTIVE_MULTIQUEUE_H
