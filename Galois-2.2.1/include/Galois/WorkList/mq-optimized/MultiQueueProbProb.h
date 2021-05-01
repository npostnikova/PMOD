#ifndef MQ_PROB_PROB
#define MQ_PROB_PROB

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
#include "../Heap.h"
#include "../WorkListHelpers.h"

namespace Galois {
namespace WorkList {

/**
 * Lockable heap structure.
 *
 * @tparam T type of stored elements
 * @tparam Comparer callable defining ordering for objects of type `T`.
 * Its `operator()` returns `true` iff the first argument should follow the second one.
 */
template <typename  T,
          typename Comparer,
          typename Prior = unsigned long,
          size_t D = 4>
struct HeapWithLock {
  DAryHeap<T, Comparer, 4> heap;
  // Dummy element for setting min when the heap is empty
  static Prior dummy;

  HeapWithLock() : min(dummy) {}

  //! Non-blocking lock.
  inline bool try_lock() {
    return _lock.try_lock();
  }

  //! Blocking lock.
  inline void lock() {
    _lock.lock();
  }

  //! Unlocks the queue.
  inline void unlock() {
    _lock.unlock();
  }

  Prior getMin() {
    return min.load(std::memory_order_acquire);
  }

  static bool isMinDummy(Prior const& value) {
    return value == dummy;
  }

  void updateMin() {
    return min.store(
      heap.size() > 0 ? heap.min().prior() : dummy,
      std::memory_order_release
    );
  }

private:
  Runtime::LL::PaddedLock<true> _lock;
  std::atomic<Prior> min;
};

template<typename T,
         typename Compare,
         typename Prior,
         size_t D>
Prior HeapWithLock<T, Compare, Prior, D>::dummy;


/**
 * MultiQueue optimized variant. Uses temporal locality idea for
 * `push` and `pop`, which performs a sequence of operations on 
 * one queue, changing it with some probability.
 * 
 * Provides efficient pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam ChangeQPush Changes the queue for push with 1 / ChangeQPush probability
 * @tparam ChangeQPop Changes the queue for pop with 1 / ChangeQPop probability
 * @tparam C parameter for queues number
 * @tparam Prior Type of T's priority. Need to support < operator.
 * @tparam Concurrent if the implementation should be concurrent
 */
template<typename T,
         typename Comparer,
         size_t ChangeQPush,
         size_t ChangeQPop,
         size_t C = 2,
         typename Prior = unsigned long,
         bool Concurrent = true>
class MultiQueueProbProb {
private:
  typedef T value_t;
  typedef HeapWithLock<T, Comparer, Prior, 4> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;

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

  inline size_t rand_heap() {
    return random() % nQ;
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap) {
    auto result = heap->heap.extractMin();
    heap->updateMin();
    heap->unlock();
    return result;
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

public:
  MultiQueueProbProb() : nT(Galois::getActiveThreads()), nQ(C * nT) {
    // Setting dummy element of the heap
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Prior));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueProbProb<T, Comparer, ChangeQPush, ChangeQPop, C, Prior, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueProbProb<_T, Comparer, ChangeQPush, ChangeQPop, C, Prior, Concurrent> type;
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

  size_t numToPush(size_t limit) {
    // todo min is one as we trow a coin a least ones to get the local queue
    for (size_t i = 1; i < limit; i++) {
      if ((random() % ChangeQPush) < 1) {
        return i;
      }
    }
    return limit;
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    if (b == e) return 0;

    // local queue
    static thread_local size_t local_q = rand_heap();

    int npush = 0;
    Heap* heap = nullptr;

    int total = std::distance(b, e);

    while (b != e) {
      auto batchSize = numToPush(total);
      heap = &heaps[local_q].data;

      while (!heap->try_lock()) {
        local_q = rand_heap();
        heap = &heaps[local_q].data;
      }

      for (size_t i = 0; i < batchSize; i++) {
        heap->heap.push(*b++);
        npush++;
        total--;
      }
      heap->updateMin();
      heap->unlock();
      if (total > 0) {
        local_q = rand_heap();
      }
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
    const size_t ATTEMPTS = 4;

    static thread_local size_t local_q = rand_heap();
    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

    size_t change = random() % ChangeQPop;

    if (change > 0) {
      heap_i = &heaps[local_q].data;
      if (heap_i->try_lock()) {
        if (!heap_i->heap.empty()) {
          return extract_min(heap_i);
        }
        heap_i->unlock();
      }
    }

    for (size_t i = 0; i < ATTEMPTS; i++) {
      while (true) {
        i_ind = rand_heap();
        heap_i = &heaps[i_ind].data;

        j_ind = rand_heap();
        heap_j = &heaps[j_ind].data;

        if (i_ind == j_ind && nQ > 1)
          continue;
        if (isFirstLess(heap_j->getMin(), heap_i->getMin())) {
          i_ind = j_ind;
          heap_i = heap_j;
        }

        if (heap_i->try_lock())
          break;
      }

      if (!heap_i->heap.empty()) {
        local_q = i_ind;
        return extract_min(heap_i);
      } else {
        heap_i->unlock();
      }
    }
  }
};

GALOIS_WLCOMPILECHECK(MultiQueueProbProb)

} // namespace WorkList
} // namespace Galois

#endif // MQ_PROB_PROB
