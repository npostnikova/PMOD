#ifndef MQ_LOCAL_LOCAL_NUMA
#define MQ_LOCAL_LOCAL_NUMA

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
 * `push` and `pop`.
 *
 * Provides efficient pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam PushSize Number of elements to push onto one queue.
 * @tparam PopSize Number of elements popped from one queue.
 * @tparam C parameter for queues number
 * @tparam Prior Type of T's priority. Need to support < operator.
 * @tparam Concurrent if the implementation should be concurrent
 */
template<typename T,
         typename Comparer,
         size_t PushSize,
         size_t PopSize,
         size_t C,
         size_t LOCAL_NUMA_W,
         typename Prior = unsigned long,
         bool Concurrent = true>
class MultiQueueLocalLocalNuma {
private:
  typedef T value_t;
  typedef HeapWithLock<T, Comparer, Prior, 8> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const size_t nQ;
  //! Local buffers for pop.
  std::unique_ptr<Runtime::LL::CacheLineStorage<std::vector<value_t>>[]> popBuffer;
  //! Local buffers for push
  std::unique_ptr<Runtime::LL::CacheLineStorage<std::vector<value_t>>[]> pushBuffer;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x =
        std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
    uint32_t local_x = x;
    local_x ^= local_x << 13;
    local_x ^= local_x >> 17;
    local_x ^= local_x << 5;
    x = local_x;
    return local_x;
  }

#include "NUMA.h"

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();

    auto result = heap->extractMin();
    auto& buffer = popBuffer[tId].data;

    for (size_t i = 0; i < PopSize - 1 && !heap->empty(); i++) {
      buffer.push_back(heap->extractMin());
    }
    std::reverse(buffer.begin(), buffer.end());
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

public:
  MultiQueueLocalLocalNuma() : nT(Galois::getActiveThreads()), nQ(C * nT) {
    // Setting dummy element of the heap
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Prior));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    popBuffer = std::make_unique<Runtime::LL::CacheLineStorage<std::vector<value_t >>[]>(nT);
    pushBuffer = std::make_unique<Runtime::LL::CacheLineStorage<std::vector<value_t >>[]>(nT);
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueLocalLocalNuma<T, Comparer, PushSize, PopSize, C, LOCAL_NUMA_W, Prior, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueLocalLocalNuma<_T, Comparer, PushSize, PopSize, C, LOCAL_NUMA_W, Prior, Concurrent> type;
  };
  
  size_t lockRandomQ() {
    auto r = rand_heap();
    while (!heaps[r].data.try_lock()) {
      r = rand_heap();
    }
    return r;
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

  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();

    // Retrieve an element from the buffer
    std::vector<value_type>& buffer = popBuffer[tId].data;
    if (!buffer.empty()) {
      auto ret = buffer.back();
      buffer.pop_back();
      return ret;
    }

    static const size_t ATTEMPTS = 4;

    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

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
    // Filling pop buffer from push buffer
    auto& pushB = pushBuffer[tId].data;
    auto& popB = popBuffer[tId].data;
    if (pushB.empty()) return result;
    std::sort(pushB.begin(), pushB.end(), [](const T& e1, const T& e2) { return e1.prior() > e2.prior(); });
    auto resultValue = pushB.back();
    pushB.pop_back();
    while (!pushB.empty() && popB.size() < PopSize) {
      popB.push_back(pushB.back());
      pushB.pop_back();
    }
    return resultValue;
  }
};

GALOIS_WLCOMPILECHECK(MultiQueueLocalLocalNuma)

} // namespace WorkList
} // namespace Galois

#endif // MQ_LOCAL_LOCAL_NUMA
