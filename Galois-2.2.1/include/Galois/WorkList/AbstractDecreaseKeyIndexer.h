#ifndef GALOIS_ABSTRACTDECREASEKEYINDEXER_H
#define GALOIS_ABSTRACTDECREASEKEYINDEXER_H

/**
 * Interface of DecreaseKeyIndexer for AdaptiveMultiQueue.
 * @tparam T type of indexed elements 
 */
template <typename T>
struct DecreaseKeyIndexer {
	//! Returns number of queue of the element. -1 if none.
	virtual static int get_queue(T const& t) = 0;

	//! Returns number of index of the element.
	//! Should not return -1 if queue index is specified.
	virtual static int get_index(T const& t) = 0;

	//! Sets queue to new value if the old value equals to expected.
	virtual static bool set_queue(T const& t, int expQ, int newQ) = 0;

	//! Sets index.
	virtual void set_index(T const& t, size_t index) = 0;
};


#endif //GALOIS_ABSTRACTDECREASEKEYINDEXER_H
