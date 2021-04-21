#ifndef ADAPTIVE_MULTIQUEUE_H
#define ADAPTIVE_MULTIQUEUE_H

#include <atomic>
#include <memory>
#include <cstdlib>
#include <mutex>
#include <condition_variable>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <thread>
#include <random>
#include <array>
#include <iostream>
#include "Heap.h"
#include "WorkListHelpers.h"

namespace Galois {
namespace WorkList {

/**
 * Lockable heap structure.
 *
 * @tparam T type of stored elements
 * @tparam Comparer callable defining ordering for objects of type `T`.
 * Its `operator()` returns `true` iff the first argument should follow the second one.
 */

static const bool AMQ_DEBUG = false;

template <typename  T, typename Comparer, size_t D = 4, typename Prior = unsigned long>
struct LockableHeap {
  DAryHeap<T, Comparer, 4> heap;
  // todo: use atomic
  std::atomic<Prior> min;
  std::atomic<size_t> size;
  static T usedT;

  LockableHeap() : min(usedT.prior()), size(0) {}

  //! Non-blocking lock.
  inline bool try_lock() {
    return _lock.try_lock();
  }

  //! Blocking lock.
  inline void lock() {
    _lock.lock();
  }

  //! Unlocks the queue.
  inline void unlock() {
    _lock.unlock();
  }

  inline bool is_locked() {
    _lock.is_locked();
  }

  Prior getMin() {
    return min.load(std::memory_order_acquire);
  }

  void writeMin(Prior value = usedT.prior()) {
    return min.store(value, std::memory_order_release);
  }

  void updateMin() {
    return min.store(
    heap.size() > 0 ? heap.min().prior() : usedT.prior(),
    std::memory_order_release
    );
  }

  static bool isUsed(T const& value) {
    return value == usedT;
  }

  static bool isUsedMin(Prior const& value) {
    return value == usedT.prior();
  }

  size_t getSize() {
    return size.load(std::memory_order_acquire);
  }

  void updateSize() {
    return size.store(heap.size(), std::memory_order_release);
  }

private:
  Runtime::LL::PaddedLock<true> _lock;
};



template<typename T,
typename Compare,
size_t D, typename Prior>
T LockableHeap<T, Compare, D, Prior>::usedT;

// Probability P / Q
template <size_t PV, size_t QV>
struct Prob {
  static const size_t P = PV;
  static const size_t Q = QV;
};

Prob<1, 1> oneProb;

/**
 * Basic implementation. Provides effective pushing of range of elements only.
 *
 * @tparam T type of elements
 * @tparam Comparer comparator for elements of type `T`
 * @tparam DecreaseKey if auto decrease key should be supported
 * @tparam DecreaseKeyIndexer indexer for decrease key operation
 * @tparam C parameter for queues number
 * @tparam Concurrent if the implementation should be concurrent
 * @tparam Blocking if the implementation should be blocking
 * @tparam PushChange probability of changing the local queue for push
 * @tparam PopChange probability of changing the local queue for pop
 * @tparam Numa weights for choosing same/not same NUMA node
 * @tparam Prior type which is used for task priority
 * @tparam PERCENT_F weight for success event
 * @tparam PERCENT_LF weight for failure event
 * @tparam PERCENT_E weight for empty event
 * @tparam REFRESH_SIZE number of reported events needed for adaptivity changes
 * @tparam PERCENT_S proportion of events to make the decision on adaptivity changes
 * @tparam RESUME_SIZE number of elements in the queue which signal resume is needed
 */
template<typename T,
typename Comparer,
size_t C = 2,
bool DecreaseKey = false,
typename DecreaseKeyIndexer = void,
bool Concurrent = true,
bool Blocking = false,
typename PushChange = Prob<1, 1>,
typename PopChange  = Prob<1, 1>,
typename Numa = Prob<2, 1>,
typename Prior = unsigned long,
size_t PERCENT_F = 1, // todo
size_t PERCENT_LF = 1,
size_t PERCENT_E = 1,
size_t REFRESH_SIZE = 32,
size_t PERCENT_S = 90,
size_t RESUME_SIZE = REFRESH_SIZE
>
class AdaptiveMultiQueue {
private:
  typedef T value_t;
  typedef LockableHeap<T, Comparer, 4, Prior> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
//  const int nQ;
  std::atomic<int> nQ = {0}; // todo atomic for adaptive!!!

  //! Maximum element of type `T`.
  // T maxT;
  //! The number of queues is changed under the mutex.
  Runtime::LL::PaddedLock<Concurrent> adaptLock;
  const size_t maxQNum;

  // todo int?
  int getNQ() {
    return nQ.load(std::memory_order_acquire);
  }

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random() + 1; // todo
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


  const size_t socketSize = 24;
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

  size_t is1Node(size_t tId) {
    return tId < socketSize || (tId >= socketSize * 2 && tId < socketSize * 3);
  }

  size_t is2Node(size_t tId) {
    return !is1Node(tId);
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

  size_t rand_heap(size_t curnQ) {
    return random() % curnQ;
  }

  inline size_t rand_heap() {
    return random() % getNQ();
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    static const size_t LOCAL_W = Numa::P;
    static const size_t OTHER_W = Numa::Q;

    size_t isFirst = is1Node(tId);
    size_t localCnt = isFirst ? node1Cnt() : node2Cnt();
    size_t otherCnt = nT - localCnt;
    const size_t Q = localCnt * LOCAL_W * C + otherCnt * OTHER_W * C;
    const size_t r = random() % Q;
    if (r < localCnt * LOCAL_W * C) {
      // we are stealing from our node
      auto qId = r / LOCAL_W;
      return isFirst ? map1Node(qId) : map2Node(qId);
    } else {
      auto qId = (r - localCnt * LOCAL_W * C) / OTHER_W;
      return isFirst ? map2Node(qId) : map1Node(qId);
    }
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap, size_t success, size_t failure, size_t empty,
                                        size_t localStat, size_t curNQ, size_t tId, size_t avgSize, size_t sizeN) {
    auto result = getMin(heap);

    heap->updateMin();
    heap->updateSize();
    if (heap->heap.empty()) {
//      empty_queues.fetch_add(1, std::memory_order_acq_rel);
    }
    heap->unlock();
    reportPop(success, failure, empty, localStat, curNQ, tId, avgSize, sizeN);
    return result;
  }

  //! Gets minimum from locked heap which depends on AMQ flags.
  value_t getMin(Heap* heap) {
    if constexpr (DecreaseKey) {
      static DecreaseKeyIndexer indexer;
      return heap->heap.extractMin(indexer);
    } else {
      return heap->heap.extractMin();
    }
  }

  /////////// BLOCKING //////////////////

  struct CondNode {
    enum State {
      FREE,
      SUSPENDED,
      RESUMED
    };

    std::condition_variable cond_var;
    std::atomic<State> state = { FREE };
    std::mutex cond_mutex;
  };

  //! Number of locked queues.
  std::atomic<size_t> suspended = {0u};
  //! All the queues are empty. Rarely changed.
  std::atomic<bool> no_work = {false};
  //! Number of empty queues.
  std::atomic<size_t> empty_queues = { nQ };
  //! Array for threads to suspend.
  std::unique_ptr<CondNode[]> suspend_array;
  //! Suspend array size if CondC * nQ.
  static const size_t CondC = 8;

  //! Size of suspend array.
  inline size_t suspend_size() {
    return nT * CondC;
  }

  inline size_t rand_suspend_cell() {
    return random() % suspend_size();
  }

  inline size_t get_empty_queues() {
    return empty_queues.load(std::memory_order_acquire);
  }

  //! Suspends if the thread is not the last.
  void suspend() {
    while (get_empty_queues() == getNQ()) {
      size_t cell_id = rand_suspend_cell();
      if (try_suspend(cell_id)) {
        return;
      }
      for (size_t i = 1; i < 8 && i + cell_id < suspend_size(); i++) {
        if (try_suspend(i + cell_id))
          return;
      }
    }
  }

  //! Resumes at most cnt threads.
  void resume(size_t cnt = 1) {
    if (suspended <= 0) return;
    auto r = random() % suspend_size();
    for (size_t i = r; i < r + 8 && i < suspend_size() && cnt != 0; i++) {
      if (try_resume(i))
        cnt--;
    }
  }

  //! Resumes all the threads. Blocking.
  void resume_all() {
    no_work = true;
    for (size_t i = 0; i < suspend_size(); i++) {
      blocking_resume(i);
    }
  }

  //! Locks the cell if it's free.
  bool try_suspend(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::FREE)
      return false;
    if (!node.cond_mutex.try_lock())
      return false;
    if (node.state != CondNode::FREE) {
      node.cond_mutex.unlock();
      return false;
    }
    auto suspended_num = suspended.fetch_add(1, std::memory_order_acq_rel);
    *suspendedNum += 1;
    if (suspended_num + 1 == nT) {
      node.cond_mutex.unlock();
      suspended.fetch_sub(1, std::memory_order_acq_rel);
      *resumedNum += 1;
      if (get_empty_queues() < getNQ())
        return true;
      resume_all();
      return true;
    }
    node.state = CondNode::SUSPENDED;
    std::unique_lock<std::mutex> lk(node.cond_mutex, std::adopt_lock);
    node.cond_var.wait(lk, [&node] { return node.state == CondNode::RESUMED; });
    node.state = CondNode::FREE;
    // lk.unlock();
    return true;
  }

  //! Resumes if the cell is busy with a suspended thread. Non-blocking.
  bool try_resume(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::SUSPENDED)
      return false;
    if (!node.cond_mutex.try_lock())
      return false;
    if (node.state != CondNode::SUSPENDED) {
      node.cond_mutex.unlock();
      return false;
    }
    std::lock_guard<std::mutex> guard(node.cond_mutex, std::adopt_lock);
    node.state = CondNode::RESUMED;
    node.cond_var.notify_one();
    suspended.fetch_sub(1, std::memory_order_acq_rel);
    *resumedNum += 1;
    return true;
  }

  //! Resumes if the cell is busy with a suspended thread. Blocking.
  void blocking_resume(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::SUSPENDED)
      return;
    std::lock_guard<std::mutex> guard(node.cond_mutex);
    if (node.state == CondNode::SUSPENDED) {
      suspended.fetch_sub(1, std::memory_order_acq_rel);
      *resumedNum += 1;
      node.state = CondNode::RESUMED;
      node.cond_var.notify_all();
    }
  }

  //! Active waiting to avoid using shared data.
  void active_waiting(int iters) {
    static thread_local std::atomic<uint32_t> consumedCPU = { random() % 92001};
    int32_t t = consumedCPU;
    for (size_t i = 0; i < iters; i++)
      t += int32_t(t * 0x5DEECE66DLL + 0xBLL + (uint64_t)i and 0xFFFFFFFFFFFFLL);
    if (t == 42)
      consumedCPU += t;
  }

  //! Number of threads to resume by number of pushed.
  size_t resume_num(size_t n) {
    if (n == 0) return 0;
    static const size_t RESUME_PROB = 16;
    return random() % (n * RESUME_PROB) / RESUME_PROB;
  }

  //////////// DECREASE KEY ///////////////////

  //! Checks whether the index is a valid index of a queue.
  inline bool valid_index(int ind) {
    return ind >= 0 && ind < getNQ();
  }

  //! Update element if the new value is smaller that the value in the heap.
  //! The element *must* be in the heap.
  //! Heap *must* be locked.
  inline void update_elem(const value_t& val, Heap* heap) {
    static DecreaseKeyIndexer indexer;
    heap->heap.decrease_key(indexer, val);
  }

  //! Add element to the locked heap with index q_ind.
  //! The element may be in another heap.
  inline void push_elem(const value_t& val, Heap* heap) {
    static DecreaseKeyIndexer indexer;
    // indexer.cas_queue(val, q_ind, -1); // fails if the element was added to another queue
    heap->heap.push(indexer, val);
  }

public:
  static Galois::Statistic* deletedQ;
  static Galois::Statistic* addedQ;
  static Galois::Statistic* addedPush;
  static Galois::Statistic* addedPopLocal;
  static Galois::Statistic* addedPopFailure;
  static Galois::Statistic* wantedToDelete;
  static Galois::Statistic* wantedToAdd;
  static Galois::Statistic* reportNum;
  static Galois::Statistic* minNumQ;
  static Galois::Statistic* maxNumQ;
  static Galois::Statistic* suspendedNum;
  static Galois::Statistic* resumedNum;

  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

  enum DebugVectors {
    maxSizeDebugPush,
    minSizeDebugPush,
    failedNumDebugPush,
    successNumDebugPush,
    emptyNumDebugPop,
    failedNumDebugPop,
    successNumDebugPop,
//
//    elementsInQDebugPop,
//    elementsInQDebugPush,

    mockLast

  };

  std::vector<std::string> debugVectorNames = {
  "maxSizePush",
  "minSizePush",
  "failedNumDebugPush",
  "successNumDebugPush",
  "emptyNumDebugPop",
  "failedNumDebugPop",
  "successNumDebugPop",
  };

  std::vector<std::vector<std::vector<int>>> debugInfo;


  static const size_t pushIdS = 0;
  static const size_t pushIdF = 1;
  static const size_t curNQPush = pushIdF + 1;
  static const size_t pushRepId = curNQPush + 1;


  static const size_t popIdS = pushRepId + 1;
  static const size_t popIdF = popIdS + 1;
  static const size_t popIdE = popIdF + 1;
  static const size_t popIdENum = popIdE + 1;
  static const size_t popIdLF = popIdENum + 1;
  static const size_t popIdLS = popIdLF + 1;

  static const size_t curNQPop = popIdLS + 1;
  static const size_t popRepId = curNQPop + 1;
  static const size_t NUM = 12;


  std::unique_ptr<Runtime::LL::CacheLineStorage<std::array<size_t, NUM>>[]> threadLocalStats;



  AdaptiveMultiQueue() : nT(Galois::getActiveThreads()),
                         nQ(1), // C * nT/*C > 0 ? C * nT :*/ ),
                         suspend_array(std::make_unique<CondNode[]>(nT * CondC)),
                         maxQNum(C * nT * 4),
                         debugInfo(mockLast, std::vector<std::vector<int>>(nT, std::vector<int>()))
  {
    //std::atomic_init(&nQ, C * nT); // todo
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));

    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(C * nT * 4);
    threadLocalStats = std::make_unique<Runtime::LL::CacheLineStorage<std::array<size_t, NUM>>[]>(nT);

    for (size_t i = 0; i < nT; i++) {
      threadLocalStats[i].data.fill(0);
    }

    std::cout << "Queues: " << nQ << std::endl;

    for (size_t i = 0; i < C * nT * 4; i++) {
      heaps[i].data.heap.set_index(i);
    }

    initStatistic(deletedQ, "deletedQ");
    initStatistic(addedQ, "addedQ");
    initStatistic(addedPush, "addedQ");
    initStatistic(addedPopLocal, "addedQ");
    initStatistic(addedPopFailure, "addedQ");
    initStatistic(wantedToDelete, "wantedToDelete");
    initStatistic(wantedToAdd, "wantedToAdd");
    initStatistic(reportNum, "reportNum");
    initStatistic(minNumQ, "minNumQ");
    initStatistic(maxNumQ, "maxNumQ");
    initStatistic(suspendedNum, "suspendedNum");
    initStatistic(resumedNum, "resumedNum");
    *maxNumQ = getNQ();
    *minNumQ = getNQ();
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

  ~AdaptiveMultiQueue() {
//    std::string result_name;
//    std::ifstream name("result_name");
//    name >> result_name;
//    std::cout << result_name << std::endl;
//    name.close();
    auto exists = std::filesystem::exists("amq_stats.csv");
    std::ofstream out("amq_stats.csv" /*result_name*/, std::ios::app);
    if (!exists) {
      out << "threads,pushQ,popQ,s,f,e,segment_size,percent,resume_size,deleted,added,wantedToDelete," <<
          "wantedToAdd,reportNum,minQ,maxQ,suspendedNum,resumedNum" << std::endl;
    }
    out << nT << "," << PushChange::Q << "," << PopChange::Q << "," << PERCENT_F << "," << PERCENT_LF << ","
        << PERCENT_E << "," << REFRESH_SIZE << "," << PERCENT_S << "," << RESUME_SIZE;
    deleteStatistic(deletedQ, out);
    deleteStatistic(addedQ, out);
    deleteStatistic(addedPush, out);
    deleteStatistic(addedPopLocal, out);
    deleteStatistic(addedPopFailure, out);
    deleteStatistic(wantedToDelete, out);
    deleteStatistic(wantedToAdd, out);
    deleteStatistic(reportNum, out);
    deleteStatistic(minNumQ, out);
    deleteStatistic(maxNumQ, out);
    deleteStatistic(suspendedNum, out);
    deleteStatistic(resumedNum, out);
    out << std::endl;
    out.close();

    if constexpr (AMQ_DEBUG) {
//      auto tId = Galois::Runtime::LL::getTID();
      std::string suffix =
      "_C_" + std::to_string(C) +
      "_nT_" + std::to_string(nT) +
      "_nQ_" + std::to_string(nQ);
      std::ofstream pushF("pushStats" + suffix);
      pushF << "tId,success,failure,minQ,maxQ" << std::endl;
      for (int tId = 0; tId < nT; tId++) {
        for (size_t i = 0; i < debugInfo[successNumDebugPush][tId].size(); i++) {
          pushF << tId;
          for (DebugVectors eId: {successNumDebugPush, failedNumDebugPush, minSizeDebugPush, maxSizeDebugPush}) {
            pushF << "," << debugInfo[(int) eId][tId][i];
          }
          pushF << std::endl;
        }
      }
      pushF.close();

      std::ofstream popF("popStats" + suffix);
      popF << "tId,success,failure,empty" << std::endl;

      for (int tId = 0; tId < nT; tId++) {
        for (size_t i = 0; i < debugInfo[successNumDebugPop][tId].size(); i++) {
          popF << tId;
          for (DebugVectors eId: {successNumDebugPop, failedNumDebugPop, emptyNumDebugPop}) {
            popF << "," << debugInfo[(int) eId][tId][i];
          }
          popF << std::endl;
        }
      }
      popF.close();
    }
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef AdaptiveMultiQueue<T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, _concurrent, Blocking, PushChange, PopChange, Numa, Prior, PERCENT_F, PERCENT_LF, PERCENT_E, REFRESH_SIZE, PERCENT_S, RESUME_SIZE> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<_T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, Concurrent, Blocking, PushChange, PopChange, Numa, Prior, PERCENT_F, PERCENT_LF, PERCENT_E, REFRESH_SIZE, PERCENT_S, RESUME_SIZE> type;
  };

  //! Push a value onto the queue.
  void push(const value_type &val) {
    Heap* heap;
    int q_ind;

    do {
      q_ind = rand_heap();
      heap = &heaps[q_ind].data;
    } while (!heap->try_lock());

    heap->heap.push(val);
    heap->min.store(heap->heap.min().prior(), std::memory_order_release);
    heap->updateSize();
    heap->unlock();
  }

  size_t get_push_local(size_t old_local) {
    size_t change = random() % PushChange::Q;
    if (old_local >= getNQ() || change < PushChange::P)
      return rand_heap();
    return old_local;
  }

  bool deleteQueue(size_t expectedNQ) {
    *wantedToDelete += 1;
    if (expectedNQ == 1)
      return false;
    if (!adaptLock.try_lock())
      return false;
    size_t curNQ = getNQ();
    if (curNQ == 1 || expectedNQ != curNQ) {
      adaptLock.unlock();
      return false;
    }
    size_t idMin = 0;
    size_t szMin = SIZE_MAX;
    for (size_t i = 0; i < curNQ - 1; i++) {
      auto curSize = heaps[i].data.size.load(std::memory_order_acquire);
      if (szMin > curSize) {
        szMin = curSize;
        idMin = i;
      }
    }
    Heap* mergeH = &heaps[idMin].data;
    mergeH->lock();
    Heap* removeH = &heaps[curNQ - 1].data;
    removeH->lock();
//    while (removeH->heap.size() > 0) {
////      // todo: fix for decrease key
//      mergeH->heap.push(removeH->heap.extractMin()); // todo dont need to change min everytime
//    }

//    if constexpr (DecreaseKey) {
//      static DecreaseKeyIndexer indexer;
//      mergeH->heap.pushAllAndClear(removeH->heap, indexer);
//    } else {
    bool wasEmptyRemove = removeH->heap.empty();
    bool wasEmptyMerge = mergeH->heap.empty();
    mergeH->heap.pushAllAndClear(removeH->heap);
//    }
    *deletedQ += 1;

    minNumQ->setMin(curNQ - 1);

    // mark the heap empty
//    removeH->size.store(0, std::memory_order_release);

    const size_t newSize = mergeH->heap.size();
    mergeH->updateSize();
    mergeH->updateMin();

    removeH->updateSize();
    removeH->updateMin();

    if (wasEmptyRemove) empty_queues.fetch_sub(1, std::memory_order_acq_rel);
    nQ.store(curNQ - 1, std::memory_order_release);
    if (wasEmptyMerge && !wasEmptyRemove) empty_queues.fetch_sub(1, std::memory_order_acq_rel);
    removeH->unlock();
    mergeH->unlock();
    // std::cout << ">>>>>>>>>>>>>>>>>>> Deleted: " << nQ << std::endl;
    adaptLock.unlock();
    return true;
  }

  bool addQueue(size_t expectedNQ) {
//    return false;
    *wantedToAdd += 1;
    if (expectedNQ == maxQNum)
      return false;
    if (expectedNQ != getNQ())
      return false;
    if (!adaptLock.try_lock())
      return false;
    size_t curNQ = getNQ();
    if (curNQ == maxQNum || expectedNQ != curNQ) {
      adaptLock.unlock();
      return false;
    }
    size_t id = 0;
    size_t szVal = 0;
    for (size_t i = 0; i < curNQ; i++) {
      auto curVal = heaps[i].data.size.load(std::memory_order_acquire);
      if (szVal < curVal) {
        szVal = curVal;
        id = i;
      }
    }
    Heap* elemsH = &heaps[id].data;
    Heap* addH = &heaps[curNQ].data;
    elemsH->lock();
    addH->lock();

    nQ.store(curNQ + 1, std::memory_order_release);

    // todo: if empty
    *addedQ += 1;

    bool wasEmpty = elemsH->heap.empty();
    if constexpr (DecreaseKey) {
      static DecreaseKeyIndexer indexer;
      elemsH->heap.divideElems(addH->heap, indexer);
    } else {
      elemsH->heap.divideElems(addH->heap);
    }
    maxNumQ->setMax(curNQ + 1);

    elemsH->updateSize();
    elemsH->updateMin();

    addH->updateSize();
    addH->updateMin();


    if (!wasEmpty) {
      if (elemsH->heap.empty()) empty_queues.fetch_add(1, std::memory_order_acq_rel);
    }
    if (addH->heap.empty()) empty_queues.fetch_add(1, std::memory_order_acq_rel);
    addH->unlock();
    elemsH->unlock();
    // std::cout << ">>>>>>>>>>>>>>> Added: " << nQ << std::endl;
    adaptLock.unlock();
    return true;
  }

  // TODO: do i need to count local failure/success?
  void reportPush(size_t s, size_t f, size_t curNQ, size_t tId, size_t avgSize) {
//    if (tId != 0) return;
//    return;
//    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    std::array<size_t, NUM>& arr = threadLocalStats[tId].data;
    if (curNQ != arr[curNQPush]) {
      arr[curNQPush] = curNQ;
      arr[pushIdF] = 0;
      arr[pushIdS] = 0;
      arr[pushRepId] = 0;
    }
    arr[pushRepId]++;
    arr[pushIdS] += s;
    arr[pushIdF] += f;
    if (arr[pushRepId] == REFRESH_SIZE) {
      // f / (s + f) >= percent / 100
      // 100 * f >= percent (s + f)
      // TODO: is segment size needed?
      if (100 * arr[pushIdF] >= PERCENT_S * (arr[pushIdF] + arr[pushIdS])) {
//        size_t sizeSum = 0;
//        for (size_t i = 0; i < curNQ; i++) {
//          sizeSum += heaps[i].data.getSize();
//        }
//        if (avgSize >= RESUME_SIZE) {
        // the percent of failures is huge enough
        if (addQueue(curNQ)) *addedPush += 1;
//        }
      }
      arr[pushIdF] = 0;
      arr[pushIdS] = 0;
      arr[pushRepId] = 0;
    }
  }

  static const size_t LOCAL_POPPED = 0;
  static const size_t LOCAL_CHANGED = 1;
  static const size_t LOCAL_FAILED = 2;

  void reportPop(size_t s, size_t f, size_t e, size_t localStatus, size_t curNQ, size_t tId, size_t avgSize, size_t sizeN) {
//if (tId != 0) return;
    //    static thread_local size_t tId = Galois::Runtime::LL::getTID();
//   return;
    std::array<size_t, NUM>& arr = threadLocalStats[tId].data;
    if (curNQ != arr[curNQPop]) {
      arr[curNQPop] = curNQ;
      arr[popRepId] = 0;
      arr[popIdF] = 0;
      arr[popIdS] = 0;
      arr[popIdLF] = 0;
      arr[popIdLF] = 0;
      arr[popIdE] = 0;
      arr[popIdENum] = 0;
    }
    arr[popRepId]++;
    arr[popIdS] += s;
    arr[popIdF] += f;
    arr[popIdE] += e;
    arr[popIdENum] += sizeN;
    if (arr[popRepId] == REFRESH_SIZE) {
      if (localStatus == LOCAL_FAILED) {
        arr[popIdLF]++;
      } else if (localStatus == LOCAL_POPPED) {
        arr[popIdLS]++;
      } else if (localStatus == LOCAL_CHANGED) {
        // ???
        // :shrug:
      }
//      auto emptyNum = get_empty_queues();
      // empty / askedNum >= E / 100
      if (arr[popIdE] * 100 >= PERCENT_E * arr[popIdENum]) {
        deleteQueue(curNQ);
        // localFailure / (LF + LS) >= LF / 100
      } else  if (arr[popIdLF] > 0 && arr[popIdLF] * 100 >= PERCENT_LF * (arr[popIdLF] + arr[popIdLS])) {
        if (avgSize >= RESUME_SIZE) {
          if (addQueue(curNQ)) *addedPopLocal += 1;
        }
      } else if (arr[popIdF] * 100 >= PERCENT_F * (arr[popIdF] + arr[popIdS])) {
//        size_t sizeSum = 0;
//        for (size_t i = 0; i < curNQ; i++) {
//          sizeSum += heaps[i].data.getSize();
//        }
        if (avgSize >= RESUME_SIZE) {
          if (addQueue(curNQ)) *addedPopFailure += 1;
        }
      }
      arr[popRepId] = 0;
      arr[popIdF] = 0;
      arr[popIdS] = 0;
      arr[popIdLF] = 0;
      arr[popIdLF] = 0;
      arr[popIdE] = 0;
      arr[popIdENum] = 0;
    }
  }



  inline void reportStat(size_t s, size_t f, size_t e, size_t flb, size_t ls) {
    *reportNum += 1;
    static thread_local size_t curQ = 0;

    if constexpr (AMQ_DEBUG) {
      // pop is here
      static thread_local size_t tId = Galois::Runtime::LL::getTID();
      debugInfo[successNumDebugPop][tId].push_back(s);
      debugInfo[failedNumDebugPop][tId].push_back(f);
      debugInfo[emptyNumDebugPop][tId].push_back(e);
      return;
    }


    static const size_t segmentSize = REFRESH_SIZE;
    static thread_local size_t statInd = 0;
    static thread_local int64_t windowSumF = 0;
    static thread_local int64_t windowSumS = 0;
    static thread_local int64_t windowSumE = 0;

    if (curQ != getNQ()) {
      curQ = getNQ();
      windowSumF = 0;
      windowSumS = 0;
      windowSumE = 0;
      statInd = 0;
    }

    windowSumF += f;
    windowSumS += s;
    windowSumE += e;

    statInd++;
    if (statInd >= segmentSize) {
      const int64_t addSum = windowSumF * PERCENT_LF;
      const int64_t deleteSum = windowSumE * PERCENT_E + windowSumS * PERCENT_F;
      const double allsum = addSum + deleteSum;

      static const double percent = PERCENT_S / 100.0;
      if (windowSumF >= segmentSize && addSum / allsum > percent) {
        addQueue(curQ);
      } else if ((windowSumS + windowSumF) >= segmentSize && deleteSum / allsum > percent) {
        deleteQueue(curQ);
      }
      windowSumF = 0;
      windowSumS = 0;
      windowSumE = 0;
      statInd = 0;
    }
  }

  // Throw a coin until the queue should be changed.
  // At least one.
  size_t numToPush(size_t limit) {
    for (size_t i = 1; i < limit; i++) {
      if ((random() % PushChange::Q) < PushChange::P) {
        return i;
      }
    }
    return limit;
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    if (b == e) return 0;
    if constexpr (Blocking) {
      // Adapt to Galois abort policy.
      if (no_work)
        no_work = false;
    }

    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    // local queue
    static thread_local size_t local_q = rand_heap();  // only one queue in the beginning
    // todo: for numa is1Node(tId) ? map1Node(random() % (C * node1Cnt())): map2Node(random() % (C * node2Cnt()));

    size_t curNQ = getNQ();
    int npush = 0;
    Heap* heap = nullptr;
    int total = std::distance(b, e);
    size_t qToLock = local_q < curNQ ? local_q : rand_heap(curNQ);

    // Statistics
    size_t failure = 0;
    size_t success = 0;

    size_t maxSize = 0;
    size_t minSize = SIZE_MAX;

    size_t sizeSum = 0;
    size_t sizeNum = 0;

    while (b != e) {
      auto batchSize = numToPush(total);
      heap = &heaps[qToLock].data;

      while (true) {
        while (!heap->try_lock()) {
          qToLock = rand_heap(curNQ);
          heap = &heaps[qToLock].data;
          failure++;
        }
        auto newNQ = getNQ();
        if (curNQ != newNQ) {
          failure = 0;
          success = 0;
          curNQ = newNQ;
        }
        if (heap->heap.qInd >= newNQ) {
          // The queue was deleted
          heap->unlock();
          qToLock = rand_heap();
          heap = &heaps[qToLock].data;
          continue;
        } else {
          break;
        }
      }
      success++;
      if constexpr (Blocking) {
        if (heap->heap.empty())
          empty_queues.fetch_sub(1, std::memory_order_acq_rel);
      }

      maxSize = std::max(maxSize, heap->heap.size());
      minSize = std::min(minSize, heap->heap.size());

      for (size_t i = 0; i < batchSize; i++) {
        heap->heap.push(*b++);
        npush++;
        total--;
      }
      heap->updateMin();
      heap->updateSize();

//      sizeSum += heap->getSize();
//      sizeNum++;

      heap->unlock();
      if (total > 0) {
        qToLock = rand_heap();
      }
    }
//    TODO: NUMA
//    if (!(is1Node(tId) ^ is1Node(qToLock / C))) {
//      local_q = qToLock;
//    }
    local_q = qToLock;

    if constexpr (Blocking) {
      if (maxSize >= RESUME_SIZE) {
        // todo: is there a need to resume more than one?
        // todo: is there a need to add a new queue immediately?
        resume(1);
      }
    }
    if constexpr (AMQ_DEBUG) {
      debugInfo[maxSizeDebugPush][tId].push_back(maxSize);
      debugInfo[minSizeDebugPush][tId].push_back(minSize);
      debugInfo[successNumDebugPush][tId].push_back(success);
      debugInfo[failedNumDebugPush][tId].push_back(failure);
    }
//    reportStat(success, failure, empty, failed_local_busy, local_success);
    reportPush(success, failure, curNQ, tId, sizeSum/ sizeNum);
    return npush;
  }


  //! Push initial range onto the queue.
  //! Called with the same b and e on each thread.
  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  bool isFirstLess(Prior const& v1, Prior const& v2) {
    if (Heap::isUsedMin(v1)) {
      return false;
    }
    if (Heap::isUsedMin(v2)) {
      return true;
    }
    return v1 < v2;
  }

  //! Pop a value from the queue.
  Galois::optional<value_type> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();

    static const size_t SLEEPING_ATTEMPTS = 8;
    const size_t RANDOM_ATTEMPTS = 4;// nT < 4 ? 1 : 4;

    static thread_local size_t local_q = rand_heap();

    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;
    Prior i_min = 0;
    Prior j_min = 0;

    size_t localStatus = 0;

    Galois::optional<value_type> result;

    size_t curNQ = getNQ();

    size_t change = random() % PopChange::Q;
    if (local_q < curNQ && change >= PopChange::P) {
      heap_i = &heaps[local_q].data;
      if (heap_i->try_lock()) {
        if (!heap_i->heap.empty()) {
          return extract_min(heap_i, 1, 0, 0, LOCAL_POPPED, curNQ, tId, heap_i->getSize(), 1);
        }
        localStatus = LOCAL_CHANGED;
        heap_i->unlock();
      } else {
        localStatus = LOCAL_FAILED;
      }
    }

    size_t failure = 0;
    size_t success = 0;
    size_t empty   = 0;

    size_t emptyNum = 0;

    size_t sumSize = 0;
    size_t sizeN = 0;

    static const size_t BLOCKING_ITERS_LIMIT = 1024;
    while (true) {
      if constexpr (Blocking) {
        if (no_work) return result;
      }
      size_t blocking_iters = 32;
      do {
        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          while (true) {
            i_ind = rand_heap(curNQ);
            heap_i = &heaps[i_ind].data;

            j_ind = rand_heap(curNQ);
            heap_j = &heaps[j_ind].data;

            if (i_ind == j_ind && curNQ > 1)
              continue;
            sizeN += 2;
            size_t size1 = heap_i->getSize();
            size_t size2 = heap_j->getSize();

            sumSize += size1 + size2;
            if (size1 == 0) {
              emptyNum++;
            }
            if (size2 == 0) {
              emptyNum++;
            }

            i_min = heap_i->getMin();
            j_min = heap_j->getMin();
            if (isFirstLess(j_min, i_min)) {
              std::swap(i_ind, j_ind);
              std::swap(i_min, j_min);
              std::swap(heap_i, heap_j);
            }

//            if (i_ind != j_ind && Heap::isUsedMin(j_min)) {
//              empty++;
//            }
            if (Heap::isUsedMin(i_min)) {
//              empty++;
              break;
            }
            if (heap_i->try_lock()) {
              auto newNQ = getNQ();
              if  (curNQ != newNQ) {
                curNQ = newNQ;
                failure = 0;
                success = 0;
                empty = 0;
                localStatus = LOCAL_CHANGED; // TODO to make no effect
              }
              if (heap_i->heap.qInd >= newNQ) {
                // The queue was deleted
                heap_i->unlock();
                continue;
              }
              success++;
              break;
            } else failure++;
          }
          if (Heap::isUsedMin(i_min)) {
            continue;
          }

          if (!heap_i->heap.empty()) {
            local_q = i_ind;
            return extract_min(heap_i, success, failure, emptyNum, localStatus, curNQ, tId, sumSize / sizeN, sizeN);
          } else {
            empty++;
            heap_i->unlock();
          }
        }
//        if constexpr (Blocking) {
//          active_waiting(64); //blocking_iters);
//          if (blocking_iters < BLOCKING_ITERS_LIMIT)
//            blocking_iters *= 2;
//        }
        // TODO when blocking
//        localStatus = LOCAL_CHANGED; // todo not really appropriate
//        active_waiting(64); //blocking_iters);
      } while (Blocking && get_empty_queues() != getNQ());

      reportPop(success, failure, emptyNum, localStatus, curNQ, tId, sumSize / sizeN, sizeN);
      if constexpr (Blocking) {
//        active_waiting(random() % 128 + 32);
        if (empty_queues == getNQ()) {
          suspend();
        }
      } else {
        return result;
      }
    }
  }
};


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::deletedQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::addedQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::addedPush;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::addedPopLocal;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::addedPopFailure;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::wantedToDelete;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::wantedToAdd;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::reportNum;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::minNumQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::maxNumQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::suspendedNum;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Numa,
typename Prior,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB,
size_t RESUME_SIZE>
Statistic* AdaptiveMultiQueue<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Numa, Prior,
S, F, E, WINDOW_SIZE, PROB, RESUME_SIZE>::resumedNum;


} // namespace WorkList
} // namespace Galois

#endif // ADAPTIVE_MULTIQUEUE_H
