#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

template<typename T = int,
				 typename Compare = std::greater<int>,
				 size_t D = 4>
struct StealDAryHeap {
	typedef size_t index_t;

	std::vector<T> heap;
	std::atomic<T> min;
	static T usedT;
	std::atomic<bool> localEmpty = {true};
//	std::atomic<bool> empty = {true };
	Compare cmp;

	size_t qInd;

	void set_id(size_t id) {
	  qInd = id;
	}

	StealDAryHeap(): min(usedT) {
    //memset(reinterpret_cast<void*>(&usedT), 0xff, sizeof(usedT));
    //min.store(usedT, std::memory_order_release);
	}

	bool isEmpty() {
	  return min.load(std::memory_order_acquire) == usedT && localEmpty.load(std::memory_order_acquire);
	}

	bool isUsed(T const& element) {
	  return element == usedT;
	}

	T getMin() {
		return min.load(std::memory_order_acquire);
	}

	T steal() {
	  return min.exchange(usedT, std::memory_order_acq_rel);
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


  T extractMinLocally() {
    auto minVal = heap[0];
    heap[0] = heap.back();
    heap.pop_back();
    if (heap.size() > 0) {
      sift_down(0);
    } else {
      localEmpty.store(true, std::memory_order_release);
    }
    return minVal;
  }

  template <typename Indexer>
  T extractMinLocally(Indexer const& indexer) {
    auto minVal = heap[0];
    heap[0] = heap.back();
    remove_info(indexer, 0);
    heap.pop_back();
    if (heap.size() > 0) {
      sift_down(indexer, 0);
    } else {
      localEmpty.store(true, std::memory_order_release);
    }
    return minVal;
  }

	// When current min is stolen
	T updateMin() {
	  if (heap.size() == 0) return usedT;
	  auto val = extractMinLocally();
	  min.store(val, std::memory_order_release);
	  return min;
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
    if (heap.size() == 0) return usedT;
    auto val = extractMinLocally(indexer);
    min.store(val, std::memory_order_release);
    return min;
  }

	T extractMin() {
	  if (heap.size() > 0) {
	    auto secondMin = extractMinLocally();
	    if (isUsed(getMin())) {
	      if (heap.size() > 0) {
	        auto thirdMin = extractMinLocally();
	        min.store(thirdMin, std::memory_order_release);
	      } else {
	        // No elements for other threads
	      }
	      return secondMin;
	    } else {
	      auto firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
	      if (isUsed(firstMin)) {
	        // somebody took the element
	        if (heap.size() > 0) return extractMinLocally();
	        return usedT; // todo or optional as we have nothing to do
	      } else {
	        // min was not stolen
	        return firstMin;
	      }
	    }
	  } else {
	    return steal();
	  }
	}

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
    if (heap.size() > 0) {
      auto secondMin = extractMinLocally(indexer);
      if (isUsed(getMin())) {
        if (heap.size() > 0) {
          auto thirdMin = extractMinLocally(indexer);
          min.store(thirdMin, std::memory_order_release);
        } else {
          // No elements left for min
        }
        return secondMin;
      } else {
        auto firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
        if (isUsed(firstMin)) {
          // somebody took the element
          if (heap.size() > 0) return extractMinLocally(indexer);
          return usedT; // todo or optional as we have nothing to do
        } else {
          // min was not stolen
          return firstMin;
        }
      }
    } else {
      return steal();
    }
  }


	void pushHelper(T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(index);
    if (heap.size() == 1) localEmpty.store(false, std::memory_order_release);
	}

  template <typename Indexer>
  void pushHelper(Indexer const& indexer, T const& val) {
    index_t index = heap.size();
    heap.push_back({val});
    sift_up(indexer, index);
    if (heap.size() == 1)
      localEmpty.store(false, std::memory_order_release);
  }

	//! Push the element.
	void push(T const& val) {
    auto curMin = getMin();

    if (!isUsed(curMin) && cmp(curMin, val)) {
      auto exchanged = min.exchange(val, std::memory_order_acq_rel);
      if (isUsed(exchanged)) return;
      else pushHelper(exchanged);
    } else {
      pushHelper(val);
      if (isUsed(curMin)) {
        min.store(extractMinLocally(), std::memory_order_release);
      }
    }
	}

  template <typename Indexer>
  void push(Indexer const& indexer, T const& val) {
    auto curMin = getMin();

    auto position = indexer.get_pair(val);
    auto queue = position.first;
    auto index = position.second;

    if (queue != qInd) {
      if (!isUsed(curMin) && cmp(curMin, val)) {
        auto exchanged = min.exchange(val, std::memory_order_acq_rel);
        if (isUsed(exchanged)) return;
        else pushHelper(indexer, exchanged);
      } else {
        pushHelper(indexer, val);
        if (isUsed(curMin)) {
          auto minFromHeap = extractMinLocally(indexer);
          min.store(minFromHeap, std::memory_order_release);
        }
      }
    } else {
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
         bool DecreaseKey = false,
         typename Indexer = void
         >
class StealingMultiQueue {

private:
  typedef StealDAryHeap<T, Comparer, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  const size_t nQ;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random(); // todo
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

public:
  StealingMultiQueue() : nQ(Galois::getActiveThreads()) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    for (size_t i = 0; i < nQ; i++) {
      heaps[i].data.set_id(i);
    }
    std::cout << "Queues: " << nQ << std::endl;
  }

  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef StealingMultiQueue<T, Comparer, StealProb, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingMultiQueue<_T, Comparer, StealProb, Concurrent> type;
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
    int npush = 0;
    if constexpr (DecreaseKey) {
      static Indexer indexer;
      while (b != e) {
        heap->push(indexer, *b++);
        npush++;
      }
    } else {
      while (b != e) {
        heap->push(*b++);
        npush++;
      }
    }
    return npush;
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); // todo bounds? can be changed?

    Galois::optional<T> result;

    if constexpr (DecreaseKey) {
      static Indexer indexer;
      const size_t RANDOM_ATTEMPTS = nQ > 2 ? 4 : 0;
      if (heaps[tId].data.isEmpty()) {
        if (nQ > 1) {
          for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
            auto randH = rand_heap();
            if (randH == tId) continue;
            auto stolen = heaps[randH].data.steal();
            if (!heaps[randH].data.isUsed(stolen)) {
              return stolen;
            }
          }
          for (size_t i = 0; i < nQ; i++) {
            if (i == tId) continue;
            auto stolen = heaps[i].data.steal();
            if (!heaps[i].data.isUsed(stolen)) {
              return stolen;
            }
          }
        }
        return result;
      } else {
        // our heap is not empty
        if (nQ > 1) {
          size_t change = random() % StealProb::Q;
          if (change < StealProb::P) {
            // we try to steal
            auto randId = (tId + 1 + (random() % (nQ - 1))) % nQ;
            Heap *randH = &heaps[randId].data;
            auto randMin = randH->getMin();
            if (randH->isUsed(randMin)) {
              goto extract_locally_dk;
            }
            Heap *localH = &heaps[tId].data;
            auto localMin = localH->getMin();
            if (localH->isUsed(localMin)) {
              localMin = localH->updateMin(indexer);
            }
            if (randH->isUsed(localMin) || compare(localMin, randMin)) {
              auto stolen = randH->steal();
              if (!randH->isUsed(stolen))
                return stolen;
            }
          }
        }
        extract_locally_dk:
        auto extracted = heaps[tId].data.extractMin(indexer);
        if (!heaps[tId].data.isUsed(extracted))
          return extracted;
        return result;
        //}
      }
    } else {
      const size_t RANDOM_ATTEMPTS = nQ > 2 ? 4 : 0;
      if (heaps[tId].data.isEmpty()) {
        if (nQ > 1) {
          for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
            auto randH = rand_heap();
            if (randH == tId) continue;
            auto stolen = heaps[randH].data.steal();
            if (!heaps[randH].data.isUsed(stolen)) {
              return stolen;
            }
          }
          for (size_t i = 0; i < nQ; i++) {
            if (i == tId) continue;
            auto stolen = heaps[i].data.steal();
            if (!heaps[i].data.isUsed(stolen)) {
              return stolen;
            }
          }
        }
        return result;
      } else {
        // our heap is not empty
        if (nQ > 1) {
          size_t change = random() % StealProb::Q;
          if (change < StealProb::P) {
            // we try to steal
            auto randId = (tId + 1 + (random() % (nQ - 1))) % nQ;
            Heap *randH = &heaps[randId].data;
            auto randMin = randH->getMin();
            if (randH->isUsed(randMin)) {
              goto extract_locally;
            }
            Heap *localH = &heaps[tId].data;
            auto localMin = localH->getMin();
            if (localH->isUsed(localMin)) {
              localMin = localH->updateMin();
            }
            if (randH->isUsed(localMin) || compare(localMin, randMin)) {
              auto stolen = randH->steal();
              if (!randH->isUsed(stolen))
                return stolen;
            }
          }
        }
        extract_locally:
        auto extracted = heaps[tId].data.extractMin();
        if (!heaps[tId].data.isUsed(extracted))
          return extracted;
        return result;
        //}
      }
    }
  }
};

template<typename T, typename Comparer, typename StealProb = SProb<0, 1>, bool Concurrent = true>
class StealingDoubleQueue {
private:
  typedef StealDAryHeap<T, Comparer, 4> Heap;
  std::unique_ptr<Galois::Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  const size_t nQ;

  //! Thread local random.
  uint32_t random() {
    static thread_local uint32_t x = generate_random(); // todo
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

public:
  StealingDoubleQueue() : nQ(Galois::getActiveThreads() * 2) {
    memset(reinterpret_cast<void*>(&Heap::usedT), 0xff, sizeof(Heap::usedT));
    heaps = std::make_unique<Galois::Runtime::LL::CacheLineStorage<Heap>[]>(nQ);
    std::cout << "Queues: " << nQ << std::endl;
  }

  typedef T value_type;

  //! Change the concurrency flag.
  template<bool _concurrent>
  struct rethread {
    typedef StealingDoubleQueue<T, Comparer, StealProb, _concurrent> type;
  };

  //! Change the type the worklist holds.
  template<typename _T>
  struct retype {
    typedef StealingDoubleQueue<_T, Comparer, StealProb, Concurrent> type;
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
    static thread_local size_t qId = tId * 2;
    Heap* heap = &heaps[qId].data;
    int npush = 0;
    while (b != e) {
      heap->push(*b++);
      npush++;
    }
    qId ^= 1;
    return npush;
  }

  T extractLocalMin(size_t const& qId) {
    Heap* h1 = &heaps[qId].data;
    Heap* h2 = &heaps[qId + 1].data;

    auto min1 = h1->getMin();
    auto min2 = h2->getMin();


    if (h1->isUsed(min1)) {
      min1 = h1->updateMin();
    }
    if (h2->isUsed(min2)) {
      min2 = h2->updateMin();
    }

    if (h1->isUsed(min1)) {
      return h2->extractMin();
    }
    if (h2->isUsed(min1)) {
      return h1->extractMin();
    }

    if (compare(min1, min2)) { // >
      return h2->extractMin();
    } else {
      return h1->extractMin();
    }
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID();
    static thread_local size_t qId = tId * 2;

    Galois::optional<T> result;

    const size_t RANDOM_ATTEMPTS = nQ > 2 ? 4 : 0;
    if (heaps[qId].data.isEmpty() && heaps[qId + 1].data.isEmpty()) {
      if (nQ > 2) {
        for (size_t i = 0; i < RANDOM_ATTEMPTS; i++) {
          auto randH = rand_heap(); // (qId + 2 +  (random() % (nQ - 2))) % nQ;
          if (randH == qId || randH == qId + 1) continue;
          auto stolen = heaps[randH].data.steal();
          if (!heaps[randH].data.isUsed(stolen)) {
            return stolen;
          }
        }
        for (size_t i = 0; i < nQ; i++) {
          if (i == qId || i == qId + 1) continue;
          auto stolen = heaps[i].data.steal();
          if (!heaps[i].data.isUsed(stolen)) {
            return stolen;
          }
        }
      }
      return result;
    } else {
      // our heap is not empty
      if (nQ > 2) {
        size_t change = random() % StealProb::Q;
        if (change < StealProb::P) {
          // Trying to steal
          auto randH = (qId + 2 +  (random() % (nQ - 2))) % nQ;
          auto stolen = heaps[randH].data.steal();
          if (!heaps[randH].data.isUsed(stolen)) {
            return stolen;
          }
        }
      }
      auto extracted = extractLocalMin(qId);
      if (!heaps[qId].data.isUsed(extracted))
        return extracted;
      return result;
      //}
    }
  }
};


template<typename T,
typename Compare,
size_t D>
T StealDAryHeap<T, Compare, D>::usedT;


#endif //GALOIS_STEALINGMULTIQUEUE_H
