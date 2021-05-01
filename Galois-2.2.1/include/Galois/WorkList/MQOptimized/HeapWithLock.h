#ifndef GALOIS_HEAPWITHLOCK_H
#define GALOIS_HEAPWITHLOCK_H

#include "../Heap.h"
#include "../WorkListHelpers.h"

/**
 * Lockable heap structure.
 *
 * @tparam T type of stored elements
 * @tparam Comparer callable defining ordering for objects of type `T`
 * @tparam Prior Type of T's priority
 * @tparam D Arity of a sequential heap
 * Its `operator()` returns `true` iff the first argument should follow the second one.
 */
template <typename  T,
typename Comparer,
typename Prior = unsigned long,
size_t D = 4>
struct HeapWithLock {
  DAryHeap<T, Comparer, 4> heap;
  // Dummy element for setting min when the heap is empty
  static Prior dummy;

  HeapWithLock() : min(dummy) {}

  //! Non-blocking lock.
  bool try_lock() {
    return _lock.try_lock();
  }

  //! Blocking lock.
  void lock() {
    _lock.lock();
  }

  //! Unlocks the queue.
  void unlock() {
    _lock.unlock();
  }

  Prior getMin() {
    return min.load(std::memory_order_acquire);
  }

  static bool isMinDummy(Prior const& value) {
    return value == dummy;
  }

  void updateMin() {
    return min.store(
    heap.size() > 0 ? heap.min().prior() : dummy,
    std::memory_order_release
    );
  }

private:
  Runtime::LL::PaddedLock<true> _lock;
  std::atomic<Prior> min;
};

template<typename T,
typename Compare,
typename Prior,
size_t D>
Prior HeapWithLock<T, Compare, Prior, D>::dummy;

#endif //GALOIS_HEAPWITHLOCK_H
