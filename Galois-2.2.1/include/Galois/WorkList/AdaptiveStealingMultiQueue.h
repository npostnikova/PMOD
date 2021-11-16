#ifndef GALOIS_ADAPTIVE_SMQ_H
#define GALOIS_ADAPTIVE_SMQ_H

#include <atomic>
#include <cstdlib>
#include <vector>
//#include <filesystem>

#include "StealingQueue.h"
#include "Heap.h"
#include "StealingMultiQueue.h"

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
struct HeapWithStealBufferAdap {
  Compare compare;
  // Represents a flag for empty cells.
  static T dummy;
  DAryHeap<T, Compare, D> heap;

  HeapWithStealBufferAdap(): version(0) {
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
    auto v2 = getVersion();
    if (v1 == v2) {
      return res;
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
      auto v2 = getVersion();
      if (v1 == v2) {
        return res;
      }
    }
    return fillBuffer(elementsSum);
  }

  //! Tries to steal the elements from the stealing buffer.
  //! Returns the number of stolen elements, the elements are written in the buffer.
  int trySteal(bool& raceHappened, T* buffer) {
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      // Already stolen
      return 0;
    }
    size_t ind = 0;

    for (; ind < STEAL_NUM; ind++) {
      if (!isDummy(stealBuffer[ind])) buffer[ind] = stealBuffer[ind];
      else break;
    }
    if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
      return ind;
    }
    // Another thread got ahead
    raceHappened = true;
    return 0;
  }

  //! Fills the steal buffer.
  //! Called when the elements from the previous epoch are empty.
  T fillBuffer(size_t elementsNumber) {
    if (heap.empty()) return dummy;
    static thread_local size_t prevFillNum = STEAL_NUM;
    size_t curFillNum = 0;
    for (; curFillNum < elementsNumber && !heap.empty(); curFillNum++) {
      stealBuffer[curFillNum] = heap.extractMin();
    }
    for (size_t j = curFillNum; j < prevFillNum; j++) {
      stealBuffer[j] = dummy;
    }
    prevFillNum = curFillNum;
    version.fetch_add(1, std::memory_order_acq_rel);
    return stealBuffer[0];
  }

  //! Extract min from the structure: both the buffer and the heap
  //! are considered. Called from the owner-thread.
  Galois::optional<T> extractMin(size_t elementsNum, T* buffer) {
    if (heap.empty()) {
      return tryStealLocally(buffer);
    }
    bool raceFlag = false; // useless now
    auto bufferMin = getBufferMin(raceFlag);
    if (!isDummy(bufferMin) && compare(heap.min(), bufferMin)) {
      auto stolen = tryStealLocally(buffer);
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
  Galois::optional<T> tryStealLocally(T* buffer) {
    bool raceFlag = false; // useless now
    const auto stolen = trySteal(raceFlag, buffer);
    if (stolen > 0) {
      for (size_t i = 1; i < stolen; i++) {
        heap.push(buffer[i]);
      }
      return buffer[0];
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
T HeapWithStealBufferAdap<T, Compare, STEAL_NUM, D>::dummy;

template<typename T,
typename Comparer,
bool Concurrent = true>
class AdaptiveStealingMultiQueue {
private:
  //! Number of reports needed to update the number of elements for stealing.
  static const size_t BUFF_REPORT_CNT = 16;
  //! Number of reports needed to update stealing probability.
  static const size_t PROB_REPORT_CNT = 1024;

  //! Size of stealing buffers, which is the maximum steal batch.
  static const size_t STEAL_BUFFER_SIZE = 16;
  //! Initial buffer fill size.
  static const size_t INIT_BUFFER_FILL = 16;
  //! Initial stealing probability.
  static const size_t INIT_STEAL_PROB = 8;
  typedef HeapWithStealBufferAdap<T, Comparer, STEAL_BUFFER_SIZE, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]> stealBuffers;
  Comparer compare;
  const size_t nQ;

  struct PerThread {
    //! Buffer for stolen elements.
    T stolen[STEAL_BUFFER_SIZE];

    size_t stealProb = INIT_STEAL_PROB;
    size_t stealSize = INIT_BUFFER_FILL;

    // We wanted to steal
    size_t stealAttemptsNum = 0;
    // We tried to steal
    size_t otherMinLessNum = 0;
    size_t stealReportId = 0;

    size_t emptyNum = 0;
    size_t tryNum = 0;
    size_t emptyReportId = 0;


    void reportBetterPer(size_t ourBetter, size_t otherBetter) {
      stealAttemptsNum += ourBetter + otherBetter;
      otherMinLessNum += otherBetter;
      stealReportId++;
      if (stealReportId >= PROB_REPORT_CNT) {
        updateProbability();
        clearProbStats();
      }
    }

    void reportEmptyPer(size_t empty, size_t total) {
      emptyNum += empty;
      tryNum += total;
      emptyReportId++;
      if (emptyReportId >= BUFF_REPORT_CNT) {
        updateSize();
        clearEmptyStats();
      }
    }

    void updateSize() {
      size_t size = 0;
      if (emptyNum * 100 <= tryNum * 30) {
        size = 1;
        *size1 += 1;
      } else if (emptyNum * 100 <= tryNum * 60) {
        size = 2;
        *size2 += 1;
      } else if (emptyNum * 100 <= tryNum * 90) {
        size = 8;
        *size8 += 1;
      } else {
        size = 16;
        *size16 += 1;
      }
      stealSize = size;
    }

    void updateProbability() {
      size_t newProb = 0;
      if (otherMinLessNum * 100 <= stealAttemptsNum * 5) {
        newProb = 32;
        *prob32 += 1;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 20) {
        newProb = 16;
        *prob16 += 1;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 80) {
        newProb = 8;
        *prob8 += 1;
      } else if (otherMinLessNum * 100 <= stealAttemptsNum * 90) {
        newProb = 4;
        *prob4 += 1;
      } else {
        newProb = 2;
        *prob2 += 1;
      }
      stealProb = newProb;
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
  };

  Runtime::PerThreadStorage<PerThread> threadStorage;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x =
        std::chrono::system_clock::now().time_since_epoch().count() % 16386 + 1;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
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
    size_t ourBetterCnt = 0;
    size_t otherBetterCnt = 0;
    size_t emptyCnt = 0;
    size_t stealAttemptCnt = 0;
    while (nextIterNeeded) {
      stealAttemptCnt++;
      nextIterNeeded = false;
      auto randId = rand_heap();
      if (randId == tId) continue;
      Heap* randH = &heaps[randId].data;
      auto randMin = randH->getBufferMin(nextIterNeeded);
      if (randH->isDummy(randMin)) {
        // Nothing to steal
        if (!nextIterNeeded) emptyCnt++;
        continue;
      }
      if (Heap::isDummy(localMin) || compare(localMin, randMin)) {
        otherBetterCnt++;
        T* stolenBuff = threadStorage.getLocal()->stolen;
        const auto stolen = randH->trySteal(nextIterNeeded, stolenBuff);
        if (stolen > 0) {
          auto& buffer = stealBuffers[tId].data;
          for (size_t i = 1; i < stolen; i++) {
            buffer.push_back(stolenBuff[i]);
          }
          std::reverse(buffer.begin(), buffer.end());
          threadStorage.getLocal()->reportBetterPer(ourBetterCnt, otherBetterCnt);
          threadStorage.getLocal()->reportEmptyPer(emptyCnt, stealAttemptCnt);
          return threadStorage.getLocal()->stolen[0];
        } 
      } else {
        ourBetterCnt++;
      }
    }
    threadStorage.getLocal()->reportBetterPer(ourBetterCnt, otherBetterCnt);
    threadStorage.getLocal()->reportEmptyPer(emptyCnt, stealAttemptCnt);
    return Galois::optional<T>();
  }

  //! Fills steal buffer if it is empty.
  void fillBufferIfNeeded() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    auto& heap = heaps[tId].data;
    PerThread* local = threadStorage.getLocal();
    if (heap.isBufferStolen()) {
      heap.fillBuffer(local->stealSize);
    }
  }

public:
  static Galois::Statistic* size1;
  static Galois::Statistic* size2;
  static Galois::Statistic* size8;
  static Galois::Statistic* size16;
  static Galois::Statistic* prob2;
  static Galois::Statistic* prob4;
  static Galois::Statistic* prob8;
  static Galois::Statistic* prob16;
  static Galois::Statistic* prob32;

  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

  AdaptiveStealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::dummy), 0xff, sizeof(Heap::dummy));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    stealBuffers = std::make_unique<Galois::Runtime::LL::CacheLineStorage<std::vector<T>>[]>(nQ);

    initStatistic(size1, "size1");
    initStatistic(size2, "size2");
    initStatistic(size8, "size8");
    initStatistic(size16, "size16");
    initStatistic(prob2, "prob2");
    initStatistic(prob4, "prob4");
    initStatistic(prob8, "prob8");
    initStatistic(prob16, "prob16");
    initStatistic(prob32, "prob32");
  }

  void deleteStatistic(Galois::Statistic*& st, std::ofstream& out) {
    if (st != nullptr) {
      out << "," << getStatVal(st);
      delete st;
      st = nullptr;
    }
  }

  ~AdaptiveStealingMultiQueue() {
    auto exists = true; // std::filesystem::exists("asmq_stats.csv");
    std::ofstream out("asmq_stats.csv", std::ios::app);
    if (!exists) {
      out << "threads,s1,s2,s8,s16,p2,p4,p8,p16,p32" << std::endl;
    }
    out << nQ;
    deleteStatistic(size1, out);
    deleteStatistic(size2, out);
    deleteStatistic(size8, out);
    deleteStatistic(size16, out);
    deleteStatistic(prob2, out);
    deleteStatistic(prob4, out);
    deleteStatistic(prob8, out);
    deleteStatistic(prob16, out);
    deleteStatistic(prob32, out);

    out << std::endl;
    out.close();
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

GALOIS_WLCOMPILECHECK(AdaptiveStealingMultiQueue)

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::size1;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::size2;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::size8;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::size16;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::prob2;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::prob4;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::prob8;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::prob16;

template<typename T,
typename Comparer,
bool Concurrent> Statistic* AdaptiveStealingMultiQueue<
T, Comparer, Concurrent>::prob32;

} // namespace WorkList
} // namespace Galois

#endif //GALOIS_ADAPTIVE_SMQ_H
