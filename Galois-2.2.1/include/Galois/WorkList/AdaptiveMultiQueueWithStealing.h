//
// Created by nastya on 15.12.2020.
//

#ifndef GALOIS_ADAPTIVEMULTIQUEUEWITHSTEALING_H
#define GALOIS_ADAPTIVEMULTIQUEUEWITHSTEALING_H

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


template <typename  T, typename Comparer, size_t D = 4>
struct LockableHeapWithStealing {
  DAryHeap<T, Comparer, D> heap;
  T min;
  // version mod 2 = 0  -- element is stolen
  // version mod 2 = 1  -- can steal
  std::atomic<size_t> version;
  std::atomic<unsigned long> prior;
  Comparer cmp;

  static T usedT;

  static bool isUsed(T const& element) {
    return element == usedT;
  }

  static bool isPriorUsed(unsigned long const& element) {
    return element == usedT.prior();
  }

  // the heap is locked
  void updatePrior() {
    prior.store(heap.size() > 0? heap.min().prior() : usedT.prior(), std::memory_order_release);
  }

  LockableHeapWithStealing(): min(usedT), prior(usedT.prior()), version(0) {}

  size_t getVersion() {
    return version.load(std::memory_order_acquire);
  }

  T getMin() {
//    while (true) {
    auto v1 = getVersion();
    if (v1 % 2 == 0) {
      return usedT;
    }
    auto val = min;
    auto v2 = getVersion();
    if (v1 == v2) {
      return val;
    }
    return usedT; // is the version was changed, somebody stole the element
//    }
  }

  T steal() {
    while (true) {
      auto v1 = getVersion();
      if (v1 % 2 == 0) {
        return usedT;
      }
      auto val = min;
      if (version.compare_exchange_weak(v1, v1 + 1, std::memory_order_acq_rel)) {
        return val;
      }
    }
  }

  void writeMin(T const& val) {
    min = val;
    version.fetch_add(1, std::memory_order_acq_rel);
  }


  unsigned long getPrior() {
    return prior.load(std::memory_order_acquire);
  }

  T getRealMin() {
    if (heap.empty()) {
      return getMin();
    }
    auto atomicMin = getMin();
    auto localMin = heap.min();
    return cmp(localMin, atomicMin) ? atomicMin : localMin;
  }

  // todo its not real
  T extractRealMin() {
    if (heap.empty()) {
      return steal(); // min.exchange(usedT, std::memory_order_acq_rel); // todo should we load first?
    }
    auto atomicMin = getMin();
    auto localMin = heap.extractMin();
    if (isUsed(atomicMin)) {
      if (heap.size() > 0) {
        writeMin(heap.extractMin());
//        min.store(heap.extractMin(), std::memory_order_release);
      }
      return localMin;
    }
    if (cmp(localMin, atomicMin)) { // local >(=?) atomic
      auto stolen = steal();
      if (!isUsed(stolen)) {
        writeMin(localMin);
        return stolen;
      }
      if (!heap.empty()) {
        writeMin(heap.extractMin());
      }
    }
    return localMin;
  }

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

private:
  Runtime::LL::PaddedLock<true> _lock;
};


template<typename T,
typename Compare,
size_t D>
T LockableHeapWithStealing<T, Compare, D>::usedT;

template<typename T,
typename Comparer,
size_t C = 2,
bool Concurrent = true,
typename ChangeLocal = Prob<1, 1>>
class AdaptiveMultiQueueWithStealing {
private:
  typedef T value_t;
  typedef LockableHeapWithStealing<T, Comparer> Heap;
  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  //! Total number of threads.
  const size_t nT;
  //! Number of queues.
  const int nQ;

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

  inline size_t rand_heap() {
    return random() % nQ;
  }

  //! Extracts minimum from the locked heap.
  Galois::optional<value_t> extract_min(Heap* heap) {
    return heap->heap.extractMin();
  }

public:

  AdaptiveMultiQueueWithStealing() : nT(Galois::getActiveThreads()), nQ(C > 0 ? C * nT : 1) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    std::cout << "Queues: " << nQ << std::endl;

    for (size_t i = 0; i < nQ; i++) {
      heaps[i].data.heap.set_index(i);
    }
  }

  //! T is the value type of the WL.
  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef AdaptiveMultiQueueWithStealing<T, Comparer, C, _concurrent, ChangeLocal> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueueWithStealing<_T, Comparer, C, Concurrent, ChangeLocal> type;
  };

  //! Push a value onto the queue.
  void push(const value_type &val) {
    throw "shouldn't be used";
  }

  //! Push a range onto the queue.
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    if (b == e) return 0;

    size_t qId = 0;
    Heap* heap = nullptr;
    int npush = 0;

    int total = std::distance(b, e);

    qId = localQ(false);
    while (b != e) {
      auto batchSize = numToPush(total);
      heap = &heaps[qId].data;
      for (size_t i = 0; i < batchSize; i++) {
        heap->heap.push(*b++);
        npush++;
        total--;
      }
      if (Heap::isUsed(heap->getMin())) {
        heap->writeMin(heap->heap.extractMin());
      }
      if (total > 0) {
        qId = localQ(true);
      }
    }
    return npush;
  }


  bool isFirstLess(unsigned long const& v1, unsigned long const& v2) {
    if (LockableHeapWithStealing<T, Comparer>::isPriorUsed(v1)) {
      return false;
    }
    if (LockableHeapWithStealing<T, Comparer>::isPriorUsed(v2)) {
      return true;
    }
    return v1 < v2;
  }

  value_t steal(size_t localId, size_t attempts) {
    for (size_t i = 0; i < attempts; i++) {
      auto id = rand_heap();
      if (id == localId)
        continue;
      auto stolen = heaps[id].data.steal();
      if (!Heap::isUsed(stolen)) {
        return stolen;
      }
    }
    return Heap::usedT;
  }

  size_t numToPush(size_t limit) {
    // todo min is one as we trow a coin a least ones to get the local queue
    for (size_t i = 1; i < limit; i++) {
      if ((random() % ChangeLocal::Q) < ChangeLocal::P) {
        return i;
      }
    }
    return limit;
  }

  size_t localQ(bool change, bool hell = false) {
    static thread_local int qId = -1;
    if (hell) {
      for (size_t i = 0; i < nQ; i++) {
        if (i == qId) continue;
        if (heaps[i].data.try_lock()) {
          if (!Heap::isUsed(heaps[i].data.getRealMin())) {
            if (qId != -1) {
              heaps[qId].data.updatePrior();
              heaps[qId].data.unlock();
            }
            qId = i;
            return i;
          }

          heaps[i].data.updatePrior();
          heaps[i].data.unlock();
        }
      }
      return qId;
    }
    if (change || (random() % ChangeLocal::Q) < ChangeLocal::P || qId == -1) {
      size_t i_ind = 0;
      size_t j_ind = 0;
      if (qId != -1) {
        heaps[qId].data.updatePrior();
        heaps[qId].data.unlock();
      }
      for (size_t k = 0; k < 8; k++) {
        i_ind = rand_heap();
        auto heap_i = &heaps[i_ind].data;

        j_ind = rand_heap();
        auto heap_j = &heaps[j_ind].data;

        if (i_ind == j_ind && nQ > 1) continue;

        if (qId == i_ind || isFirstLess(heap_j->getPrior(), heap_i->getPrior())) {
          if (random() % 32 > 0) {
            std::swap(i_ind, j_ind);
            std::swap(heap_i, heap_j);
          }
        }
        if (qId == i_ind) continue;
        if (heap_i->try_lock()) {
          qId = i_ind;
          goto locked;
        }

//        if (qId == j_ind) continue;
//        if (heap_j->try_lock()) {
//          qId = j_ind;
//          goto locked;
//        }
      }
      for (size_t i = 0; i < 8; i++) {
        i = rand_heap();
        if (heaps[i].data.try_lock()) {
          qId = i;
          return i;
        }
      }

      while (true) {
        for (size_t i = 0; i < nQ; i++) {
          if (heaps[i].data.try_lock()) {
            qId = i;
            return i;
          }
        }
      }
    }
    locked:
    return qId;
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
    const size_t RANDOM_ATTEMPTS = nT < 4 ? 1 : 4;
    const size_t ATTEMPTS = 4;

    Galois::optional<value_type> result;
    Heap *heap;
//    static thread_local int valid = 0;

    for (size_t a = 0; a < ATTEMPTS; a++) {
      // Force changing local queue when it's not the first attempt.
      // I tried to steal for a long time, so I don't want to fill the queue any more.
      size_t qId = localQ(a > 0, a + 1 == ATTEMPTS); //, a + 1 == ATTEMPTS);
      heap = &heaps[qId].data;
      auto localMin = heap->extractRealMin();
      if (!Heap::isUsed(localMin)) {
//        valid = 0;
        return localMin;
      }
      // the heap is empty
      auto stolen = steal(qId, numToPush(ChangeLocal::Q)); // it tries for a few times inside
      if (!Heap::isUsed(stolen)) {
//        valid = 0;
        return stolen;
      }
    }
//    valid++;
    return result;
  }
};

} // namespace WorkList
} // namespace Galois


#endif //GALOIS_ADAPTIVEMULTIQUEUEWITHSTEALING_H
