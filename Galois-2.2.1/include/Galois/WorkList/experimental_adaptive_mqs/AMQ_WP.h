#ifndef GALOIS_AMQ_WP_H
#define GALOIS_AMQ_WP_H


#include <atomic>
#include <memory>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <thread>
#include <random>
#include <iostream>
#include "../Heap.h"
#include "../WorkListHelpers.h"
#include "AdaptiveMultiQueue.h"

/**
 * Implementation with window + percent
 */

namespace Galois {
namespace WorkList {


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
typename PopChange  = Prob<1, 1>,
size_t ChunkPop = 0,
size_t S = 1, // todo
size_t F = 1,
size_t E = 1,
int LEFT  = -900,
int RIGHT = 900,
size_t WINDOW_SIZE = 128,
int EVENT_PERCENT = 70>
class AMQ_WP {
private:
  typedef T value_t;
  typedef LockableHeap<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  std::atomic<int> nQ = {0};
  const size_t initNQ;
  //size_t nQ; // todo
  //! Maximum element of type `T`.
  // T maxT;
  //! The number of queues is changed under the mutex.
  Runtime::LL::SimpleLock<true> adaptLock;
  const size_t maxQNum;

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
  Galois::optional<value_t> extract_min(Heap* heap, std::vector<value_t>& popped_v, size_t success, size_t failure, size_t empty) {
    auto result = getMin(heap);
    if constexpr (Blocking) {
      if (heap->heap.size() == 0) {
        empty_queues++;
      }
    }
    if constexpr (ChunkPop > 0) {
      for (size_t i = 0; i < ChunkPop; i++) {
        if (heap->heap.size() > 1) {
          popped_v.push_back(getMin(heap));
        } else break;
      }
      std::reverse(popped_v.begin(), popped_v.end());
    }
    heap->min = heap->heap.min();
    heap->size--;
    heap->unlock();
    reportStat(success, failure, empty);
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

public:
  AMQ_WP() : nT(Galois::getActiveThreads()), nQ(C * nT), initNQ(nT * C), suspend_array(std::make_unique<CondNode[]>(nT * CondC)), maxQNum(C * nT * 4) {
    //std::atomic_init(&nQ, C * nT); // todo
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(maxQNum);
    std::cout << "Queues: " << nQ << std::endl;

//    memset(reinterpret_cast<void *>(&maxT), 0xff, sizeof(maxT));
    for (size_t i = 0; i < maxQNum; i++) {
      heaps[i].data.heap.set_index(i);
//      heaps[i].data.heap.set_max_val(maxT);
//      heaps[i].data.min = maxT;
//      heaps[i].data.heap.push(heaps[i].data.min);
    }
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef AMQ_WP<T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, _concurrent, Blocking, PushChange, PopChange, ChunkPop, S, F, E, LEFT, RIGHT, WINDOW_SIZE, EVENT_PERCENT> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AMQ_WP<_T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, Concurrent, Blocking, PushChange, PopChange, ChunkPop, S, F, E, LEFT, RIGHT, WINDOW_SIZE, EVENT_PERCENT> type;
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

  void deleteQueue(size_t expectedNQ) {
    if (!adaptLock.try_lock())
      return;
    size_t curNQ = nQ;
    if (curNQ == 1 || expectedNQ != curNQ) {
      adaptLock.unlock();
      return;
    }
    size_t idMin = 0;
    size_t szMin = SIZE_MAX;
    for (size_t i = 0; i < curNQ - 1; i++) {
      if (szMin > heaps[i].data.size) {
        szMin = heaps[i].data.size;
        idMin = i;
      }
    }
    Heap* mergeH = &heaps[idMin].data;
    mergeH->lock();
    Heap* removeH = &heaps[curNQ - 1].data;
    removeH->lock();
//    while (removeH->heap.size() > 0) {
//      // todo: fix for decrease key
//      mergeH->heap.push(removeH->heap.extractMin());
//    }

    mergeH->heap.pushAllAndClear(removeH->heap);

    // mark the heap empty
    removeH->size = 0;
    // removeH->min = maxT;

    mergeH->size = mergeH->heap.size();
    mergeH->min = mergeH->heap.min();

    nQ--;
    removeH->unlock();
    mergeH->unlock();
    // std::cout << ">>>>>>>>>>>>>>>>>>> Deleted: " << nQ << std::endl;
    adaptLock.unlock();
  }

  void addQueue(size_t expectedNQ) {
    if (!adaptLock.try_lock())
      return;
    size_t curNQ = nQ;
    if (curNQ == maxQNum || expectedNQ != curNQ) {
      adaptLock.unlock();
      return;
    }
    size_t id = 0;
    size_t szVal = 0;
    for (size_t i = 0; i < curNQ; i++) {
      if (szVal < heaps[i].data.size) {
        szVal = heaps[i].data.size;
        id = i;
      }
    }
    Heap* elemsH = &heaps[id].data;
    Heap* addH = &heaps[curNQ].data;
    elemsH->lock();
    addH->lock();

    // todo: if empty
    elemsH->heap.divideElems(addH->heap);

    elemsH->size = elemsH->heap.size();
    elemsH->min = elemsH->heap.min();

    addH->size = addH->heap.size();
    addH->min = addH->heap.min();

    nQ++;
    addH->unlock();
    elemsH->unlock();
    // std::cout << ">>>>>>>>>>>>>>> Added: " << nQ << std::endl;
    adaptLock.unlock();
  }


  inline void reportStat(size_t s, size_t f, size_t e) {
    static thread_local size_t curQ = 0;

    static const size_t windowSize = WINDOW_SIZE;
    static const double event_percent = EVENT_PERCENT / 100.0;
    static thread_local int windowF[windowSize] = { 0 };
    static thread_local int windowE[windowSize] = { 0 };
    static thread_local int windowS[windowSize] = { 0 };
    static thread_local size_t statInd = 0;
    static thread_local int64_t windowSumE = 0;
    static thread_local int64_t windowSumS = 0;
    static thread_local int64_t windowSumF = 0;

    if (curQ != nQ) {
      curQ = nQ;
      memset(&windowE, 0, sizeof(int) * windowSize);
      memset(&windowS, 0, sizeof(int) * windowSize);
      memset(&windowF, 0, sizeof(int) * windowSize);
      windowSumS = 0;
      windowSumF = 0;
      windowSumE = 0;
      statInd = 0;
    }
    windowSumE -= windowE[statInd];
    windowSumF -= windowF[statInd];
    windowSumS -= windowS[statInd];
    windowE[statInd] = e * E;
    windowF[statInd] = f * F;
    windowS[statInd] = s * S;
    windowSumE += windowE[statInd];
    windowSumF += windowF[statInd];
    windowSumS += windowS[statInd];
    int64_t windowSum = windowE[statInd] + windowF[statInd] - windowS[statInd];

    if (windowSum > RIGHT && (windowSumE + windowSumF) / windowSum > event_percent)  {
      addQueue(curQ);
    }
    else if (windowSum < LEFT && windowSumS / windowSum > event_percent) {
      deleteQueue(curQ);
    }
    statInd = (statInd + 1) % windowSize;
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
    static thread_local size_t local_q = rand_heap();

    size_t q_ind = 0;
    int npush = 0;
    Heap* heap = nullptr;

    size_t failure = 0;
    size_t success = 0;
    size_t empty   = 0; // empty doesn't matter in push

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
        failure++;
      }
      success++;

      if (local_q >= nQ) {
        // the queue was "deleted" before we locked it
        heap->unlock();
        continue;
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
        if (heap->heap.size() == 1)
          empty_queues.fetch_sub(1);
      }
      heap->min = heap->heap.min();
      heap->size = heap->heap.size();
      heap->unlock();
    }
    if constexpr (Blocking)
      resume(resume_num(npush));
    reportStat(success, failure, empty);
    return npush;
  }

  bool try_lock_heap(size_t i) {
    if (heaps[i].data.try_lock()) {
      if (i >= nQ) {
        heaps[i].data.unlock();
        return false;
      }
      return true;
    }
    return false;
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
    static thread_local std::vector<value_type> popped_v;
    if constexpr (ChunkPop > 0) {
      if (!popped_v.empty()) {
        auto ret = popped_v.back();
        popped_v.pop_back();
        return ret;
      }
    }
    static const size_t SLEEPING_ATTEMPTS = 8;
    const size_t RANDOM_ATTEMPTS = nT == 1 ? 1 : 4;

    size_t failure = 0;
    size_t success = 0;
    size_t empty   = 0;

    static thread_local size_t local_q = rand_heap();
    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

    size_t change = random() % PopChange::Q;

    if constexpr (ChunkPop > 0) {
      if (local_q < nQ && change >= PopChange::P * (ChunkPop + 1)) {
        heap_i = &heaps[local_q].data;
        if (heap_i->size != 0) {
          if (try_lock_heap(local_q)) {
            if (heap_i->heap.size() != 0) {
              return extract_min(heap_i, popped_v, 1, 0, 0);
            }
            heap_i->unlock();
            success++;
            empty++;
          } else {
            failure++;
          }
        } else {
          empty++;
        }
      }
    } else {
      if (local_q < nQ && change >= PopChange::P) {
        heap_i = &heaps[local_q].data;
        if (try_lock_heap(local_q)) {
          if (heap_i->heap.size() != 0) {
            return extract_min(heap_i, popped_v, 1, 0, 0);
          }
          heap_i->unlock();
          empty++;
          success++;
        } else {
          failure++;
        }
      }
    }

    while (true) {
      if constexpr (Blocking) {
        if (no_work) return result;
      }
      do {
        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          while (true) {
            i_ind = rand_heap();
            heap_i = &heaps[i_ind].data;

            j_ind = rand_heap();
            heap_j = &heaps[j_ind].data;

            if (i_ind == j_ind && nQ > 1)
              continue;

            if (heap_i->size == 0) {
              heap_i = heap_j;
              local_q = i_ind = j_ind;
            } else if (heap_j->size > 0 && compare(heap_i->min, heap_j->min)) {
              heap_i = heap_j;
              local_q = i_ind = j_ind;
            } else {
              local_q = i_ind;
            }
            if (try_lock_heap(local_q))
              break;
            else
              failure++;
          }
          success++;


          if (heap_i->heap.size() != 0) {
            return extract_min(heap_i, popped_v, success, failure, empty);
          } else {
            empty++;
            heap_i->unlock();
          }
        }
        for (size_t k = 1; k < nQ; k++) {
          heap_i = &heaps[(i_ind + k) % nQ].data;
          if (heap_i->size == 0)
            continue;
          heap_i->lock();
          if (heap_i->heap.size() > 0) {
            local_q = (i_ind + k) % nQ;
            return extract_min(heap_i, popped_v, success, failure, empty);
          } else {
            //empty++; // todo should it be counted
            heap_i->unlock();
          }
        }
      } while (Blocking && empty_queues != nQ);

      if constexpr (!Blocking) {
        reportStat(success, failure, empty);
        return result;
      }
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


#endif //GALOIS_AMQ_WP_H
