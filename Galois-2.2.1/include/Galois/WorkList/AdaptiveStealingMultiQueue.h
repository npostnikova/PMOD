#ifndef GALOIS_ADAPTIVE_SMQ_H
#define GALOIS_ADAPTIVE_SMQ_H

#include <atomic>
#include <cstdlib>
#include <vector>
#include <filesystem>

#include "StealingQueue.h"
#include "Heap.h"

namespace Galois {
namespace WorkList {

template<typename T = int,
typename Compare = std::greater<int>,
size_t D = 4,
size_t STEAL_NUM = 8>
struct StealDAryHeapRrr {

//  const static size_t STEAL_NUM = 8;

  typedef size_t index_t;

  std::vector<T> heap;

  std::array<T, STEAL_NUM> mins;
  // version mod 2 = 0  -- element is stolen
  // version mod 2 = 1  -- can steal
  std::atomic<size_t> version;

  static T usedT;
  size_t minsBegin = 0;

//	std::atomic<bool> empty = {true };
  Compare cmp;

  size_t qInd;

  void set_id(size_t id) {
    qInd = id;
  }

  StealDAryHeapRrr(): version(0) {
    for (size_t i = 0; i < STEAL_NUM; i++) {
      mins[i] = usedT;
    }
    //memset(reinterpret_cast<void*>(&usedT), 0xff, sizeof(usedT));
    //min.store(usedT, std::memory_order_release);
  }

  static bool isUsed(T const& element) {
    return element == usedT;
  }

  size_t getVersion() {
    return version.load(std::memory_order_acquire);
  }

  T getMin(bool& failedBecauseOthers) {
//    while (true) {
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
//    }
  }

  Galois::optional<std::array<T, STEAL_NUM>> steal(bool& failedBecauseOthers) {
//    while (true) { // todo do i want to use while(true) here?
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
//    }
  }

  using stealing_array_t = std::array<T, STEAL_NUM>;

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBuffer(size_t elementsNumber) {
    if (heap.empty()) return usedT;
    static thread_local size_t prevFill = STEAL_NUM;
    size_t curFill = 0;
    for (; curFill < elementsNumber && !heap.empty(); curFill++) {
      mins[curFill] = extractOneMinLocally();
    }
    for (size_t j = curFill; j < prevFill; j++) {
      mins[j] = usedT;
    }
    prevFill = curFill;
    version.fetch_add(1, std::memory_order_acq_rel);
    return mins[0];
  }

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBufferWithArray(std::array<T, STEAL_NUM> const & arr) {
    mins = arr;
    version.fetch_add(1, std::memory_order_acq_rel);
    return mins[getMinId(arr)];
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




  size_t getMinId(stealing_array_t const& val) {
    size_t id = 0;
    for (size_t i = 1; i < STEAL_NUM; i++) {
      if ((cmp(val[id], val[i]) && !isUsed(val[i]))|| isUsed(val[id]))
        id = i;
    }
    return id;
  }

//	template <typename Indexer>
//  T updateMin(Indexer const& indexer) {
//    if (heap.size() == 0) return usedT;
//    auto val = extractMinLocally(indexer);
//    set_position_min(indexer, val);
//    min.store(val, std::memory_order_release);
//    return min;
//  }

  template <typename Indexer>
  T updateMin(Indexer const& indexer) {
//    if (heap.size() == 0) return usedT;
//    auto val = extractMinLocally(indexer);
//    min.store(val, std::memory_order_release);
    return usedT; // todo
  }

  T extractMin(size_t size) {

    bool useless = false;
    if (heap.size() > 0) {
//      auto secondMin = extractMinLocally();
      auto stolen = steal(useless); // min.exchange(secondMin, std::memory_order_acq_rel);
//      writeMin(secondMin);
      if (!stolen.is_initialized()) {
        auto res = extractOneMinLocally();
        if (heap.size() > 0) {
          fillBuffer(size);
        }
        return res;
      } else {
        auto stolenVal = stolen.get();
        auto firstMinId = getMinId(stolenVal);
        auto extracted = extractOneMinLocally();
        if (cmp(extracted, stolenVal[firstMinId])) {
          auto res = stolenVal[firstMinId];
          stolenVal[firstMinId] = extracted;
          fillBufferWithArray(stolenVal);
          return res;
        }
        fillBufferWithArray(stolenVal);
        return extracted;
      }
    } else {
      // No elements in the heap, just take min if we can
      auto stolen = steal(useless);
      if (!stolen.is_initialized()) {
        return usedT;
      }
      // todo it's not cool
      auto els = stolen.get();
      auto id = getMinId(els);
      for (size_t i = 0; i < STEAL_NUM; i++) {
        if (id == i) continue;
        if (!isUsed(els[i])) {
          pushHelper(els[i]);
        }
      }
      return els[id];
    }
  }


  void pushHelper(T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(index);
  }

  template <typename Indexer>
  void pushHelper(Indexer const& indexer, T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(indexer, index);
  }

  //! Push the element.
  void push(T const& val) {
    throw "aaa why is it called";
//    auto curMin = getMin();
//
//    if (!isUsed(curMin) && cmp(curMin, val)) {
//      auto exchanged = min.exchange(val, std::memory_order_acq_rel);
//      if (isUsed(exchanged)) return;
//      else pushHelper(exchanged);
//    } else {
//      pushHelper(val);
//      if (isUsed(curMin)) {
//        min.store(extractMinLocally(), std::memory_order_release);
//      }
//    }
  }

  template <typename Iter>
  int pushRange(Iter b, Iter e, size_t size) {
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
//      auto stolen = steal(); // todo i suppose its stolen
      assert(getVersion() % 2 == 0); // should be stolen
      fillBuffer(size);
    }
    return npush;
  }

  size_t inOurQueue = 0;
  size_t inAnotherQueue = 0;
  size_t notInQeues = 0;

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

  template <typename Indexer>
  void sift_down(Indexer const& indexer, index_t index) {
    auto smallest_child = get_smallest_child(index);
    while (smallest_child && cmp(heap[index], heap[smallest_child.get()])) {
      swap(index, smallest_child.get());
      set_position(indexer, index);
      index = smallest_child.get();
      smallest_child = get_smallest_child(index);
    }
    set_position(indexer, index);
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

  template <typename Indexer>
  index_t sift_up(Indexer const& indexer, index_t index) {
    Galois::optional<index_t> parent = get_parent(index);

    while (parent && cmp(heap[parent.get()], heap[index])) {
      swap(index, parent.get());
      set_position(indexer, index);
      index = parent.get();
      parent = get_parent(index);
    }
    set_position(indexer, index);
    return index;
  }

  template <typename Indexer>
  void set_position(Indexer const& indexer, index_t new_pos) {
    indexer.set_pair(heap[new_pos], qInd, new_pos);
  }

//  template <typename Indexer>
//  void set_position_min(Indexer const& indexer, T const& val) {
//    indexer.set_pair(val, qInd, 0);
//  }


  void push_back(T const& val) {
    heap.push_back(val);
  }
};

template<typename T,
typename Compare,
size_t D,
size_t STEAL_NUM>
T StealDAryHeapRrr<T, Compare, D, STEAL_NUM>::usedT;



template<typename T,
typename Comparer,
bool Concurrent = true>
class AdaptiveStealingMultiQueue {
private:
  static const size_t STAT_BUFF_SIZE = 4;
  static const size_t STAT_PROB_SIZE = 10;
  
  static const size_t MIN_STEAL_SIZE = 16;
  static const size_t MAX_STEAL_SIZE = 16;
  static const size_t MIN_STEAL_PROB = 8;
  static const size_t MAX_STEAL_PROB = 8;
  typedef StealDAryHeapRrr<T, Comparer, 4, MAX_STEAL_SIZE> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]> stealBuffers;
  static Comparer compare;
  const size_t nQ;

  struct PerThread {
    T stolen[MAX_STEAL_SIZE];

    size_t stealProb = MIN_STEAL_PROB;
    size_t stealSize = MIN_STEAL_SIZE;

    // We wanted to steal
    size_t stealAttemptsNum = 0;
    // We tried to steal
    size_t otherMinLessNum = 0;
    size_t stealReportId = 0;

    // We checked our buffer
    size_t stolenChecksNum = 0;
    // Our buffer was empty
    size_t stolenNum = 0;
    size_t bufferCheckReportId = 0;

    size_t emptyNum = 0;
    size_t tryNum = 0;
    size_t emptyReportId = 0;

    void reportBufferEmpty(size_t size) {
      return;
//      stolenChecksNum++;
//      stolenNum++;
//      bufferCheckReportId++;
//      if (bufferCheckReportId >= STAT_BUFF_SIZE) {
//        checkNeedExtend(size);
//        clearSizeStats(); // todo needed or not
//      }, 4
    }

    void reportBufferFull(size_t size) {
      return;
      stolenChecksNum++;
//      bufferCheckReportId++;
//      if (bufferCheckReportId >= STAT_BUFF_SIZE) {
//        checkNeedExtend(size);
//        clearSizeStats();
//      }
    }

    // Whether stealing size should be extended
    void checkNeedExtend(size_t size) {
      return;
      // stolen / checks >= inc_percent / 100
      // stolen * 100 >= inc_percent * checks
//      size_t newSize = 0;
//      if (stolenNum * 100 <= 20 * stolenChecksNum) {
//        newSize = 1;
//      } else if (stolenNum * 100 <= 40 * stolenChecksNum) {
//        newSize = 2;
//      } else if (stolenNum * 100 <= 80 * stolenChecksNum) {
//        newSize = 4;
//      } else {
//
//      }
//      if (stolenNum * 100 >= INC_SIZE_PERCENT * stolenChecksNum) {
////        if (size >= 4 * stealSize) increaseSize();
//      } else if (stolenNum * 100 <= DEC_SIZE_PERCENT * stolenChecksNum) {
//        decreaseSize();
//      }
    }

    void reportStealStats(size_t ourBetter, size_t otherBetter) {
//      return;
      stealAttemptsNum += ourBetter + otherBetter;
      otherMinLessNum += otherBetter;
      stealReportId++;
      if (stealReportId >= STAT_PROB_SIZE) {
        checkProbChanges();
        clearProbStats();
      }
    }

    void reportEmpty(size_t empty, size_t total, size_t localSize) {
//      return;
      emptyNum += empty;
      tryNum += total;
      emptyReportId++;
      if (emptyReportId >= STAT_BUFF_SIZE) {
        checkEmpty(localSize);
        clearEmptyStats();
      }
    }

    void checkEmpty(size_t localSize) {
      size_t size = 0;
      if (emptyNum * 100 <= tryNum * 10) {
        size = 1;
      } else if (emptyNum * 100 <= tryNum * 30) {
        size = 1;
      } else if (emptyNum * 100 <= tryNum * 60) {
        size = 2;
      } else if (emptyNum * 100 <= tryNum * 90) {
        size = 8;
      } else {
        size = 16;
      }
      if (stealSize < size) {
        *sizeIncrease += 1;
      } else if (stealSize > size) {
        *sizeDecrease += 1;
      }
      maxSize->setMax(size);
      minSize->setMin(size);
      stealSize = size;
//
//      if (emptyNum * 100 >= tryNum * EMPTY_PROB_PERCENT/* && localSize >= 2 * stealSize*/) {
//        increaseSize();
//      } else if (emptyNum * 100 <= tryNum * DEC_SIZE_PERCENT) {
//        decreaseSize();
//      }
    }

    void checkProbChanges() {
      size_t newProb = 0;
      if (otherMinLessNum * 100 <= stealAttemptsNum * 5) {
        newProb = 32;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 10) {
        newProb = 16;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 20) {
        newProb = 16;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 60) {
        newProb = 8;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 80) {
        newProb = 8;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 90) {
        newProb = 4;
      } else {
        newProb = 2;
      }
      maxProb->setMax(newProb);
      minProb->setMin(newProb);
      if (newProb < stealProb) {
        *probIncreased += 1;
      } else if (newProb > stealProb) {
        *probDecreased += 1;
      }

      stealProb = newProb;
//      if (otherMinLessNum * 100 >= stealAttemptsNum * INC_PROB_PERCENT) {
//        increaseProb();
//      } else if (otherMinLessNum * 100 <= stealAttemptsNum * DEC_PROB_PERCENT) {
//        decreaseProb();
//      }
    }

    void clearSizeStats() {
      stolenNum = 0;
      stolenChecksNum = 0;
      bufferCheckReportId = 0;
    }

    void clearProbStats() {
      stealAttemptsNum = 0;
      otherMinLessNum = 0;
      stealReportId = 0;
    }

    void clearEmptyStats() {
      tryNum = 0;
      emptyNum = 0;
      emptyReportId = 0;
    }

//    void increaseProb() {
//      *probIncreased += 1;
//      return;
//      if (stealProb > MIN_STEAL_PROB) {
//        stealProb >>= 1u;
//        *probIncreased += 1;
//      }
//    }
//
//    void decreaseProb() {
//      *probDecreased += 1;
//      return;
//      if (stealProb < MAX_STEAL_PROB) {
//        stealProb <<= 1u;
//        *probIncreased += 1;
//        maxProb->setMax(stealProb);
//      }
//    }
//
//    void increaseSize() {
//      if (stealSize < MAX_STEAL_SIZE) {
//        stealSize <<= 1u;
//        *sizeIncrease += 1;
//        maxSize->setMax(stealSize);
//      }
//    }
//
//    void decreaseSize() {
//      if (stealSize > MIN_STEAL_SIZE) {
//        stealSize >>= 1u;
//        *sizeDecrease += 1;
//      }
//    }
  };

  Runtime::PerThreadStorage<PerThread> threadStorage;

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
    size_t ourBetter = 0;
    size_t tried = 0;
    size_t otherBetter = 0;
    size_t races = 0;
    size_t wasEmpty = 0;
    bool again = true;
    while (again) {
      tried++;
      again = false;
      // we try to steal
      auto randId = (tId + 1 + (random() % (nQ - 1))) % nQ;
      Heap *randH = &heaps[randId].data;
      auto randMin = randH->getMin(again);
      if (randH->isUsed(randMin)) {
        // steal is not successfull
        if (!again) wasEmpty++;
        continue;
      }
      bool useless;
      auto localMin = localH->getMin(useless);
      if (localH->isUsed(localMin)) {
        localMin = localH->fillBuffer(threadStorage.getLocal()->stealSize);
      }
      if (randH->isUsed(localMin) || compare(localMin, randMin)) {
        otherBetter++;
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

          threadStorage.getLocal()->reportStealStats(ourBetter, otherBetter);
          threadStorage.getLocal()->reportEmpty(wasEmpty, tried, localH->heap.size());
          return vals[minId];
//          typename Heap::stealing_array_t vals = stolen.get();
//          auto minId = localH->getMinId(vals);
//          for (size_t i = 0; i < vals.size(); i++) {
//            if (i == minId || localH->isUsed(vals[i])) continue;
//            localH->pushHelper(vals[i]);
//          }
//          return vals[minId];
        }
      } else {
        ourBetter++;
      }

    }
    threadStorage.getLocal()->reportStealStats(ourBetter, otherBetter);
    threadStorage.getLocal()->reportEmpty(wasEmpty, tried, localH->heap.size());
    return Galois::optional<T>();
  }

  //! Fills steal buffer if it is empty.
  void fillBufferIfNeeded() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    auto& heap = heaps[tId].data;
    PerThread* local = threadStorage.getLocal();
    if (heap.isBufferStolen()) {
      heap.fillBuffer(local->stealSize);
      local->reportBufferEmpty(heap.heap.size());
    } else {
      local->reportBufferFull(heap.heap.size());
    }
  }

public:
  static Galois::Statistic* maxSize;
  static Galois::Statistic* minSize;
  static Galois::Statistic* maxProb;
  static Galois::Statistic* minProb;
  static Galois::Statistic* sizeIncrease;
  static Galois::Statistic* sizeDecrease;
  static Galois::Statistic* probIncreased;
  static Galois::Statistic* probDecreased;

  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

  AdaptiveStealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);

    initStatistic(maxSize, "maxSize");
    initStatistic(minSize, "minSize");
    initStatistic(maxProb, "maxProb");
    initStatistic(minProb, "minProb");
    initStatistic(sizeIncrease, "sizeInc");
    initStatistic(sizeDecrease, "sizeDec");
    initStatistic(probIncreased, "probInc");
    initStatistic(probDecreased, "probDec");
    *minSize = MIN_STEAL_SIZE;
    *maxSize = MIN_STEAL_SIZE;
    *minProb = MIN_STEAL_PROB;
    *maxProb = MIN_STEAL_PROB;
  }

  void deleteStatistic(Galois::Statistic*& st, std::ofstream& out) {
    if (st != nullptr) {
      if (st->getStatname().find("min") != std::string::npos) {
        out << "," << getMinVal(st);
      } else if (st->getStatname().find("max") != std::string::npos) {
        out << "," << getMaxVal(st);
      } else {
        out << "," << getStatVal(st);
      }
      delete st;
      st = nullptr;
    }
  }

  ~AdaptiveStealingMultiQueue() {
    auto exists = std::filesystem::exists("asmq_stats.csv");
    std::ofstream out("asmq_stats.csv" /*result_name*/, std::ios::app);
    if (!exists) {
      out << "threads,seg_buff,seg_prob,minSize,maxSize,inc_size,decSize,minProb,maxProb,,inc_prob,dec_prob" << std::endl;
    }
    out << nQ << "," << STAT_BUFF_SIZE  << "," << STAT_PROB_SIZE << ",";
//
//    out << nQ << "," << STAT_BUFF_SIZE  << "," << STAT_PROB_SIZE << "," << INC_PROB_PERCENT
//    << "," << DEC_PROB_PERCENT;
//    deleteStatistic(sizeIncrease, out);
//    out << "," << MIN_STEAL_SIZE;
    deleteStatistic(minSize, out);
    deleteStatistic(maxSize, out);
    deleteStatistic(sizeIncrease, out);
    deleteStatistic(sizeDecrease, out);
//    out << "," << MIN_STEAL_PROB;
    deleteStatistic(minProb, out);
    deleteStatistic(maxProb, out);
    deleteStatistic(probIncreased, out);
    deleteStatistic(probDecreased, out);
    out << std::endl;
    out.close();
  }

  uint64_t getMaxVal(Galois::Statistic* value) {
    uint64_t stat = 0;
    for (unsigned x = 0; x < Galois::Runtime::activeThreads; ++x)
      stat = std::max(stat, value->getValue(x));
    return stat;
  }

  uint64_t getMinVal(Galois::Statistic* value) {
    uint64_t stat = SIZE_MAX;
    for (unsigned x = 0; x < Galois::Runtime::activeThreads; ++x) {
      if (value->getValue(x) < 1)
        continue;
      stat = std::min(stat, value->getValue(x));
    }
    return stat;
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
    typedef AdaptiveStealingMultiQueue<T, Comparer,
    _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveStealingMultiQueue<_T, Comparer,
    Concurrent> type;
  };

  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    Heap* heap = &heaps[tId].data;
    return heap->pushRange(b, e, threadStorage.getLocal()->stealSize);
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
      if (Heap::isUsed(curMin) /**&& cmp(curMin, heap[0])*/ && !local.heap.empty()) {
        local.fillBuffer(threadStorage.getLocal()->stealSize);
      }
      return val;
    }
    Galois::optional<T> result;

    if (nQ > 1) {
      size_t change = random() % threadStorage.getLocal()->stealProb;
      if (change < 1) {
        auto stolen = trySteal();
        if (stolen.is_initialized()) return stolen;
      }
    }
    auto minVal = heaps[tId].data.extractMin(threadStorage.getLocal()->stealSize);
    if (!heaps[tId].data.isUsed(minVal)) {
      return minVal;
    }
    // our heap is empty
    if (nQ == 1)  { // nobody to steal from
      return result;
    }
    auto stolen = trySteal();
    if (stolen.is_initialized()) return stolen;
    return result;
  }
};

GALOIS_WLCOMPILECHECK(AdaptiveStealingMultiQueue)

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::sizeIncrease;
template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::sizeDecrease;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::minSize;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::maxSize;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::probIncreased;
template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::probDecreased;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::minProb;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::maxProb;


} // namespace WorkList
} // namespace Galois

#endif //GALOIS_ADAPTIVE_SMQ_H