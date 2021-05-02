#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

#include "StealingQueue.h"

namespace smq {

template<typename T = int,
typename Compare = std::greater<int>,
size_t D = 4,
size_t STEAL_NUM = 8>
struct StealDAryHeap {

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

  StealDAryHeap(): version(0) {
    for (size_t i = 0; i < STEAL_NUM; i++) {
      mins[i] = usedT;
    }
    //memset(reinterpret_cast<void*>(&usedT), 0xff, sizeof(usedT));
    //min.store(usedT, std::memory_order_release);
  }

  bool isUsed(T const& element) {
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

  void writeMin(std::array<T, STEAL_NUM> const& val) {
    mins = val;
//    for (size_t i = 0; i < STEAL_NUM; i++) {
//      mins[i] = val[i]; // todo maybe array
//    }
    version.fetch_add(1, std::memory_order_acq_rel);
  }


//	template <typename Indexer>
//  T steal(Indexer const& indexer) {
//	  while (true) {
//      auto wantToSteal = getMin();
//
//      if (isUsed(wantToSteal))
//        return wantToSteal;
//
//      remove_info_by_val(indexer, wantToSteal);
//      auto changeVal = usedT; // non const reference is needed
//      min.compare_exchange_strong(wantToSteal, changeVal, std::memory_order_acq_rel, std::memory_order_acq_rel);
//      if (changeVal == wantToSteal)
//        return wantToSteal;
//    }
//  }

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

  template <typename Indexer>
  T extractMinLocally(Indexer const& indexer) {
    auto minVal = heap[0];
    remove_info(indexer, 0);
    heap[0] = heap.back();
    heap.pop_back();
    if (heap.size() > 0) {
      sift_down(indexer, 0);
    }
    return minVal;
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

  T extractMin() {

    bool useless = false;
    if (heap.size() > 0) {
//      auto secondMin = extractMinLocally();
      auto stolen = steal(useless); // min.exchange(secondMin, std::memory_order_acq_rel);
//      writeMin(secondMin);
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

//	    if (isUsed(getMin())) {
//	      if (heap.size() > 0) {
//	        auto thirdMin = extractMinLocally();
//	        min.store(thirdMin, std::memory_order_release);
//	      } else {
//	        // No elements for other threads
//	      }
//	      return secondMin;
//	    } else {
//	      auto firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
//	      if (isUsed(firstMin)) {
//	        // somebody took the element
//	        if (heap.size() > 0)
//	          return extractMinLocally();
//	        return usedT;
//	      } else {
//	        // min was not stolen
//	        return firstMin;
//	      }
//	    }


//	template <typename Indexer>
//  T extractMin(Indexer const& indexer) {
//    if (heap.size() > 0) {
//      auto secondMin = extractMinLocally(indexer);
//      if (isUsed(getMin())) {
//        if (heap.size() > 0) {
//          auto thirdMin = extractMinLocally(indexer);
//          set_position_min(indexer,thirdMin);
//          min.store(thirdMin, std::memory_order_release);
//        } else {
//          // No elements for other threads
//        }
//        return secondMin;
//      } else {
//        auto firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
//        remove_info_by_val(indexer, firstMin);
//        if (isUsed(firstMin)) {
//          // somebody took the element
//          if (heap.size() > 0) return extractMinLocally(indexer);
//          return secondMin; // todo or optional as we have nothing to do
//        } else {
//          // min was not stolen
//          return firstMin;
//        }
//      }
//    } else {
//      return steal(indexer);
//    }
//  }

  template <typename Indexer>
  T extractMin(Indexer const& indexer) {
//    if (heap.size() > 0) {
//      auto secondMin = extractMinLocally(indexer);
//      auto firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
//      if (isUsed(firstMin)) {
//        return heap.size() > 0 ? extractMinLocally(indexer) : usedT;
//      }
//      return firstMin;
//    } else {
//      // No elements in the heap, just take min if we can
//      return steal();
//    }
    return usedT;
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
//      auto stolen = steal(); // todo i suppose its stolen
      assert(getVersion() % 2 == 0); // should be stolen
      writeMin(extractMinLocally());
//      auto exchanged = min.exchange(extractMinLocally(), std::memory_order_acq_rel);
//      if (!isUsed(stolen))
//        pushHelper(stolen);
//    } else if (isUsed(curMin)) {
//      writeMin(extractMinLocally());
//      min.store(extractMinLocally(), std::memory_order_release);
//    }
    }
    return npush;
  }

  size_t inOurQueue = 0;
  size_t inAnotherQueue = 0;
  size_t notInQeues = 0;

  ~StealDAryHeap() {
//    std::cout << "Found in our queue: " << inOurQueue << std::endl;
//    std::cout << "Found in another queue: " << inAnotherQueue << std::endl;
//    std::cout << "Not in queues: " << notInQeues << std::endl;
  }

  template <typename Indexer, typename Iter>
  int pushRange(Indexer const& indexer, Iter b, Iter e) {
    if (b == e) return 0;

    int npush = 0;

    while (b != e) {
      npush++;
      auto position = indexer.get_pair(*b);
      auto queue = position.first;
      auto index = position.second;

      if (queue != qInd) {
        if (queue == -1) {
          notInQeues++;
        } else {
          inAnotherQueue++;
        }
        pushHelper(indexer, *b++);
      } else {
        inOurQueue++;
        if (cmp(heap[index], *b)) {
          heap[index] = *b++;
          sift_up(indexer, index);
        }
      }
    }
//    auto curMin = getMin();
//    if (!isUsed(curMin) && cmp(curMin, heap[0])) {
//      auto exchanged = steal();
//      writeMin(extractMinLocally(indexer));
////      auto exchanged = min.exchange(extractMinLocally(indexer), std::memory_order_acq_rel);
//      if (!isUsed(exchanged))
//        pushHelper(indexer, exchanged);
//    } else {
//      if (isUsed(curMin)) {
//        auto minFromHeap = extractMinLocally(indexer);
//        writeMin(minFromHeap);
////        min.store(minFromHeap, std::memory_order_release);
//      }
//    }
    return npush;
  }


  template <typename Indexer>
  void push(Indexer const& indexer, T const& val) {
    bool useless;
    auto curMin = getMin(useless);

    auto position = indexer.get_pair(val);
    auto queue = position.first;
    auto index = position.second;

    if (queue != qInd) {
      if (queue == -1) {
        notInQeues++;
      } else {
        inAnotherQueue++;
      }
      if (!isUsed(curMin) && cmp(curMin, val)) {
        auto exchanged = getMin(useless);// todo заглушка min.exchange(val, std::memory_order_acq_rel);
        if (isUsed(exchanged)) return;
        else pushHelper(indexer, exchanged);
      } else {
        pushHelper(indexer, val);
        if (isUsed(curMin)) {
          auto minFromHeap = extractMinLocally(indexer);
          writeMin(minFromHeap);
//          min.store(minFromHeap, std::memory_order_release);
        }
      }
    } else {
      inOurQueue++;
      if (cmp(heap[index], val)) {
        heap[index] = val;
        sift_up(indexer, index);
      }
    }
  }

  //! Set that the element is not in the heap anymore.
  template <typename Indexer>
  void remove_info(Indexer const& indexer, index_t index) {
    indexer.set_pair(heap[index], -1, 0);
  }

  template <typename Indexer>
  void remove_info_by_val(Indexer const& indexer, T const& val) {
    indexer.set_pair(val, -1, 0);
  }

//  template <typename Indexer>
//  void remove_info_min(Indexer const& indexer) {
//	  auto minVal = getMin();
//	  if (!isUsed(minVal)) {
//      indexer.set_pair(minVal, -1, 0);
//    }
//  }

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


template <size_t PV, size_t QV>
struct SProb {
  static const size_t P = PV;
  static const size_t Q = QV;
};

template<typename T,
typename Comparer,
typename StealProb = SProb<0, 1>,
bool Concurrent = true,
size_t StealBatchSize = 8,
typename Numa = SProb<2, 1>,
bool DecreaseKey = false,
typename Indexer = void,
typename Container = StealDAryHeap<T, Comparer, 4, StealBatchSize>
>
class StealingMultiQueue {
private:
  typedef Container Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
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

  const size_t socketSize = 24;
  size_t node1Cnt() {
    size_t res = 0;
    if (nQ > socketSize) {
      res += socketSize;
      if (socketSize * 2 < nQ) {
        res += std::min(nQ, socketSize * 3) - socketSize * 2;
      }
      return res;
    } else {
      return nQ;
    }
  }

  size_t node2Cnt() {
    return nQ - node1Cnt();
  }

  size_t is1Node(size_t tId) {
    return tId < socketSize || (tId >= socketSize * 2 && tId < socketSize * 3);
  }

  size_t is2Node(size_t tId) {
    return !is1Node(tId);
  }


  size_t map1Node(size_t qId) {
    if (qId < socketSize) {
      return qId;
    }
    return qId + socketSize;
  }

  size_t map2Node(size_t qId) {
    if (qId < socketSize) {
      return qId + socketSize;
    }
    return qId + socketSize * 2;
  }

  inline size_t rand_heap() {
    return random() % nQ;
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    static const size_t LOCAL_W = Numa::P;
    static const size_t OTHER_W = Numa::Q;

    size_t isFirst = is1Node(tId);
    size_t localCnt = isFirst ? node1Cnt() : node2Cnt();
    size_t otherCnt = nQ - localCnt;
    const size_t Q = localCnt * LOCAL_W + otherCnt * OTHER_W;
    const size_t r = random() % Q;
    if (r < localCnt * LOCAL_W) {
      // we are stealing from our node
      auto qId = r / LOCAL_W;
      return isFirst ? map1Node(qId) : map2Node(qId);
    } else {
      auto qId = (r - localCnt * LOCAL_W) / OTHER_W;
      return isFirst ? map2Node(qId) : map1Node(qId);
    }
  }


  static Galois::Statistic* popNull;
  static Galois::Statistic* popRes;

  void initStatistic(Galois::Statistic*& st, std::string const& name) {
    if (st == nullptr)
      st = new Galois::Statistic(name);
  }

public:
  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    for (size_t i = 0; i < nQ; i++) {
      heaps[i].data.set_id(i);
    }
    std::cout << "Queues: " << nQ << std::endl;
    initStatistic(popNull, "popNull");
    initStatistic(popRes, "popRes");
  }

  void deleteStatistic(Galois::Statistic*& st) {
    if (st != nullptr) {
      delete st;
      st = nullptr;
    }
  }

  ~StealingMultiQueue() {
//    std::ofstream out("smq_pop_" + std::to_string(nQ), std::ios::app);
//    out << "smq_" << std::to_string(StealProb::P) + "_" + std::to_string(StealProb::Q) << "_" <<
//        std::to_string(StealBatchSize) + "_" << Numa::P << "_" << Numa::Q
//        << " " << getStatVal(popRes) << " " << getStatVal(popNull) << std::endl;
//    out.close();
    deleteStatistic(popNull);
    deleteStatistic(popRes);
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
    typedef StealingMultiQueue<T, Comparer, StealProb, _concurrent, StealBatchSize, Numa, DecreaseKey, Indexer, Container> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueue<_T, Comparer, StealProb, Concurrent,StealBatchSize, Numa, DecreaseKey, Indexer, Container> type;
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
    if constexpr (DecreaseKey) {
      static Indexer indexer;
      return heap->pushRange(indexer, b, e);
//      while (b != e) {
//        heap->push(indexer, *b++);
//        npush++;
//      }
    } else {
      return heap->pushRange(b, e);
//      while (b != e) {
//        heap->push(*b++);
//        npush++;
//      }
    }
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); // todo bounds? can be changed?

    Galois::optional<T> result;

    if (nQ > 1) {
      size_t change = random() % StealProb::Q;
      if (change < StealProb::P) {

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
            Heap *localH = &heaps[tId].data;
            bool useless;
            auto localMin = localH->getMin(useless);
            if (localH->isUsed(localMin)) {
              localMin = localH->updateMin();
            }
            if (randH->isUsed(localMin) || compare(localMin, randMin)) {
              auto stolen = randH->steal(again);
              if (stolen.is_initialized()) {
                typename Heap::stealing_array_t vals = stolen.get();
                auto minId = localH->getMinId(vals);
                for (size_t i = 0; i < vals.size(); i++) {
                  if (i == minId || localH->isUsed(vals[i])) continue;
                  localH->pushHelper(vals[i]);
                }
                *popRes += 1;
                return vals[minId];
              }
            }
          }
        }
      }
    }
    auto minVal = heaps[tId].data.extractMin();
    if (!heaps[tId].data.isUsed(minVal)) {
      *popRes += 1;
      return minVal;
    }
    // our heap is empty
    if (nQ == 1)  { // nobody to steal from
      *popNull += 1;
      return result;
    }

    const size_t RANDOM_ATTEMPTS = nQ > 2 ? 4 : 0;
    bool again = true;
//    for (size_t r = 0; r < RANDOM_ATTEMPTS; r++) {
    while (again) {
      again = false;
      auto randH = rand_heap();
      if (randH == tId) continue;
      auto stolen = heaps[randH].data.steal(again);
      if (stolen.is_initialized()) {
        Heap* localH = &heaps[tId].data;
        typename Heap::stealing_array_t vals = stolen.get();
        auto minId = localH->getMinId(vals);
        for (size_t i = 0; i < vals.size(); i++) {
          if (i == minId || localH->isUsed(vals[i])) continue;
          localH->pushHelper(vals[i]);
        }
        *popRes += 1;
        return vals[minId];
      }
    }
//    for (size_t i = 0; i < nQ; i++) {
//      if (i == tId) continue;
//      auto stolen = heaps[i].data.steal();
//      if (!heaps[i].data.isUsed(stolen))
//        return stolen;
//    }
    *popNull += 1;
    return result;
  }
};



template<typename T,
typename Comparer,
typename StealProb,
bool Concurrent,
size_t StealBatchSize,
typename Numa,
bool DecreaseKey,
typename Indexer,
typename Container
>
Galois::Statistic* StealingMultiQueue<T, Comparer, StealProb, Concurrent, StealBatchSize, Numa, DecreaseKey,
Indexer, Container>::popRes;


template<typename T,
typename Comparer,
typename StealProb,
bool Concurrent,
size_t StealBatchSize,
typename Numa,
bool DecreaseKey,
typename Indexer,
typename Container
>
Galois::Statistic* StealingMultiQueue<T, Comparer, StealProb, Concurrent, StealBatchSize, Numa, DecreaseKey,
Indexer, Container>::popNull;

template<typename T,
typename Compare,
size_t D,
size_t STEAL_NUM>
T StealDAryHeap<T, Compare, D, STEAL_NUM>::usedT;

}

#endif //GALOIS_STEALINGMULTIQUEUE_H

