#ifndef MQ_LOCAL_PROB
#define MQ_LOCAL_PROB

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
         size_t C = 2,
         typename Prior = unsigned long,
         bool Concurrent = true>
class MultiQueueLocalProb {
private:
  typedef T value_t;
  typedef HeapWithLock<T, Comparer, Prior, 8> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;
  //! Local buffers for push
  std::unique_ptr<Runtime::LL::CacheLineStorage<std::vector<value_t>>[]> pushBuffer;

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

  //! Extracts minimum from the locked
  Galois::optional<value_t> extract_min(Heap* heap) {
    auto result = heap->extractMin();
    heap->updateMin();
    heap->unlock();
    return result;
  }

  //! Checks whether the first priority value is less.
  bool isFirstLess(Prior const& v1, Prior const& v2) {
    if (Heap::isMinDummy(v1)) {
      return false;
    }
    if (Heap::isMinDummy(v2)) {
      return true;
    }
    return v1 < v2;
  }

  //! Push an element onto the local buffer, flushing
  //! the buffer when the capacity is exceeded.
  void pushLocally(T val) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    auto& buffer = pushBuffer[tId].data;
    buffer.push_back(val);
    if (buffer.size() >= PushSize) {
      auto id = lockRandomQ();
      auto heap = &heaps[id].data;
      while (!buffer.empty()) {
        heap->push(buffer.back());
        buffer.pop_back();
      }
      heap->updateMin();
      heap->unlock();
    }
  }

  //! Locks a random queue and returns its id.
  size_t lockRandomQ() {
    auto r = rand_heap();
    while (!heaps[r].data.try_lock()) {
      r = rand_heap();
    }
    return r;
  }
public:
  MultiQueueLocalProb() : nT(Galois::getActiveThreads()), nQ(C * nT) {
    // Setting dummy element of the heap
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Prior));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    pushBuffer = std::make_unique<Runtime::LL::CacheLineStorage<std::vector<value_t >>[]>(nT);
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueLocalProb<T, Comparer, PushSize, ChangeQPop, C, Prior, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueLocalProb<_T, Comparer, PushSize, ChangeQPop, C, Prior, Concurrent> type;
  };

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

  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static const size_t ATTEMPTS = 4;
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
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
      heap_i = &heaps[local_q].data;
      if (heap_i->try_lock()) {
        if (!heap_i->empty())
          return extract_min(heap_i);
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
          heap_i = heap_j;
        }
        if (heap_i->try_lock())
          break;
      }
      if (!heap_i->empty()) {
        return extract_min(heap_i);
      }
      heap_i->unlock();
    }
    // Retrieving an element from the push buffer
    auto& pushB = pushBuffer[tId].data;
    if (pushB.empty()) return result;
    std::sort(pushB.begin(), pushB.end(), [](const T& e1, const T& e2) { return e1.prior() > e2.prior(); });
    auto resultValue = pushB.back();
    pushB.pop_back();
    return resultValue;
  }
};

GALOIS_WLCOMPILECHECK(MultiQueueLocalProb)

} // namespace WorkList
} // namespace Galois

#endif // MQ_LOCAL_PROB
