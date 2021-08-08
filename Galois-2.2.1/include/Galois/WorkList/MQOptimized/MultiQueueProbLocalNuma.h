#ifndef MQ_PROB_LOCAL_NUMA
#define MQ_PROB_LOCAL_NUMA

#include <atomic>
#include <memory>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <fstream>
#include <numa.h>
#include <thread>
#include <random>
#include <iostream>
#include "HeapWithLock.h"
#include "NUMA.h"

namespace Galois {
namespace WorkList {

/**
 * MultiQueue optimized variant. Uses temporal locality idea for
 * `push` and task batching for `pop`.
 *
 * Provides efficient pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam ChangeQPush Changes the queue for push with 1 / ChangeQPush probability
 * @tparam PopSize Number of elements popped from one queue.
 * @tparam C parameter for queues number
 * @tparam Prior Type of T's priority. Need to support < operator.
 * @tparam Concurrent if the implementation should be concurrent
 */
template<typename T,
         typename Comparer,
         size_t ChangeQPush,
         size_t PopSize,
         size_t C,
         size_t LOCAL_NUMA_W,
         typename Prior = unsigned long,
         bool Concurrent = true>
class MultiQueueProbLocalNuma {
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

  static const size_t OTHER_W = 1;
  static const size_t socketSize = SOCKET_SIZE;
  const size_t node1CntVal = node1Cnt();
  const size_t node2CntVal = node2Cnt();

  size_t node1Cnt() {
    size_t res = 0;
    if (nT > socketSize) {
      res += socketSize;
      if (socketSize * 2 < nT) {
        res += std::min(nT, socketSize * 3) - socketSize * 2;
      }
      return res;
    } else {
      return nT;
    }
  }

  size_t node2Cnt() {
    return nT - node1Cnt();
  }

  bool is1Node(size_t tId) {
    return tId < socketSize || (tId >= socketSize * 2 && tId < socketSize * 3);
  }

  bool is2Node(size_t tId) {
    return !is1Node(tId);
  }

  size_t socketIdByTID(size_t tId) {
    return is1Node(tId) ? 1 : 2; // tODO
  }

  size_t socketIdByQID(size_t qId) {
    if (qId < socketSize * C)
      return 1;
    if (qId < 2 * socketSize * C)
      return 2;
    if (qId < 3 * socketSize * C)
      return 1;
    return 2;
  }


  size_t map1Node(size_t qId) {
    if (qId < socketSize * C) {
      return qId;
    }
    return qId + socketSize * C;
  }

  size_t map2Node(size_t qId) {
    if (qId < socketSize * C) {
      return qId + socketSize * C;
    }
    return qId + socketSize * 2 * C;
  }

  inline size_t rand_heap() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();

    size_t isFirst = is1Node(tId);
    size_t localCnt = isFirst ? node1CntVal : node2CntVal;
    size_t otherCnt = nT - localCnt;
    const size_t Q = localCnt * LOCAL_NUMA_W * C + otherCnt * OTHER_W * C;
    const size_t r = random() % Q;
    if (r < localCnt * LOCAL_NUMA_W * C) {
      // we are stealing from our node
      auto qId = r / LOCAL_NUMA_W;
      return isFirst ? map1Node(qId) : map2Node(qId);
    } else {
      auto qId = (r - localCnt * LOCAL_NUMA_W * C) / OTHER_W;
      return isFirst ? map2Node(qId) : map1Node(qId);
    }
  }

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

  //! Number of elements to push onto one queue.
  //! At least one.
  size_t randomBatchSize(size_t limit) {
    for (size_t i = 0; i < limit; i++) {
      if (random() % ChangeQPush == 0) {
        // Need to change the queue
        return i + 1;
      }
    }
    return limit;
  }

public:
  MultiQueueProbLocalNuma() : nT(Galois::getActiveThreads()), nQ(C * nT) {
    // Setting dummy element of the heap
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Prior));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    popBuffer = std::make_unique<Runtime::LL::CacheLineStorage<std::vector<value_t >>[]>(nT);
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueProbLocalNuma<T, Comparer, ChangeQPush, PopSize, C, LOCAL_NUMA_W, Prior, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueProbLocalNuma<_T, Comparer, ChangeQPush, PopSize, C, LOCAL_NUMA_W, Prior, Concurrent> type;
  };

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    if (b == e) return 0;

    static thread_local size_t local_q = rand_heap();

    unsigned int pushNumber = 0;
    Heap* heap = nullptr;

    ptrdiff_t elementsLeft = std::distance(b, e);

    while (b != e) {
      auto batchSize = randomBatchSize(elementsLeft);
      heap = &heaps[local_q].data;

      while (!heap->try_lock()) {
        local_q = rand_heap();
        heap = &heaps[local_q].data;
      }

      for (size_t i = 0; i < batchSize; i++) {
        heap->push(*b++);
        pushNumber++;
        elementsLeft--;
      }
      heap->updateMin();
      heap->unlock();
      if (elementsLeft > 0) {
        local_q = rand_heap();
      }
    }
    if (socketIdByQID(local_q) != socketIdByTID(tId))
      local_q = rand_heap();
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
          i_ind = j_ind;
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
    return result;
  }
};

GALOIS_WLCOMPILECHECK(MultiQueueProbLocalNuma)

} // namespace WorkList
} // namespace Galois

#endif // MQ_PROB_LOCAL_NUMA
