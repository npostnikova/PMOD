#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>
#include <filesystem>

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
    auto res = stealBuffer[0];
//    std::array<T, STEAL_NUM> vals = stealBuffer;
    auto v2 = getVersion();
    if (v1 == v2) {
      return res;
//      return vals[0];
    }
    // Somebody has stolen the elements
    raceHappened = true;
    return dummy;
  }

  //! Returns min element from the buffer, updating the buffer if empty.
  //! Can be called only by the thread-owner.
  T getMinWriter(size_t elementsSum) {
    auto v1 = getVersion();
    if (v1 % 2 != 0) {
      auto res = stealBuffer[0];
//      std::array<T, STEAL_NUM> vals = stealBuffer;
      auto v2 = getVersion();
      if (v1 == v2) {
        return res;
//        return vals[0];
      }
    }
    return fillBuffer(elementsSum);
  }

  //! Tries to steal the elements from the stealing buffer.
  int trySteal(bool& raceHappened, T* arr) {
//    auto emptyRes = Galois::optional<std::vector<T>>();
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      // Already stolen
      return 0; //emptyRes;
    }
    size_t ind = 0;
//     arr[ind] = stealBuffer[0];
//    static std::vector<T> res;
//    res.push_back(stealBuffer[0]);

    for (; ind < STEAL_NUM; ind++) {
      if (!isDummy(stealBuffer[ind])) arr[ind] = stealBuffer[ind];
      else break;
    }
//    }
//    std::array<T, STEAL_NUM> buffer = stealBuffer;
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return ind; //buffer;
    }
    // Another thread got ahead
    raceHappened = true;
    return 0; //emptyRes;
  }

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBuffer(size_t elementsNumber) {
    if (heap.empty()) return dummy;
    static thread_local size_t prevFill = STEAL_NUM;
//    std::array<T, STEAL_NUM> elements;
//    elements.fill(dummy);
    size_t curFill = 0;
    for (; curFill < elementsNumber && !heap.empty(); curFill++) {
      stealBuffer[curFill] = heap.extractMin();
    }
    for (size_t j = curFill; j < prevFill; j++) {
      stealBuffer[j] = dummy;
    }
    prevFill = curFill;
//      stealBuffer[0] = heap.extractMin();
//    stealBuffer = elements;
    version.fetch_add(1, std::memory_order_acq_rel);
    return stealBuffer[0];
  }

  //! Extract min from the structure: both the buffer and the heap
  //! are considered. Called from the owner-thread.
  Galois::optional<T> extractMin(size_t elementsNum, T* arr) {
    if (heap.empty()) {
      // Only check the steal buffer
      return tryStealLocally(arr);
    }
    bool raceFlag = false; // useless now
    auto bufferMin = getBufferMin(raceFlag);
    if (!isDummy(bufferMin) && compare(heap.min(), bufferMin)) {
      auto stolen = tryStealLocally(arr);
      if (stolen.is_initialized()) {
        fillBuffer(elementsNum);
        return stolen;
      }
    }
    auto localMin = heap.extractMin();
    if (isDummy(bufferMin)) fillBuffer(elementsNum);
    return localMin;
  }

private:
  //! Tries to steal elements from local buffer.
  //! Return minimum among stolen elements.
  Galois::optional<T> tryStealLocally(T* arr) {
    bool raceFlag = false; // useless now
    const auto stolen = trySteal(raceFlag, arr);
    if (stolen > 0) {
//      auto elements = stolen.get();
//      while (stolen)
      for (size_t i = 1; i < stolen; i++) {
        /*if (!isDummy(elements[i])) */ heap.push(arr[i]);
      }
      return arr[0]; //elements[0];
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
size_t STAT_BUFF_SIZE = 4,
size_t STAT_PROB_SIZE = 4,
size_t INC_SIZE_PERCENT = 80,
size_t DEC_SIZE_PERCENT = 40,
size_t INC_PROB_PERCENT = 80,
size_t DEC_PROB_PERCENT = 40,
size_t EMPTY_PROB_PERCENT = 40,
bool Concurrent = true>
class StealingMultiQueue {
private:
  static const size_t MIN_STEAL_SIZE = 1;
  static const size_t MAX_STEAL_SIZE = 16;
  static const size_t MIN_STEAL_PROB = 8;
  static const size_t MAX_STEAL_PROB = 8;
  typedef HeapWithStealBuffer<T, Comparer, MAX_STEAL_SIZE, 4> Heap;
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
      stolenChecksNum++;
      stolenNum++;
      bufferCheckReportId++;
      if (bufferCheckReportId >= STAT_BUFF_SIZE) {
        checkNeedExtend(size);
        clearSizeStats(); // todo needed or not
      }
    }

    void reportBufferFull(size_t size) {
      return;
      stolenChecksNum++;
      bufferCheckReportId++;
      if (bufferCheckReportId >= STAT_BUFF_SIZE) {
        checkNeedExtend(size);
        clearSizeStats();
      }
    }

    // Whether stealing size should be extended
    void checkNeedExtend(size_t size) {
      return;
      // stolen / checks >= inc_percent / 100
      // stolen * 100 >= inc_percent * checks
      size_t newSize = 0;
      if (stolenNum * 100 <= 20 * stolenChecksNum) {
        newSize = 1;
      } else if (stolenNum * 100 <= 40 * stolenChecksNum) {
        newSize = 2;
      } else if (stolenNum * 100 <= 80 * stolenChecksNum) {
        newSize = 4;
      } else {

      }
      if (stolenNum * 100 >= INC_SIZE_PERCENT * stolenChecksNum) {
//        if (size >= 4 * stealSize) increaseSize();
      } else if (stolenNum * 100 <= DEC_SIZE_PERCENT * stolenChecksNum) {
        decreaseSize();
      }
    }

    void reportStealStats(size_t ourBetter, size_t otherBetter) {
      return;
      stealAttemptsNum += ourBetter + otherBetter;
      otherMinLessNum += otherBetter;
      stealReportId++;
      if (stealReportId >= STAT_PROB_SIZE) {
        checkProbChanges();
        clearProbStats();
      }
    }

    void reportEmpty(size_t empty, size_t total, size_t localSize) {
      return;
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
        size = 3;
      } else {
        size = 4;
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
        newProb = 64;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 10) {
        newProb = 32;
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

    void increaseProb() {
      *probIncreased += 1;
      return;
      if (stealProb > MIN_STEAL_PROB) {
        stealProb >>= 1u;
        *probIncreased += 1;
      }
    }

    void decreaseProb() {
      *probDecreased += 1;
      return;
      if (stealProb < MAX_STEAL_PROB) {
        stealProb <<= 1u;
        *probIncreased += 1;
        maxProb->setMax(stealProb);
      }
    }

    void increaseSize() {
      if (stealSize < MAX_STEAL_SIZE) {
        stealSize <<= 1u;
        *sizeIncrease += 1;
        maxSize->setMax(stealSize);
      }
    }

    void decreaseSize() {
      if (stealSize > MIN_STEAL_SIZE) {
        stealSize >>= 1u;
        *sizeDecrease += 1;
      }
    }
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
    auto& local = heaps[tId].data;
    T localMin = heaps[tId].data.getMinWriter(threadStorage.getLocal()->stealSize);
    bool nextIterNeeded = true;
    size_t ourBetter = 0;
    size_t tried = 0;
    size_t otherBetter = 0;
    size_t races = 0;
    size_t wasEmpty = 0;
    while (nextIterNeeded) {
      tried++;
      nextIterNeeded = false;
      auto randId = rand_heap();
      if (randId == tId) continue;
      Heap* randH = &heaps[randId].data;
      auto randMin = randH->getBufferMin(nextIterNeeded);
      if (randH->isDummy(randMin)) {
        // Nothing to steal
        if (!nextIterNeeded) wasEmpty++;
        continue;
      }
      if (Heap::isDummy(localMin) || compare(localMin, randMin)) {
        otherBetter++;
        T* stolenBuff = threadStorage.getLocal()->stolen;
        const auto stolen = randH->trySteal(nextIterNeeded, stolenBuff);
        if (stolen > 0) {
//          auto elements = stolen.get();
          auto& buffer = stealBuffers[tId].data;
          for (size_t i = 1; i < stolen /*MAX_STEAL_SIZE*/; i++) {
//            if (!Heap::isDummy(elements[i]))
              buffer.push_back(stolenBuff[i]);
//            else break;
          }
          std::reverse(buffer.begin(), buffer.end());
          threadStorage.getLocal()->reportStealStats(ourBetter, otherBetter);
          threadStorage.getLocal()->reportEmpty(wasEmpty, tried, local.heap.size());
          return threadStorage.getLocal()->stolen[0]; //elements[0];
        } else {
//          wasEmpty++;
        }
      } else {
        ourBetter++;
      }
    }
    threadStorage.getLocal()->reportStealStats(ourBetter, otherBetter);
    threadStorage.getLocal()->reportEmpty(wasEmpty, tried, local.heap.size());
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

  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));
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

  ~StealingMultiQueue() {
    auto exists = std::filesystem::exists("smq_stats.csv");
    std::ofstream out("smq_stats.csv" /*result_name*/, std::ios::app);
    if (!exists) {
//      out << "threads,stat_buff,stat_prob,inc_size,dec_size,inc_prob,dec_prob,empty_prob" <<
      out << "threads,prob,segment,inc_prob_per,dec_prob_per,incProb,decProb" << std::endl;
    }
//    out << nQ << "," << STAT_BUFF_SIZE  << "," << STAT_PROB_SIZE << "," << INC_SIZE_PERCENT << ","
//        << DEC_SIZE_PERCENT << "," << INC_PROB_PERCENT << "," << DEC_PROB_PERCENT << "," << EMPTY_PROB_PERCENT;

    out << nQ << "," << STAT_BUFF_SIZE  << "," << STAT_PROB_SIZE << "," << INC_PROB_PERCENT
    << "," << DEC_PROB_PERCENT;
//    deleteStatistic(sizeIncrease, out);
//    out << "," << MIN_STEAL_SIZE;
    deleteStatistic(minSize, out);
    deleteStatistic(maxSize, out);
    deleteStatistic(sizeIncrease, out);
    deleteStatistic(sizeDecrease, out);
//    out << "," << MIN_STEAL_PROB;
//    deleteStatistic(minProb, out);
//    deleteStatistic(maxProb, out);
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
    typedef StealingMultiQueue<T, Comparer,
    STAT_BUFF_SIZE, STAT_PROB_SIZE,
    INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
    INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT,
    _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueue<_T, Comparer,
    STAT_BUFF_SIZE, STAT_PROB_SIZE,
    INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
    INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT,
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
    if (b == e) return 0;
    unsigned int pushedNum = 0;
    Heap* heap = &heaps[tId].data;
    while (b != e) {
      heap->heap.push(*b++);
      pushedNum++;
    }
    fillBufferIfNeeded();
    return pushedNum;
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    Galois::optional<T> emptyResult;
    auto& buffer = stealBuffers[tId].data;
    if (!buffer.empty()) {
      auto val = buffer.back();
      buffer.pop_back();
      fillBufferIfNeeded();
      return val;
    }

    // rand == 0 -- try to steal
    // otherwise, pop locally
    if (nQ > 1 && random() % (threadStorage.getLocal()->stealProb) == 0) {
      Galois::optional<T> stolen = trySteal();
      if (stolen.is_initialized()) return stolen;
    }
    auto minVal = heaps[tId].data.extractMin(threadStorage.getLocal()->stealSize, threadStorage.getLocal()->stolen);
    if (minVal.is_initialized()) return minVal;

    // Our heap is empty
    return nQ == 1 ? emptyResult : trySteal();
  }
};

GALOIS_WLCOMPILECHECK(StealingMultiQueue)

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::sizeIncrease;
template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::sizeDecrease;

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::minSize;

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::maxSize;

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::probIncreased;
template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::probDecreased;

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::minProb;

template<typename T,
typename Comparer,
size_t STAT_BUFF_SIZE,
size_t STAT_PROB_SIZE,
size_t INC_SIZE_PERCENT,
size_t DEC_SIZE_PERCENT,
size_t INC_PROB_PERCENT,
size_t DEC_PROB_PERCENT,
size_t  EMPTY_PROB_PERCENT,
bool Concurrent> Statistic* StealingMultiQueue<
T, Comparer, STAT_BUFF_SIZE, STAT_PROB_SIZE,
INC_SIZE_PERCENT, DEC_SIZE_PERCENT,
INC_PROB_PERCENT, DEC_PROB_PERCENT, EMPTY_PROB_PERCENT, Concurrent>::maxProb;


} // namespace WorkList
} // namespace Galois

#endif //GALOIS_STEALINGMULTIQUEUE_H