#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

#include "StealingQueue.h"
#include "Heap.h"

namespace Galois {
namespace WorkList {

/**
 * Class-helper, consists of a sequential heap and
 * a stealing buffer.
 * 
 * @tparam T Type of the elements.
 * @tparam Compare Elements comparator.
 * @tparam STEAL_NUM Number of elements to steal at once.
 * @tparam D Arity of the heap.
 */
template<typename T,
         typename Compare,
         size_t STEAL_NUM,
         size_t D = 4>
struct HeapWithStealBuffer {
  static Compare compare;
  // Represents a flag for empty cells.
  static T dummy;
  DAryHeap<T, Compare, D> heap;

  HeapWithStealBuffer(): version(0) {
    stealBuffer.fill(dummy);
  }

  //! Checks whether the element is "null".
  static bool isDummy(T const& element) {
    return element == dummy;
  }

  //! Gets current version of the stealing buffer.
  size_t getVersion() {
    return version.load(std::memory_order_acquire);
  }

  //! Checks whether elements in the buffer are stolen.
  size_t isBufferStolen() {
    return getVersion() % 2 == 0;
  }

  //! Get min among elements that can be stolen.
  //! Sets a flag to true, if operation failed because of a race.
  T getBufferMin(bool& raceHappened) {
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      return dummy;
    }
    std::array<T, STEAL_NUM> vals = stealBuffer;
    auto v2 = getVersion();
    if (v1 == v2) {
      return vals[0];
    }
    // Somebody has stolen the elements
    raceHappened = true;
    return dummy;
  }

  //! Returns min element from the buffer, updating the buffer if empty.
  //! Can be called only by the thread-owner.
  T getMinWriter() {
    auto v1 = getVersion();
    if (v1 % 2 != 0) {
      std::array<T, STEAL_NUM> vals = stealBuffer;
      auto v2 = getVersion();
      if (v1 == v2) {
        return vals[0];
      }
    }
    return fillBuffer();
  }

  //! Tries to steal the elements from the stealing buffer.
  Galois::optional<std::array<T, STEAL_NUM>> trySteal(bool& raceHappened) {
    auto emptyRes = Galois::optional<std::array<T, STEAL_NUM>>();
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      // Already stolen
      return emptyRes;
    }
    std::array<T, STEAL_NUM> buffer = stealBuffer;
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return buffer;
    }
    // Another thread got ahead
    raceHappened = true;
    return emptyRes;
  }

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBuffer() {
    if (heap.empty()) return dummy;
    std::array<T, STEAL_NUM> elements;
    elements.fill(dummy);
    for (size_t i = 0; i < STEAL_NUM && !heap.empty(); i++) {
      elements[i] = heap.extractMin();
    }
    stealBuffer = elements;
    version.fetch_add(1, std::memory_order_acq_rel);
    return elements[0];
  }

  //! Extract min from the structure: both the buffer and the heap
  //! are considered. Called from the owner-thread.
  Galois::optional<T> extractMin() {
    if (heap.empty()) {
      // Only check the steal buffer
      return tryStealLocally();
    }
    bool raceFlag = false; // useless now
    auto bufferMin = getBufferMin(raceFlag);
    if (!isDummy(bufferMin) && compare(heap.min(), bufferMin)) {
      auto stolen = tryStealLocally();
      if (stolen.is_initialized()) {
        fillBuffer();
        return stolen;
      }
    }
    auto localMin = heap.extractMin();
    if (isDummy(bufferMin)) fillBuffer();
    return localMin;
  }

private:
  //! Tries to steal elements from local buffer.
  //! Return minimum among stolen elements.
  Galois::optional<T> tryStealLocally() {
    bool raceFlag = false; // useless now
    auto stolen = trySteal(raceFlag);
    if (stolen.is_initialized()) {
      auto elements = stolen.get();
      for (size_t i = 1; i < STEAL_NUM; i++) {
        if (!isDummy(elements[i])) heap.push(elements[i]);
      }
      return elements[0];
    }
    return Galois::optional<T>();
  }

  std::array<T, STEAL_NUM> stealBuffer;
  // Represents epoch & stolen flag
  // version mod 2 = 0  -- element is stolen
  // version mod 2 = 1  -- can steal
  std::atomic<size_t> version;
};

template<typename T,
         typename Compare,
         size_t STEAL_NUM,
         size_t D>
T HeapWithStealBuffer<T, Compare, STEAL_NUM, D>::dummy;

template<typename T,
         typename Comparer,
         size_t StealProb,
         size_t StealBatchSize = 8,
         bool Concurrent = true
>
class StealingMultiQueue {
private:
  typedef HeapWithStealBuffer<T, Comparer, StealBatchSize, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  static Comparer compare;
  const size_t nQ;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random() + 1;
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

  size_t rand_heap() {
    return random() % nQ;
  }

  //! Tries to steal from a random queue.
  //! Repeats if failed because of a race.
  Galois::optional<T> trySteal() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    T localMin = heaps[tId].data.getMinWriter();
    bool nextIterNeeded = true;
    while (nextIterNeeded) {
      nextIterNeeded = false;
      auto randId = rand_heap();
      if (randId == tId) continue;
      Heap* randH = &heaps[randId].data;
      auto randMin = randH->getBufferMin(nextIterNeeded);
      if (randH->isDummy(randMin)) {
        // Nothing to steal
        continue;
      }
      if (Heap::isDummy(localMin) || compare(localMin, randMin)) {
        auto stolen = randH->trySteal(nextIterNeeded);
        if (stolen.is_initialized()) {
          auto elements = stolen.get();
          for (size_t i = 1; i < StealBatchSize; i++) {
            if (!Heap::isDummy(elements[i]))
              heaps[tId].data.heap.push(elements[i]);
          }
          return elements[0];
        }
      }
    }
    return Galois::optional<T>();
  }

public:
  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
  }

  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef StealingMultiQueue<T, Comparer, StealProb, StealBatchSize, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueue<_T, Comparer, StealProb, StealBatchSize, Concurrent> type;
  };

  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    if (b == e) return 0;
    unsigned int pushedNum = 0;
    Heap* heap = &heaps[tId].data;
    while (b != e) {
      heap->heap.push(*b++);
      pushedNum++;
    }
    if (heap->isBufferStolen()) heap->fillBuffer();
    return pushedNum;
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    Galois::optional<T> emptyResult;

    // rand == 0 -- try to steal
    // otherwise, pop locally
    if (nQ > 1 && random() % StealProb == 0) {
        Galois::optional<T> stolen = trySteal();
        if (stolen.is_initialized()) return stolen;
    }
    auto minVal = heaps[tId].data.extractMin();
    if (minVal.is_initialized()) return minVal;

    // Our heap is empty
    return nQ == 1 ? emptyResult : trySteal();
  }
};

GALOIS_WLCOMPILECHECK(StealingMultiQueue)

} // namespace WorkList
} // namespace Galois

#endif //GALOIS_STEALINGMULTIQUEUE_H

