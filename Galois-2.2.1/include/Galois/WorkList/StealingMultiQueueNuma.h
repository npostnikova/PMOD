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
size_t StealProb = 8,
size_t StealBatchSize = 8,
size_t LOCAL_NUMA_W,
bool Concurrent = true,
typename Container = StealDAryHeapHaate<T, Comparer, 4, StealBatchSize>
>
class StealingMultiQueueNuma {
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

  const size_t C = 1;
  const size_t nT = nQ;
#include "MQOptimized/NUMA.h"

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
  StealingMultiQueueNuma() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    for (size_t i = 0; i < nQ; i++) {
      heaps[i].data.set_id(i);
    }
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
    typedef StealingMultiQueueNuma<T, Comparer, StealProb, StealBatchSize, LOCAL_NUMA_W, _concurrent, Container> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueueNuma<_T, Comparer, StealProb, StealBatchSize, LOCAL_NUMA_W, Concurrent, Container> type;
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
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); 
    Heap* heap = &heaps[tId].data;
    return heap->pushRange(b, e);
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); 

    auto& buffer = stealBuffers[tId].data;
    if (!buffer.empty()) {
      auto val = buffer.back();
      buffer.pop_back();
      Heap& local = heaps[tId].data;
      bool useless = false;
      auto curMin = local.getMin(useless);
      if (Heap::isUsed(curMin) /**&& cmp(curMin, heap[0])*/ && !local.heap.empty()) { 
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

}
}

#endif //GALOIS_SMQ_NUMA_H
