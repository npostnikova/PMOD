#ifndef ADAPTIVE_MULTIQUEUE_H
#define ADAPTIVE_MULTIQUEUE_H

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
template <typename  T, typename Comparer, size_t D = 4, typename Prior = unsigned long>
struct LockableHeap {
  DAryHeap<T, Comparer, 4> heap;
  // todo: use atomic
  std::atomic<Prior> min;
  static T usedT;

  LockableHeap() : min(usedT.prior()) {}

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

  static bool isUsed(T const& value) {
    return value == usedT;
  }

  static bool isUsedMin(Prior const& value) {
    return value == usedT.prior();
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
typename Prior = unsigned long,
size_t ChunkPop = 0,
size_t S = 1, // todo
size_t F = 1,
size_t E = 1,
size_t SEGMENT_SIZE = 32,
size_t PERCENT = 90>
class MultiQueueProbLocal {
private:
  typedef T value_t;
  typedef LockableHeap<T, Comparer, 4, Prior> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const int nQ;


  std::unique_ptr<Runtime::LL::CacheLineStorage<std::vector<value_t>>[]> popLocal;
  // std::atomic<int> nQ = {0}; todo atomic for adaptive!!!
  //size_t nQ; // todo
  //! Maximum element of type `T`.
  // T maxT;
  //! The number of queues is changed under the mutex.
  Runtime::LL::PaddedLock<Concurrent> adaptLock;
  const size_t maxQNum;

  // todo int?
  int getNQ() {
    return nQ; // nQ.load(std::memory_order_acquire);
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

  inline size_t rand_heap() {
    return random() % getNQ();
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap, std::vector<value_t>& popped_v, size_t success, size_t failure, size_t empty) {
    auto result = getMin(heap);

    for (size_t i = 0; i < PopChange::Q - 1; i++) {
      if (!heap->heap.empty()) {
        popped_v.push_back(getMin(heap));
      } else break;
    }
    std::reverse(popped_v.begin(), popped_v.end());

//    if (heap->heap.size() > 0)
    heap->min.store(!heap->heap.empty() ? heap->heap.min().prior() : heap->usedT.prior(), std::memory_order_release);
//    heap->size.store(heap->heap.size(), std::memory_order_release);
    heap->unlock();
//    reportStat(success, failure, empty);
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

  //! Suspends if the thread is not the last.
  void suspend() {
    while (empty_queues == getNQ()) {
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
    for (size_t i = 0; i < suspend_size() && cnt != 0; i++) {
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
    auto suspended_num = suspended.fetch_add(1);
    if (suspended_num + 1 == nT) {
      node.cond_mutex.unlock();
      suspended.fetch_sub(1);
      if (empty_queues < getNQ())
        return true;
      resume_all();
      return true;
    }
    node.state = CondNode::SUSPENDED;
    std::unique_lock<std::mutex> lk(node.cond_mutex, std::adopt_lock);
    node.cond_var.wait(lk, [&node] { return node.state == CondNode::RESUMED; });
    node.state = CondNode::FREE;
    lk.unlock();
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
    suspended.fetch_sub(1);
    return true;
  }

  //! Resumes if the cell is busy with a suspended thread. Blocking.
  void blocking_resume(size_t id) {
    CondNode& node = suspend_array[id];
    if (node.state != CondNode::SUSPENDED)
      return;
    std::lock_guard<std::mutex> guard(node.cond_mutex);
    if (node.state == CondNode::SUSPENDED) {
      suspended.fetch_sub(1);
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
  static Galois::Statistic* wantedToDelete;
  static Galois::Statistic* wantedToAdd;
  static Galois::Statistic* reportNum;
  static Galois::Statistic* minNumQ;
  static Galois::Statistic* maxNumQ;

  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

  MultiQueueProbLocal() : nT(Galois::getActiveThreads()), nQ(C > 0 ? C * nT : 1), suspend_array(std::make_unique<CondNode[]>(nT * CondC)), maxQNum(nT * 4) {
    //std::atomic_init(&nQ, C * nT); // todo
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));

    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(C * nT);

    popLocal = std::make_unique<Runtime::LL::CacheLineStorage<std::vector<value_t >>[]>(nT);

    std::cout << "Queues: " << nQ << std::endl;

    for (size_t i = 0; i < C * nT; i++) {
      heaps[i].data.heap.set_index(i);
    }

    initStatistic(deletedQ, "deletedQ");
    initStatistic(addedQ, "addedQ");
    initStatistic(wantedToDelete, "wantedToDelete");
    initStatistic(wantedToAdd, "wantedToAdd");
    initStatistic(reportNum, "reportNum");
    initStatistic(minNumQ, "minNumQ");
    initStatistic(maxNumQ, "maxNumQ");
    *maxNumQ = getNQ();
    *minNumQ = getNQ();
  }

  void deleteStatistic(Galois::Statistic*& st, std::ofstream& out) {
    if (st != nullptr) {
      if (st->getStatname().find("min") != std::string::npos) {
        out << getMinVal(st) << "\t";
      } else if (st->getStatname().find("max") != std::string::npos) {
        out << getMaxVal(st) << "\t";
      } else {
        out << getStatVal(st) << "\t";
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

  ~MultiQueueProbLocal() {
    std::string result_name;
    std::ifstream name("result_name");
    name >> result_name;
    std::cout << result_name << std::endl;
    name.close();

    std::ofstream out(result_name, std::ios::app);
    deleteStatistic(deletedQ, out);
    deleteStatistic(addedQ, out);
    deleteStatistic(wantedToDelete, out);
    deleteStatistic(wantedToAdd, out);
    deleteStatistic(reportNum, out);
    deleteStatistic(minNumQ, out);
    deleteStatistic(maxNumQ, out);
    out.close();
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef MultiQueueProbLocal<T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, _concurrent, Blocking, PushChange, PopChange, Prior, ChunkPop, S, F, E, SEGMENT_SIZE, PERCENT> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef MultiQueueProbLocal<_T, Comparer, C, DecreaseKey, DecreaseKeyIndexer, Concurrent, Blocking, PushChange, PopChange, Prior, ChunkPop, S, F, E, SEGMENT_SIZE, PERCENT> type;
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
    heap->unlock();
  }

  size_t get_push_local(size_t old_local) {
    size_t change = random() % PushChange::Q;
    if (old_local >= getNQ() || change < PushChange::P)
      return rand_heap();
    return old_local;
  }

  void deleteQueue(size_t expectedNQ) {
    *wantedToDelete += 1;
    if (!adaptLock.try_lock())
      return;
    size_t curNQ = getNQ();
    if (curNQ == 1 || expectedNQ != curNQ) {
      adaptLock.unlock();
      return;
    }
    size_t idMin = 0;
    size_t szMin = SIZE_MAX;
    for (size_t i = 0; i < curNQ - 1; i++) {
//      if (szMin > heaps[i].data.size.load(std::memory_order_acquire)) {
//        szMin = heaps[i].data.size.load(std::memory_order_acquire);
//        idMin = i;
//      }
    }
    Heap* mergeH = &heaps[idMin].data;
    mergeH->lock();
    Heap* removeH = &heaps[curNQ - 1].data;
    removeH->lock();
//    while (removeH->heap.size() > 0) {
//      // todo: fix for decrease key
//      mergeH->heap.push(removeH->heap.extractMin());
//    }

    if constexpr (DecreaseKey) {
      static DecreaseKeyIndexer indexer;
      mergeH->heap.pushAllAndClear(removeH->heap, indexer);
    } else {
      mergeH->heap.pushAllAndClear(removeH->heap);
    }
    *deletedQ += 1;

    minNumQ->setMin(curNQ - 1);

    // mark the heap empty
//    removeH->size.store(0, std::memory_order_release);

    const size_t newSize = mergeH->heap.size();
//    mergeH->size.store(newSize, std::memory_order_release);

//    if (newSize > 0)
//      mergeH->min = mergeH->heap.min().prior();

    // todo nQ.fetch_sub(1, std::memory_order_acq_rel);
    removeH->unlock();
    mergeH->unlock();
    // std::cout << ">>>>>>>>>>>>>>>>>>> Deleted: " << nQ << std::endl;
    adaptLock.unlock();
  }

  void addQueue(size_t expectedNQ) {
    *wantedToAdd += 1;
    if (!adaptLock.try_lock())
      return;
    size_t curNQ = getNQ();
    if (curNQ == maxQNum || expectedNQ != curNQ) {
      adaptLock.unlock();
      return;
    }
    size_t id = 0;
    size_t szVal = 0;
    for (size_t i = 0; i < curNQ; i++) {
//      if (szVal < heaps[i].data.size.load(std::memory_order_acquire)) {
//        szVal = heaps[i].data.size.load(std::memory_order_acquire);
//        id = i;
//      }
    }
    Heap* elemsH = &heaps[id].data;
    Heap* addH = &heaps[curNQ].data;
    elemsH->lock();
    addH->lock();

    // todo: if empty
    *addedQ += 1;

    if constexpr (DecreaseKey) {
      static DecreaseKeyIndexer indexer;
      elemsH->heap.divideElems(addH->heap, indexer);
    } else {
      elemsH->heap.divideElems(addH->heap);
    }
    maxNumQ->setMax(curNQ + 1);

    const size_t elemsSize = elemsH->heap.size();
//    elemsH->size.store(elemsSize, std::memory_order_release);
//    if (elemsSize > 0)
//      elemsH->min = elemsH->heap.min().prior();

    const size_t addSize = addH->heap.size();
//    addH->size.store(addSize, std::memory_order_release);
//    if (addSize > 0)
//      addH->min = addH->heap.min().prior();

    // todo nQ.fetch_add(1, std::memory_order_acq_rel);
    addH->unlock();
    elemsH->unlock();
    // std::cout << ">>>>>>>>>>>>>>> Added: " << nQ << std::endl;
    adaptLock.unlock();
  }

  inline void reportStat(size_t s, size_t f, size_t e) {
    // *reportNum += 1;
    return;
    static thread_local size_t curQ = 0;

    static const size_t segmentSize = SEGMENT_SIZE;
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
      const int64_t addSum = windowSumF * F;
      const int64_t deleteSum = windowSumE * E + windowSumS * S;
      const double allsum = addSum + deleteSum;

      static const double percent = PERCENT / 100.0;
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


  size_t numToPush(size_t limit) {
    // todo min is one as we trow a coin a least ones to get the local queue
    for (size_t i = 1; i < limit; i++) {
      if ((random() % PushChange::Q) < PopChange::P) {
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
//    const size_t chunk_size = 8;

    // local queue
    static thread_local size_t local_q = rand_heap();

    int npush = 0;
    Heap* heap = nullptr;

    size_t failure = 0;
    size_t success = 0;
    size_t empty   = 0; // empty doesn't matter in push


    int total = std::distance(b, e);

    while (b != e) {
      auto batchSize = numToPush(total);
      heap = &heaps[local_q].data;

      while (!heap->try_lock()) {
        local_q = rand_heap();
        heap = &heaps[local_q].data;
        failure++;
      }
      success++;

      for (size_t i = 0; i < batchSize; i++) {
        heap->heap.push(*b++);
        npush++;
        total--;
      }

      heap->min.store(heap->heap.min().prior(), std::memory_order_release);
      heap->unlock();
      if (total > 0) {
        local_q = rand_heap();
      }
    }

//      if constexpr (Blocking) {
//        if (heap->heap.size() == 1)
//          empty_queues.fetch_sub(1);
//      }
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

    std::vector<value_type>& popped_v = popLocal[tId].data;
    if (!popped_v.empty()) {
      auto ret = popped_v.back();
      popped_v.pop_back();
      return ret;
    }

    static const size_t SLEEPING_ATTEMPTS = 8;
    const size_t RANDOM_ATTEMPTS = 4;// nT < 4 ? 1 : 4;

    size_t failure = 0;
    size_t success = 0;
    size_t empty   = 0;

    Galois::optional<value_type> result;
    Heap* heap_i = nullptr;
    Heap* heap_j = nullptr;
    size_t i_ind = 0;
    size_t j_ind = 0;

    size_t curNQ;
    size_t indexToLock = -1;
    while (true) {
      if constexpr (Blocking) {
        if (no_work) return result;
      }
      do {
        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          while (true) {
            i_ind = rand_heap();
            heap_i = &heaps[i_ind].data;

            j_ind = rand_heap();
            heap_j = &heaps[j_ind].data;

            if (i_ind == j_ind && nQ /** todo curNq*/ > 1)
              continue;
            if (isFirstLess(heap_j->getMin(), heap_i->getMin())) {
              i_ind = j_ind;
              heap_i = heap_j;
            }
            indexToLock = i_ind;

            if (heap_i->try_lock())
              break;
            else
              failure++;
          }
          success++;

          if (!heap_i->heap.empty()) {
            return extract_min(heap_i, popped_v, success, failure, empty);
          } else {
            empty++;
            heap_i->unlock();
          }
        }
//        for (size_t k = 0; k < nQ /*curNQ*/; k++) {
//          if (k == indexToLock) continue;
//          heap_i = &heaps[k].data;
//          if (LockableHeap<T, Comparer>::isUsedMin(heap_i->getMin()))
//            // todo: does it make sense? or should we just lock?
//            // when not relaxed, it makes sense....
//            continue;
//          if (heap_i->try_lock()) {
//            if (heap_i->heap.size() > 0) {
//              local_q = k;
//              return extract_min(heap_i, popped_v, success, failure, empty);
//            } else {
//              //empty++; todo should it be counted
//              heap_i->unlock();
//            }
//          }
//        }
      } while (Blocking && empty_queues != getNQ());

      if constexpr (!Blocking) {
        reportStat(success, failure, empty);
        return result;
      }
      for (size_t k = 0, iters = 32; k < SLEEPING_ATTEMPTS; k++, iters *= 2) {
        if (empty_queues != getNQ() || suspended + 1 == nT) // todo: getNQ?
          break;
        active_waiting(iters);
      }
      suspend();
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
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::deletedQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::addedQ;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::wantedToDelete;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::wantedToAdd;


template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::reportNum;
template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::minNumQ;

template<typename T,
typename Comparer,
size_t C,
bool DecreaseKey ,
typename DecreaseKeyIndexer,
bool Concurrent,
bool Blocking,
typename PushChange,
typename PopChange,
typename Prior,
size_t ChunkPop,
size_t S,
size_t F,
size_t E,
size_t WINDOW_SIZE,
size_t PROB>
Statistic* MultiQueueProbLocal<T, Comparer, C,
DecreaseKey, DecreaseKeyIndexer, Concurrent,
Blocking, PushChange, PopChange, Prior, ChunkPop,
S, F, E, WINDOW_SIZE, PROB>::maxNumQ;


} // namespace WorkList
} // namespace Galois

#endif // ADAPTIVE_MULTIQUEUE_H
