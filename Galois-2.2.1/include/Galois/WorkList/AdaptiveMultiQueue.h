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
         bool DecreaseKey = false,
         typename DecreaseKeyIndexer = void,
         size_t C = 2,
         bool Concurrent = true,
         bool Blocking = false>
class AdaptiveMultiQueue {
private:
  typedef LockableHeap<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Number of queues.
  const size_t nQ;
  //! Maximum element of type `T`.
  T maxT;

  //! Thread local random.
  size_t rand_heap() {
    static thread_local std::mt19937 generator;
    static thread_local std::uniform_int_distribution<size_t> distribution(0, nQ - 1);
    return distribution(generator);
  }

public:
  AdaptiveMultiQueue() : nQ(Galois::getActiveThreads() * C) {
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);

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
    typedef AdaptiveMultiQueue<T, Comparer, DecreaseKey, DecreaseKeyIndexer, C, _concurrent, Blocking> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<_T, Comparer, DecreaseKey, DecreaseKeyIndexer, C, Concurrent, Blocking> type;
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

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    const size_t chunk_size = 64;

    int npush = 0;

    while (b != e) {
      Heap *heap;
      int q_ind;

      do {
        q_ind = rand_heap();
        heap = &heaps[q_ind].data;
      } while (!heap->try_lock());

      for (size_t cnt = 0; cnt < chunk_size && b != e; cnt++, npush++) {
        heap->heap.push(*b++);
      }
      heap->min = heap->heap.min();
      heap->unlock();
    }
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
    Galois::optional<value_type> result;
    Heap* heap_i, * heap_j;
    int i_ind, j_ind;

    do {
      i_ind = rand_heap();
      heap_i = &heaps[i_ind].data;

      j_ind = rand_heap();
      heap_j = &heaps[j_ind].data;

      if (i_ind == j_ind)
        continue;

      if (compare(heap_i->min, heap_j->min))
        heap_i = heap_j;
    } while (!heap_i->try_lock());

    if (heap_i->heap.size() == 1) {
      heap_i->unlock();
      for (size_t k = 1; k < nQ; k++) {
        heap_i = &heaps[(i_ind + k) % nQ].data;
        if (heap_i->min == maxT) continue;
        if (!heap_i->try_lock()) continue;
        if (heap_i->heap.size() > 1)
          goto deq;
        heap_i->unlock();
      }
      // empty
      return result;
    }

    deq:
    result = heap_i->heap.min();
    heap_i->heap.extractMin();
    heap_i->min = heap_i->heap.min();
    heap_i->unlock();
    return result;
  }
};

/**
 * Specialization with decrease key support.
 *
 * @tparam Indexer indexer for decrease key operation.
 * Should implement `AbstractDecreaseKeyIndexer` interface.
 */
template<typename T,
         typename Comparer,
         typename Indexer,
         size_t C,
         bool Concurrent,
         bool Blocking>
class AdaptiveMultiQueue<T, Comparer, true, Indexer, C, Concurrent, Blocking> {
private:
  typedef LockableHeap<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  const size_t nQ;
  static const bool DecreaseKey = true;
  Indexer indexer;
  T maxT;

  //! Thread local random.
  size_t rand_heap() {
    static thread_local std::mt19937 generator;
    static thread_local std::uniform_int_distribution<size_t> distribution(0, nQ - 1);
    return distribution(generator);
  }

  //! Checks whether the index is a valid index of a queue.
  inline bool valid_index(int ind) {
    return ind >= 0 && ind < nQ;
  }
public:
  AdaptiveMultiQueue() : nQ(Galois::getActiveThreads() * C) {
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);

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
    typedef AdaptiveMultiQueue<T, Comparer, DecreaseKey, Indexer, C, _concurrent, Blocking> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<_T, Comparer, DecreaseKey, Indexer, C, Concurrent, Blocking> type;
  };

  //! Update element if the new value is smaller that the value in the heap.
  //! The element *must* be in the heap.
  //! Heap *must* be locked.
  inline void update_elem(const value_type& val, Heap* heap) {
    heap->heap.decrease_key(indexer, val);
  }

  //! Add element to the locked heap with index q_ind.
  //! The element may be in another heap.
  inline void push_elem(const value_type& val, Heap* heap, size_t q_ind) {
    Indexer::set_queue(val, -1, q_ind); // fails if the element was added to another queue
    heap->heap.push(indexer, val);
  }

  //! Push a value onto the queue.
  void push(const value_type &val) {
    Heap* heap;
    int q_ind;

    do {
      q_ind = Indexer::get_queue(val);
      if (!valid_index(q_ind))
        q_ind = rand_heap();
      heap = &heaps[q_ind].data;
    } while (!heap->try_lock());

    if (Indexer::get_queue() == q_ind) {
      // the element is in the heap
      update_elem(val);
    } else {
      // the element is not in heaps or it has just been added to another heap
      // todo: check how often the second case happens
      push_elem(val, heap, q_ind);
    }
    heap->min = heap->heap.min();
    heap->unlock();
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    const size_t chunk_size = 64;

    size_t npush = 0;

    while (b != e) {
      Heap *heap;
      int q_ind;

      do {
        q_ind = Indexer::get_queue(*b);
        if (!valid_index(q_ind))
          q_ind = rand_heap();
        heap = &heaps[q_ind].data;
      } while (!heap->try_lock());

      for (size_t cnt = 0; cnt < chunk_size && b != e; cnt++, npush++) {
        auto index = Indexer::get_queue(*b);
        if (index == q_ind) {
          // the element is in the heap
          update_elem(*b++, heap);
        } else if (index == -1) {
          // no heaps contain the element
          push_elem(*b++, heap, q_ind);
        } else {
          // need to change heap
          break;
        }
      }
      heap->min = heap->heap.min();
      heap->unlock();
    }
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
    Galois::optional<value_type> result;
    Heap* heap_i, * heap_j;
    int i_ind, j_ind;

    do {
      i_ind = rand_heap();
      heap_i = &heaps[i_ind].data;

      j_ind = rand_heap();
      heap_j = &heaps[j_ind].data;

      if (i_ind == j_ind)
        continue;

      if (compare(heap_i->min, heap_j->min))
        heap_i = heap_j;
    } while (!heap_i->try_lock());

    if (heap_i->heap.size() == 1) {
      heap_i->unlock();
      for (size_t k = 1; k < nQ; k++) {
        heap_i = &heaps[(i_ind + k) % nQ].data;
        if (heap_i->min == maxT) continue;
        if (!heap_i->try_lock()) continue;
        if (heap_i->heap.size() > 1)
          goto deq;
        heap_i->unlock();
      }
      // empty
      return result;
    }

    deq:
    result = heap_i->heap.min();
    heap_i->heap.extractMin(indexer);
    heap_i->min = heap_i->heap.min();
    heap_i->unlock();
    return result;
  }
};

/**
 * Blocking specialization.
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
         bool DecreaseKey,
         typename DecreaseKeyIndexer,
         size_t C,
         bool Concurrent>
class AdaptiveMultiQueue<T, Comparer, DecreaseKey, DecreaseKeyIndexer, C, Concurrent, true> {
private:
  static const bool Blocking = true;
  typedef LockableHeap<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;
  //! Maximum element of type `T`.
  T maxT;
  //! Number of locked queues.
  std::atomic<int> suspended = {0};
  //! All the queues are empty. Rarely changed.
  std::atomic<bool> no_work = {false};
  //! Number of empty queues.
  std::atomic<int> empty_queues = { nQ };
  static const size_t CondC = 8;

  struct CondNode {
    enum State {
      FREE,
      SUSPENDED,
      RESUMED
    };

    std::condition_variable cond_var;
    State state = FREE;
    std::mutex cond_mutex;
  };

  //! Locks the cell if it's free.
  bool try_suspend(size_t id) {
    CondNode& node = suspend_array[id];
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
    std::lock_guard<std::mutex> guard(node.cond_mutex);
    if (node.state == CondNode::SUSPENDED) {
      suspended.fetch_sub(1);
      node.state = CondNode::RESUMED;
      node.cond_var.notify_all();
    }
  }

  std::unique_ptr<CondNode[]> suspend_array;

  //! Size of suspend array.
  inline size_t suspend_size() {
    return nT * CondC;
  }

  //! Suspends if the thread is not the last.
  void suspend() {
    while (empty_queues == nQ) {
      size_t cell_id = rand_suspend_cell();
      if (try_suspend(cell_id)) {
        return;
      }
      for (size_t i = 0; i < 8 && i + cell_id < suspend_size(); i++) {
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

  size_t generate_random() {
    static thread_local std::mt19937 generator;
    static thread_local std::uniform_int_distribution<size_t> distribution(0, 1024);
    return distribution(generator);
  }

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random(); // todo
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }

  inline size_t rand_heap() {
    return random() % nQ;
  }

  inline size_t rand_suspend_cell() {
    return random() % suspend_size();
  }

public:
  AdaptiveMultiQueue() : nT(Galois::getActiveThreads()), nQ(C * nT), suspend_array(std::make_unique<CondNode[]>(nT * CondC)) {
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);

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
    typedef AdaptiveMultiQueue<T, Comparer, DecreaseKey, DecreaseKeyIndexer, C, _concurrent, Blocking> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<_T, Comparer, DecreaseKey, DecreaseKeyIndexer, C, Concurrent, Blocking> type;
  };

  //! Push a value onto the queue.
  // todo: I think it's not called.
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

  //! Number of threads to resume by number of pushed.
  size_t resume_num(size_t n) {
    if (n == 0) return 0;
    static const size_t RESUME_PROB = 16;
    return random() % (n * RESUME_PROB) / RESUME_PROB;
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    if (no_work) {
      no_work = false;
    }
    const size_t chunk_size = 64;

    int npush = 0;

    while (b != e) {
      Heap *heap;
      int q_ind;

      do {
        q_ind = rand_heap();
        heap = &heaps[q_ind].data;
      } while (!heap->try_lock());

      if (heap->heap.size() == 1)
        empty_queues.fetch_sub(1);

      for (size_t cnt = 0; cnt < chunk_size && b != e; cnt++, npush++) {
        heap->heap.push(*b++);
      }
      heap->min = heap->heap.min();
      heap->unlock();
    }
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

  //! Extracts mimumun from the locked heap.
  Galois::optional<value_type> extract_min(Heap* heap) {
    auto result = heap->heap.extractMin();
    heap->min = heap->heap.min();
    if (heap->heap.size() == 1)
      empty_queues.fetch_add(1);
    heap->unlock();
    return result;
  }

  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static const size_t ATTEMPTS = 32;
    static const size_t RANDOM_ATTEMPTS = 16;

    Galois::optional<value_type> result;

    while (true) {
      for (size_t j = 0; j < ATTEMPTS; j++) {
        if (no_work) {
          return result;
        }
        Heap *heap_i, *heap_j;
        int i_ind, j_ind;

        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          do {
            i_ind = rand_heap();
            heap_i = &heaps[i_ind].data;

            j_ind = rand_heap();
            heap_j = &heaps[j_ind].data;

            if (i_ind == j_ind)
              continue;

            if (compare(heap_i->min, heap_j->min))
              heap_i = heap_j;
          } while (!heap_i->try_lock());

          if (heap_i->heap.size() != 1) {
            return extract_min(heap_i);
          } else {
            heap_i->unlock();
          }
        }

        for (size_t k = 0; k < nQ; k++) {
          heap_i = &heaps[k].data;
          if (!heap_i->try_lock()) continue;
          if (heap_i->heap.min() != maxT) {
            return extract_min(heap_i);
          }
          heap_i->unlock();
        }
      }
      suspend();
    }
  }
};



} // namespace WorkListempty_q
} // namespace Galois

#endif // ADAPTIVE_MULTIQUEUE_H
