#ifndef MQ_LOCAL_PROB_NUMA
#define MQ_LOCAL_PROB_NUMA

#include <atomic>
#include <memory>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <fstream>
#include <thread>
#include <random>
#include <iostream>
#include "HeapWithLock.h"

namespace Galois {
namespace WorkList {

/**
 * MultiQueue optimized variant. Uses task batching idea for both
 * `push` and temporal locality `pop`.
 *
 * Provides efficient pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam PushSize Number of elements to push onto one queue.
 * @tparam ChangeQPop "Local" queue for pop changed  with 1 / ChangeQPop probability.
 * @tparam C parameter for queues number
 * @tparam Prior Type of T's priority. Need to support < operator.
 * @tparam Concurrent if the implementation should be concurrent
 */
template<typename T,
         typename Comparer,
         size_t PushSize,
         size_t ChangeQPop,
         size_t C,
         size_t LOCAL_NUMA_W,
         typename Prior = unsigned long,
         bool Concurrent = true>
class MultiQueueLocalProbNuma {
private:
  typedef T value_t;
  typedef HeapWithLock<T, Comparer, Prior, 8> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;

  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> pushLocal;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random() + 1;
    uint32_t local_x = x;
    local_x ^= local_x << 13;
    local_x ^= local_x >> 17;
    local_x ^= local_x << 5;
    x = local_x;
    return local_x;
  }

  size_t generate_random() {
    const auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    static std::mt19937 generator(seed);
    static thread_local std::uniform_int_distribution<size_t> distribution(0, 1024);
    return distribution(generator);
  }

//  inline size_t rand_heap() {
//    return random() % nQ;
//  }
//
//  inline size_t rand_heap_with_local() {
//    return random() % (nQ + 1);
//  }

#include "NUMA.h"

  Heap* get_heap_ptr(size_t id) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    if (id == nQ) {
      return &pushLocal[tId].data;
    }
    return &heaps[id].data;
  }

  bool try_lock_heap(size_t id) {
    if (id == nQ) {
      return true;
    }
    return heaps[id].data.try_lock();
  }


  void unlock_heap(size_t id) {
    if (id == nQ) {
      return;
    }
    heaps[id].data.unlock();
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap, size_t heapId) {
    auto result = heap->extractMin();
    heap->updateMin();
    if (heapId < nQ) {
      heap->unlock();
    }
    return result;
  }


public:
  MultiQueueLocalProbNuma() : nT(Galois::getActiveThreads()), nQ(C * nT) {
    // Setting dummy element of the heap
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Prior));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    pushLocal = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nT);
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueLocalProbNuma<T, Comparer, PushSize, ChangeQPop, C, LOCAL_NUMA_W, Prior, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueLocalProbNuma<_T, Comparer, PushSize, ChangeQPop, C, LOCAL_NUMA_W, Prior, Concurrent> type;
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
    heap->min.store(heap->heap.min().prior(), std::memory_order_release);
    heap->unlock();
  }

  size_t lockRandomQ() {
    auto r = rand_heap();
    while (!heaps[r].data.try_lock()) {
      r = rand_heap();
    }
    return r;
  }

  void pushLocally(T val) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    auto local = &pushLocal[tId].data;
    local->heap.push(val);
    if (local->heap.size() >= PushSize) {
      auto id = lockRandomQ();
      auto heap = &heaps[id].data;
      while (!local->heap.empty()) {
        heap->push(local->extractMin());
      }
      heap->updateMin();
      heap->unlock();
    }
    local->updateMin();
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    unsigned int pushNumber = 0;
    while (b != e) {
      pushNumber++;
      pushLocally(*b++);
    }
    return pushNumber;
  }


  //! Push initial range onto the queue.
  //! Called with the same b and e on each thread.
  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  bool isFirstLess(Prior const& v1, Prior const& v2) {
    if (Heap::isMinDummy(v1)) {
      return false;
    }
    if (Heap::isMinDummy(v2)) {
      return true;
    }
    return v1 < v2;
  }



  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    static const size_t ATTEMPTS = 4;
    static thread_local size_t local_q = rand_heap();
    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

    // change == 0 -- the local queue should be changed
    // otherwise, we try to pop from the local queue
    size_t change = random() % ChangeQPop;

    if (change > 0) {
      heap_i = get_heap_ptr(local_q);
      if (try_lock_heap(local_q)) {
        if (!heap_i->heap.empty()) {
          return extract_min(heap_i, local_q);
        }
        unlock_heap(local_q);
      }
    }


    for (size_t i = 0; i < ATTEMPTS; i++) {
      while (true) {
        i_ind = rand_heap_with_local();
        heap_i = get_heap_ptr(i_ind);

        j_ind = rand_heap_with_local();
        heap_j = get_heap_ptr(j_ind);

        if (i_ind == j_ind && nQ > 1)
          continue;
        if (isFirstLess(heap_j->getMin(), heap_i->getMin())) {
          i_ind = j_ind;
          heap_i = heap_j;
        }
        if (try_lock_heap(i_ind))
          break;
      }
      if (!heap_i->heap.empty()) {
        local_q = i_ind;
        if (socketIdByQID(local_q) != socketIdByTID(tId))
          local_q = rand_heap();
        return extract_min(heap_i, i_ind);
      }
      unlock_heap(i_ind);
    }
    auto local = get_heap_ptr(nQ);
    if (!local->empty()) {
      auto minVal = local->extractMin();
      if (!local->heap.empty()) {
        auto rQ = lockRandomQ();
        auto heap = get_heap_ptr(rQ);
        for (size_t i = 0; i < 4 && !local->empty(); i++) {
          heap->push(local->extractMin());
        }
        heap->updateMin();
        heap->unlock();
      }
      local->updateMin();
      return minVal;
    }
    if (socketIdByQID(local_q) != socketIdByTID(tId))
      local_q = rand_heap();
    return result;
  }
};

GALOIS_WLCOMPILECHECK(MultiQueueLocalProbNuma)

} // namespace WorkList
} // namespace Galois

#endif // MQ_LOCAL_PROB_NUMA
