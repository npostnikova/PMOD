#ifndef GALOIS_STEALINGMULTIQUEUE_H
#define GALOIS_STEALINGMULTIQUEUE_H

#include <atomic>
#include <cstdlib>
#include <vector>

template <typename T>
struct atomwrapper
{
  std::atomic<T> _a;

  atomwrapper()
  :_a()
  {}

  atomwrapper(const std::atomic<T> &a)
  :_a(a.load(std::memory_order_relaxed))
  {}

  atomwrapper(const atomwrapper &other)
  :_a(other._a.load(std::memory_order_relaxed))
  {}

  atomwrapper &operator=(const atomwrapper &other)
  {
    _a.store(other._a.load(std::memory_order_relaxed));
  }

  T load(std::memory_order order) {
    return _a.load(order);
  }

  void store(T val, std::memory_order order) {
    _a.store(val, order);
  }

  T exchange(T val, std::memory_order order) { // todo check types for const and &
    return _a.exchange(val, order);
  }
};

template<typename T = int,
				 typename Compare = std::greater<int>,
				 size_t D = 4>
struct StealDAryHeap {
	typedef size_t index_t;

	std::vector<T> heap;
	atomwrapper<T> min;
	static T usedT;
	std::atomic<bool> localEmpty = {true};
//	std::atomic<bool> empty = {true };
	Compare cmp;

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

	//! Size of the heap.
	// why?
	size_t size() {
		return heap.size();
	}

	//! Minimum element in the heap. UB if the heap is empty.
	T getMin() {
		return min.load(std::memory_order_relaxed); // from our thread
	}

  T steal() {
    return min.exchange(usedT, std::memory_order_acquire);
  }


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

  T loadMin() {
	  return min.load(std::memory_order_acquire);
	}

	T extractMin() {
	  if (heap.size() > 0) {
	    auto secondMin = extractMinLocally();
	    if (isUsed(loadMin())) {
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
	        return secondMin; // todo or optional as we have nothing to do
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

	//! Push the element.
	void push(T const& val) {
    auto curMin = loadMin();
    if (isUsed(curMin) || cmp(curMin, val)) {
      auto exchanged = min.exchange(val, std::memory_order_acq_rel);
      if (isUsed(exchanged)) return;
      else pushHelper(exchanged);
    } else {
      pushHelper(val);
    }
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


template <size_t PV, size_t QV>
struct SProb {
  static const size_t P = PV;
  static const size_t Q = QV;
};

template<typename T, typename Comparer, typename StealProb = SProb<0, 1>, bool Concurrent = true>
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
    while (b != e) {
      heap->push(*b++);
      npush++;
    }
    return npush;
  }

  Galois::optional<T> pop() {
    static thread_local size_t tId = Galois::Runtime::LL::getTID(); // todo bounds? can be changed?


    Galois::optional<T> result;

    const size_t RANDOM_ATTEMPTS = nQ > 2 ? 4 : 0;
    if (heaps[tId].data.isEmpty()) {
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
      return result;
    } else {
      // our heap is not empty
      size_t change = random() % StealProb::Q;
      if (change < StealProb::P) {
        auto randH = rand_heap();
        auto stolen = heaps[randH].data.steal();
        if (!heaps[randH].data.isUsed(stolen)) {
          return stolen;
        }
      }
      auto extracted = heaps[tId].data.extractMin();
      if (!heaps[tId].data.isUsed(extracted))
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
