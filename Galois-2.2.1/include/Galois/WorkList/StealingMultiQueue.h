#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

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
  typedef boost::heap::d_ary_heap<T, boost::heap::arity<D>,
          boost::heap::compare<Compare>> DAryHeap;
  Compare compare;
  // Represents a flag for empty cells.
  static T dummy;
  DAryHeap heap;

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
      return vals[getMinId(vals)];
    }
    // Somebody has stolen the elements.
    raceHappened = true;
    return dummy;
  }

  //! Tries to steal the elements from the stealing buffer.
  Galois::optional<std::array<T, STEAL_NUM>> trySteal(bool& raceHappened) {
    auto emptyRes = Galois::optional<std::array<T, STEAL_NUM>>();
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      // Already stolen.
      return emptyRes;
    }
    std::array<T, STEAL_NUM> buffer = stealBuffer;
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return buffer;
    }
    // Another thread got ahead.
    raceHappened = true;
    return emptyRes;
  }

  using stealing_array_t = std::array<T, STEAL_NUM>;

  void writeMin(std::array<T, STEAL_NUM> const& val) {
    stealBuffer = val;
    version.fetch_add(1, std::memory_order_acq_rel);
  }

  T extractOneMinLocally() {
    T result = heap.top();
    heap.pop();
    return result;
  }

  std::array<T, STEAL_NUM> extractMinLocally() {
    std::array<T, STEAL_NUM> res;
    for (size_t i = 0; i < STEAL_NUM; i++) {
      if (heap.empty()) {
        res[i] = dummy;
        continue;
      }
      res[i] = extractOneMinLocally();
    }
    return res;
  }

  // When current min is stolen
  T updateMin() {
    if (heap.empty()) return dummy;
    auto val = extractMinLocally();
    writeMin(val);
    return val[getMinId(val)];
  }

  size_t getMinId(stealing_array_t const& val) {
    size_t id = 0;
    for (size_t i = 1; i < STEAL_NUM; i++) {
      if ((compare(val[id], val[i]) && !isDummy(val[i]))|| isDummy(val[id]))
        id = i;
    }
    return id;
  }


  T extractMin() {
    bool useless = false;
    if (heap.size() > 0) {
      auto stolen = trySteal(useless);
      if (!stolen.is_initialized()) {
        auto res = extractOneMinLocally();
        if (heap.size() > 0) {
          writeMin(extractMinLocally());
        }
        return res;
      } else {
        auto stolenVal = stolen.get();
        auto firstMinId = getMinId(stolenVal);
        auto extracted = extractOneMinLocally();
        if (compare(extracted, stolenVal[firstMinId])) {
          auto res = stolenVal[firstMinId];
          stolenVal[firstMinId] = extracted;
          writeMin(stolenVal);
          return res;
        }
        writeMin(stolenVal);
        return extracted;
      }
    } else {
      // No elements in the heap, just take min if we can
      auto stolen = trySteal(useless);
      if (!stolen.is_initialized()) {
        return dummy;
      }
      // todo it's not cool
      auto id = getMinId(stolen.get());
      for (size_t i = 0; i < STEAL_NUM; i++) {
        if (id == i) continue;
        if (!isDummy(stolen.get()[i])) {
          heap.push(stolen.get()[i]);
        }
      }
      return stolen.get()[id];
    }
  }

  template <typename Iter>
  int pushRange(Iter b, Iter e) {
    if (b == e)
      return 0;
    int npush = 0;

    while (b != e) {
      npush++;
      heap.push(*b++);
    }
    bool bl;
    auto curMin = getBufferMin(bl);
    if (isDummy(curMin) /**&& compare(curMin, heap[0])*/ && !heap.empty()) { // todo i don't want to do it now
      assert(getVersion() % 2 == 0); // should be stolen
      writeMin(extractMinLocally());
    }
    return npush;
  }

private:
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
         size_t StealProb = 8,
         size_t StealBatchSize = 8,
         bool Concurrent = true
>
class StealingMultiQueue {
private:
  typedef HeapWithStealBuffer<T, Comparer, StealBatchSize, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]> stealBuffers;
  Comparer compare;
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
    Heap *localH = &heaps[tId].data;

    bool again = true;
    while (again) {
      again = false;
      // we try to steal
      auto randId = (tId + 1 + (random() % (nQ - 1))) % nQ;
      Heap *randH = &heaps[randId].data;
      auto randMin = randH->getBufferMin(again);
      if (randH->isDummy(randMin)) {
        // steal is not successful
      } else {
        bool useless = false;
        auto localMin = localH->getBufferMin(useless);
        if (localH->isDummy(localMin)) {
          localMin = localH->updateMin();
        }
        if (randH->isDummy(localMin) || compare(localMin, randMin)) {
          auto stolen = randH->trySteal(again);
          if (stolen.is_initialized()) {
            auto &buffer = stealBuffers[tId].data;
            typename Heap::stealing_array_t vals = stolen.get();
            auto minId = localH->getMinId(vals);
            for (size_t i = 0; i < vals.size(); i++) {
              if (i == minId || localH->isDummy(vals[i])) continue;
              buffer.push_back(vals[i]);
            }
            std::sort(buffer.begin(), buffer.end(), [](T const &e1, T const &e2) { return e1.prior() > e2.prior(); });
            return vals[minId];
          }
        }
      }
    }
    return Galois::optional<T>();
  }

public:
  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);
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
  int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    Heap* heap = &heaps[tId].data;
    return heap->pushRange(b, e);
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); // todo bounds? can be changed?

    auto& buffer = stealBuffers[tId].data;
    if (!buffer.empty()) {
      auto val = buffer.back();
      buffer.pop_back();
      Heap& local = heaps[tId].data;
      bool useless = false;
      auto curMin = local.getBufferMin(useless);
      if (Heap::isDummy(curMin) /**&& compare(curMin, heap[0])*/ && !local.heap.empty()) { // todo i don't want to do it now
        local.writeMin(local.extractMinLocally());
      }
      return val;
    }
    Galois::optional<T> emptyResult;

    if (nQ > 1) {
      size_t change = random() % StealProb;
      if (change < 1) {
        auto el = trySteal();
        if (el.is_initialized()) return el;

      }
    }
    auto minVal = heaps[tId].data.extractMin();
    if (!heaps[tId].data.isDummy(minVal)) {
      return minVal;
    }
    // Our heap is empty
    return nQ == 1 ? emptyResult : trySteal();
  }
};

GALOIS_WLCOMPILECHECK(StealingMultiQueue)

}  // namespace WorkList
}  // namespace Galois

#endif //GALOIS_STEALINGMULTIQUEUE_H
