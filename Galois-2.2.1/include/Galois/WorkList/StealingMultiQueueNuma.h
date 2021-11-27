#ifndef GALOIS_SMQ_NUMA_H
#define GALOIS_SMQ_NUMA_H

#include <atomic>
#include <cstdlib>
#include <vector>

#include "StealingQueue.h"
#include "StealingMultiQueue.h"


namespace Galois {
namespace WorkList {


template<typename T,
typename Comparer,
size_t StealProb,
size_t StealBatchSize,
size_t LOCAL_NUMA_W,
bool Concurrent = true
>
class StealingMultiQueueNuma {
private:
  typedef HeapWithStealBuffer<T, Comparer, StealBatchSize, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]> stealBuffers;
  Comparer compare;
  const size_t nQ;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x =
        std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  }
  
  const size_t C = 1;
  const size_t nT = nQ;
#include "MQOptimized/NUMA.h"

  //! Tries to steal from a random queue.
  //! Repeats if failed because of a race.
  Galois::optional<T> trySteal() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    T localMin = heaps[tId].data.getMinWriter();
    bool nextIterNeeded = true;
    while (nextIterNeeded) {
      auto randId = rand_heap();
      if (randId == tId) continue;
      nextIterNeeded = false;
      Heap *randH = &heaps[randId].data;
      auto randMin = randH->getBufferMin(nextIterNeeded);
      if (randH->isDummy(randMin)) {
        // Nothing to steal.
        continue;
      }
      if (Heap::isDummy(localMin) || compare(localMin, randMin)) {
        auto stolen = randH->trySteal(nextIterNeeded);
        if (stolen.is_initialized()) {
          auto &buffer = stealBuffers[tId].data;
          auto elements = stolen.get();
          for (size_t i = 1; i < elements.size() &&
                             !Heap::isDummy(elements[i]); i++) {
            buffer.push_back(elements[i]);
          }
          std::reverse(buffer.begin(), buffer.end());
          return elements[0];
        }
      }
    }
    return Galois::optional<T>();
  }

public:
  StealingMultiQueueNuma() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);
  }

  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef StealingMultiQueueNuma<T, Comparer, StealProb, StealBatchSize, LOCAL_NUMA_W, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueueNuma<_T, Comparer, StealProb, StealBatchSize, LOCAL_NUMA_W, Concurrent> type;
  };

  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  bool push(const T& key) {
    std::cerr << "Shouldn't be called" << std::endl;
    return false;
  }

  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    if (b == e) return 0;
    unsigned int pushedNum = 0;
    Heap* heap = &heaps[tId].data;
    while (b != e) {
      heap->pushLocally(*b++);
      pushedNum++;
    }
    heap->fillBufferIfStolen();
    return pushedNum;
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    auto& buffer = stealBuffers[tId].data;
    if (!buffer.empty()) {
      auto val = buffer.back();
      buffer.pop_back();
      heaps[tId].data.fillBufferIfStolen();
      return val;
    }
    Galois::optional<T> emptyResult;
    // rand == 0 -- try to steal
    // otherwise, pop locally
    if (nQ > 1 && random() % StealProb == 0) {
      Galois::optional<T> stolen = trySteal();
      if (stolen.is_initialized()) return stolen;
    }
    auto minVal = heaps[tId].data.extractMin();
    if (minVal.is_initialized()) return minVal;

    // Our heap is empty.
    return nQ == 1 ? emptyResult : trySteal();
  }
};

}
}

#endif //GALOIS_SMQ_NUMA_H
