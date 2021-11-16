#ifndef GALOIS_HEAPWITHLOCK_H
#define GALOIS_HEAPWITHLOCK_H

#include "../WorkListHelpers.h"

#include <boost/heap/d_ary_heap.hpp>


namespace Galois {
namespace WorkList {

/**
 * Lockable heap structure.
 *
 * @tparam T type of stored elements
 * @tparam Comparer callable defining ordering for objects of type `T`
 * @tparam Prior Type of T's priority
 * @tparam D Arity of a sequential heap
 * Its `operator()` returns `true` iff the first argument should follow the second one.
 */
template <typename T,
          typename Comparer,
          typename Prior = unsigned long,
          size_t D = 4>
struct HeapWithLock {
  typedef boost::heap::d_ary_heap<T, boost::heap::arity<D>,
          boost::heap::compare<Comparer>> DAryHeap;
  DAryHeap heap;
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
    heap.size() > 0 ? heap.top().prior() : dummy,
    std::memory_order_release
    );
  }

  T extractMin() {
    T result = heap.top();
    heap.pop();
    return result;
  }

  void push(const T& task) {
    heap.push(task);
  }

  bool empty() const {
    return heap.empty();
  }

private:
  Runtime::LL::SimpleLock<true> _lock;
  std::atomic<Prior> min;
};

template<typename T,
         typename Compare,
         typename Prior,
         size_t D>
Prior HeapWithLock<T, Compare, Prior, D>::dummy;

} // namespace WorkList
} // namespace Galois

#endif //GALOIS_HEAPWITHLOCK_H
