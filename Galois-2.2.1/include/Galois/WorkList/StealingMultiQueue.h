#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

#include "StealingQueue.h"


namespace Galois {
namespace WorkList {


template<typename T = int,
typename Compare = std::greater<int>,
size_t D = 4,
size_t STEAL_NUM = 8>
struct StealDAryHeapHaate {
  typedef size_t index_t;

  std::vector<T> heap;

  std::array<T, STEAL_NUM> mins;
  // version mod 2 = 0  -- element is stolen
  // version mod 2 = 1  -- can steal
  std::atomic<size_t> version;

  static T usedT;
  Compare cmp;

  StealDAryHeapHaate(): version(0) {
    for (size_t i = 0; i < STEAL_NUM; i++) {
      mins[i] = usedT;
    }
  }

  static bool isUsed(T const& element) {
    return element == usedT;
  }

  size_t getVersion() {
    return version.load(std::memory_order_acquire);
  }

  T getMin(bool& failedBecauseOthers) {
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      return usedT;
    }
    std::array<T, STEAL_NUM> vals = mins;
    auto v2 = getVersion();
    if (v1 == v2) {
      return vals[getMinId(vals)];
    }
    failedBecauseOthers = true;
    return usedT; // is the version was changed, somebody stole the element
  }

  Galois::optional<std::array<T, STEAL_NUM>> steal(bool& failedBecauseOthers) {
    auto res = Galois::optional<std::array<T, STEAL_NUM>>();
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      return res;
    }
    std::array<T, STEAL_NUM> val = mins;
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return val;
    }
    failedBecauseOthers = true;
    return res;
  }

  using stealing_array_t = std::array<T, STEAL_NUM>;

  void writeMin(std::array<T, STEAL_NUM> const& val) {
    mins = val;
    version.fetch_add(1, std::memory_order_acq_rel);
  }


  T extractOneMinLocally() {
    auto res = heap[0];
    heap[0] = heap.back();
    heap.pop_back();
    if (heap.size() > 0) {
      sift_down(0);
    }
    return res;
  }

  std::array<T, STEAL_NUM> extractMinLocally() {
    std::array<T, STEAL_NUM> res;
    for (size_t i = 0; i < STEAL_NUM; i++) {
      if (heap.empty()) {
        res[i] = usedT;
        continue;
      }
      res[i] = heap[0];
      heap[0] = heap.back();
      heap.pop_back();
      if (heap.size() > 0) {
        sift_down(0);
      }
    }
    return res;
  }

  // When current min is stolen
  T updateMin() {
    if (heap.empty()) return usedT;
    auto val = extractMinLocally();
    writeMin(val);
    return val[getMinId(val)];
  }

  size_t getMinId(stealing_array_t const& val) {
    size_t id = 0;
    for (size_t i = 1; i < STEAL_NUM; i++) {
      if ((cmp(val[id], val[i]) && !isUsed(val[i]))|| isUsed(val[id]))
        id = i;
    }
    return id;
  }



  T extractMin() {
    bool useless = false;
    if (heap.size() > 0) {
      auto stolen = steal(useless);
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
        if (cmp(extracted, stolenVal[firstMinId])) {
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
      auto stolen = steal(useless);
      if (!stolen.is_initialized()) {
        return usedT;
      }
      // todo it's not cool
      auto id = getMinId(stolen.get());
      for (size_t i = 0; i < STEAL_NUM; i++) {
        if (id == i) continue;
        if (!isUsed(stolen.get()[i])) {
          pushHelper(stolen.get()[i]);
        }
      }
      return stolen.get()[id];
    }
  }

  void pushHelper(T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(index);
  }

  template <typename Iter>
  int pushRange(Iter b, Iter e) {
    if (b == e)
      return 0;

    int npush = 0;

    while (b != e) {
      npush++;
      pushHelper(*b++);
    }

    bool bl;
    auto curMin = getMin(bl);
    if (isUsed(curMin) /**&& cmp(curMin, heap[0])*/ && !heap.empty()) { // todo i don't want to do it now
      assert(getVersion() % 2 == 0); // should be stolen
      writeMin(extractMinLocally());
    }
    return npush;
  }


  void build() {
    // D * index + 1 is the first child
    for (size_t i = 0; is_valid_index(D * i + 1); i++) {
      sift_down(i);
    }
  }

private:

  void swap(index_t  i, index_t j) {
    T t = heap[i];
    heap[i] = heap[j];
    heap[j] = t;
  }

  //! Check whether the index of the root passed.
  bool is_root(index_t index) {
    return index == 0;
  }

  //! Check whether the index is not out of bounds.
  bool is_valid_index(index_t index) {
    return index >= 0 && index < heap.size();
  }

  //! Get index of the parent.
  Galois::optional<index_t> get_parent(index_t index) {
    if (!is_root(index) && is_valid_index(index)) {
      return (index - 1) / D;
    }
    return Galois::optional<index_t>();
  }

  //! Get index of the smallest (due `Comparator`) child.
  Galois::optional<index_t> get_smallest_child(index_t index) {
    if (!is_valid_index(D * index + 1)) {
      return Galois::optional<index_t>();
    }
    index_t smallest = D * index + 1;
    for (size_t k = 2; k <= D; k++) {
      index_t k_child = D * index + k;
      if (!is_valid_index(k_child))
        break;
      if (cmp(heap[smallest], heap[k_child]))
        smallest = k_child;
    }
    return smallest;
  }

  //! Sift down without decrease key info update.
  void sift_down(index_t index) {
    auto smallest_child = get_smallest_child(index);
    while (smallest_child && cmp(heap[index], heap[smallest_child.get()])) {
      swap(index, smallest_child.get());
      index = smallest_child.get();
      smallest_child = get_smallest_child(index);
    }
  }

  //! Sift up the element with provided index.
  index_t sift_up(index_t index) {
    Galois::optional<index_t> parent = get_parent(index);

    while (parent && cmp(heap[parent.get()], heap[index])) {
      swap(index, parent.get());
      index = parent.get();
      parent = get_parent(index);
    }
    return index;
  }

  void push_back(T const& val) {
    heap.push_back(val);
  }
};



template<typename T,
typename Comparer,
size_t StealProb = 8,
size_t StealBatchSize = 8,
bool Concurrent = true,
typename Container = StealDAryHeapHaate<T, Comparer, 4, StealBatchSize>
>
class StealingMultiQueue {
private:
  typedef Container Heap;
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

  inline size_t rand_heap() {
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
      auto randMin = randH->getMin(again);
      if (randH->isUsed(randMin)) {
        // steal is not successfull
      } else {
        bool useless = false;
        auto localMin = localH->getMin(useless);
        if (localH->isUsed(localMin)) {
          localMin = localH->updateMin();
        }
        if (randH->isUsed(localMin) || compare(localMin, randMin)) {
          auto stolen = randH->steal(again);
          if (stolen.is_initialized()) {
            auto &buffer = stealBuffers[tId].data;
            typename Heap::stealing_array_t vals = stolen.get();
            auto minId = localH->getMinId(vals);
            for (size_t i = 0; i < vals.size(); i++) {
              if (i == minId || localH->isUsed(vals[i])) continue;
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


  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

public:
  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);
    std::cout << "Queues: " << nQ << std::endl;
  }

  void deleteStatistic(Galois::Statistic*& st) {
    if (st != nullptr) {
      delete st;
      st = nullptr;
    }
  }


  uint64_t getStatVal(Galois::Statistic* value) {
    uint64_t stat = 0;
    for (unsigned x = 0; x < Galois::Runtime::activeThreads; ++x)
      stat += value->getValue(x);
    return stat;
  }

  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef StealingMultiQueue<T, Comparer, StealProb, StealBatchSize, _concurrent, Container> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueue<_T, Comparer, StealProb, StealBatchSize, Concurrent, Container> type;
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
  int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); // todo bounds? can be changed?
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
      auto curMin = local.getMin(useless);
      if (Heap::isUsed(curMin) /**&& cmp(curMin, heap[0])*/ && !local.heap.empty()) { // todo i don't want to do it now
        local.writeMin(local.extractMinLocally());
      }
      return val;
    }
    Galois::optional<T> result;

    if (nQ > 1) {
      size_t change = random() % StealProb;
      if (change < 1) {
        auto el = trySteal();
        if (el.is_initialized()) return el;

      }
    }
    auto minVal = heaps[tId].data.extractMin();
    if (!heaps[tId].data.isUsed(minVal)) {
      return minVal;
    }
    // our heap is empty
    if (nQ == 1)  { // nobody to steal from
      return result;
    }

    auto el = trySteal();
    if (el.is_initialized()) return el;
    return result;
  }
};


template<typename T,
typename Compare,
size_t D,
size_t STEAL_NUM>
T StealDAryHeapHaate<T, Compare, D, STEAL_NUM>::usedT;

}
}

#endif //GALOIS_STEALINGMULTIQUEUE_H
