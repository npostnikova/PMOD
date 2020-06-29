#ifndef D_ARY_HEAP_H
#define D_ARY_HEAP_H

#include <vector>

/**
 * Heap for AdaptiveMultiQueue.
 *
 * @tparam T the type of data stored int he heap
 * @tparam Compare callable defining ordering for objects of type `T`.
 * Its `operator()` must return `true` iff the first argument should follow the second one.
 * @tparam D arity of the heap
 */
template<typename T = int,
				 typename Compare = std::greater<int>,
				 size_t D = 4>
struct DAryHeap {
	typedef size_t index_t;

	std::vector<T> heap;
	Compare cmp;

	//! Set index of the queue.
	void set_index(size_t index) {
		qInd = index;
	}

	//! Set value of dummy element for purposes of AdaptiveMultiQueue.
	void set_max_val(T const& max_val) {
		MAX_VAL = max_val;
	}

	//! Return whether the heap is empty.
	bool empty() {
		return heap.empty();
	}

	//! Size of the heap.
	size_t size() {
		return heap.size();
	}

	//! Minimum element in the heap. UB if the heap is empty.
	T min() {
		return heap[0];
	}

	//! Delete minimum element and return it.
	T extractMin() {
		auto min = heap[0];
		heap[0] = heap.back();
		heap.pop_back();
		sift_down(0);
		return min;
	}

	//! Delete minimum element and update information for Indexer.
	//! Indexer should implement AbstractDecreaseKeyIndexer interface.
	template <typename Indexer>
	T extractMin(Indexer const& indexer) {
		auto min = heap[0];
		remove_info(indexer, 0);
		heap[0] = heap.back();
		heap.pop_back();
		sift_down(indexer, 0);
		return min;
	}

	//! Push the element.
	void push(T const& val) {
		index_t index = heap.size();
		heap.push_back(val);
		sift_up(index);
	}

	//! Push the element with update information for Indexer.
	//! Should be called when `indexer` suppose that the element is not in the heap.
	template <typename Indexer>
	void push(Indexer const& indexer, T const& val) {
		index_t index = heap.size();
		heap.push_back(val);
		sift_up(indexer, index);
	}

	//! Decrease key if the new value is smaller.
	//! The `indexer` *must* be awared that the element is in the heap.
	template <typename Indexer>
	void decrease_key(Indexer const& indexer, T const& val) {
		auto index = indexer.get_index(val);
		if (cmp(heap[index], val)) {
			heap[index] = val;
			sift_up(indexer, index);
		}
	}

  void build() {
    // D * index + 1 is the first child
    for (size_t i = 0; is_valid_index(D * i + 1); i++) {
      sift_down(i);
    }
  }

  void divideElems(DAryHeap<T, Compare, D>& h, T& maxT) {
    size_t newSize = 0;
    for (size_t i = 0; i < size(); i++) {
      if (i % 2 == 0 && cmp(maxT, heap[i])) {
        h.push_back(heap[i]);
      } else {
        heap[newSize] = heap[i];
        newSize++;
      }
    }
    while (size() != newSize) {
      heap.pop_back();
    }
    build();
    h.build();
  }

private:
	T MAX_VAL;
	size_t qInd = 0;

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

	//! Sift down with decrease key info update.
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

	//! Set that the element is not in the heap anymore.
	template <typename Indexer>
	void remove_info(Indexer const& indexer, index_t index) {
		if (heap[index] != MAX_VAL && indexer.get_queue(heap[index]) == qInd) {
			indexer.set_index(heap[index], -1);
			indexer.set_queue(heap[index], qInd, -1);
		}
	}

	//! Update position in the `Indexer`.
	template <typename Indexer>
	void set_position(Indexer const& indexer, index_t new_pos) {
		if (heap[new_pos] != MAX_VAL && indexer.get_queue(heap[new_pos]) == qInd)
			indexer.set_index(heap[new_pos], new_pos);
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

	//! Sift up with updating info for DecreaseKeyIndexer.
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

	void push_back(T const& val) {
	  heap.push_back(val);
	}
	//! Nice for debug (not really).
//	void print_heap() {
//		for (size_t i = 0; i < heap.size(); i++) {
//			std::cout << heap[i] << " ";
//		}
//		std::cout << std::endl;
//	}
};

#endif //D_ARY_HEAP_H

