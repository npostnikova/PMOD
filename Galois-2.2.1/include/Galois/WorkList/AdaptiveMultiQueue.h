#ifndef ADAPTIVE_MULTIQUEUE_H
#define ADAPTIVE_MULTIQUEUE_H

#include <atomic>
#include <memory>
#include <cstdlib>

namespace Galois {
namespace WorkList {

template<class Comparer, typename T, size_t C = 2, bool Concurrent = true>
class AdaptiveMultiQueue {
private:
  typedef boost::heap::d_ary_heap<T,
              boost::heap::arity<8>,
              boost::heap::compare<Comparer>
              > DAryHeap;

  //! Lockable heap structure.
  struct Heap {
    DAryHeap heap;
    T min;

    //! Initialize Heap class. Must be called before the class is used.
    static void init() {
      memset(reinterpret_cast<void *>(&maxT), 0xff, sizeof(maxT));

    }

    //! Initialize an instance.
    void set_max_val() {
      // todo move it to constructor or fix AMQ allocation
      min = maxT;
      heap.emplace(min);
    }

    //! Blocking lock.
    inline void lock() {
      bool expected = false;
      while (!_lock.compare_exchange_strong(expected, true)) {}
    }

    //! Non-blocking lock.
    inline bool try_lock() {
      bool expected = false;
      return _lock.compare_exchange_strong(expected, true);
    }

    //! Unlocks the queue.
    inline void unlock() {
      _lock = false;
    }

    //! Returns maximum value of T.
    T& max_val() {
      return maxT;
    }

  private:
    inline static T maxT;
    std::atomic<bool> _lock;
  };

  std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]> heaps;
  Comparer compare;
  const size_t nQ;

  void init_heaps() {
    Heap::init();
    for (size_t i = 0; i < nQ; i++) {
      heaps[i].data.set_max_val();
    }
  }

public:
  AdaptiveMultiQueue() : nQ(Galois::getActiveThreads() * C) {
  //heaps(new Runtime::LL::CacheLineStorage<Heap>[nQ]) {
    // TODO make_unique + why does it throw bad_alloc /\??
    heaps = std::unique_ptr<Runtime::LL::CacheLineStorage<Heap>[]>(new Runtime::LL::CacheLineStorage<Heap>[nQ]);
    init_heaps();
    std::srand(std::time(0));
  }

  //! T is the value type of the WL
  typedef T value_type;

  //! change the concurrency flag
  template<bool _concurrent>
  struct rethread {
    typedef AdaptiveMultiQueue<Comparer, T, C, _concurrent> type;
  };

  //! change the type the worklist holds
  template<typename _T>
  struct retype {
    typedef AdaptiveMultiQueue<Comparer, _T, C, Concurrent> type;
  };

  //! push a value onto the queue
  void push(const value_type &val) {
    Heap* heap;
    int q_ind;

    do {
      q_ind = std::rand() % nQ;
      heap = &heaps[q_ind].data;
    } while (!heap->try_lock());

    heap->heap.emplace(val);
    heap->min = heap->heap.top();
    heap->unlock();
  }

  //! push a range onto the queue
  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    const size_t chunk_size = 64;

    int npush = 0;

    while (b != e) {
      Heap *heap;
      int q_ind;

      do {
        q_ind = std::rand() % nQ;
        heap = &heaps[q_ind].data;
      } while (!heap->try_lock());

      for (size_t cnt = 0; cnt < chunk_size && b != e; cnt++, npush++) {
        heap->heap.emplace(*b++);
      }
      heap->min = heap->heap.top();
      heap->unlock();
    }
    return npush;
  }

  //! push initial range onto the queue
  //! called with the same b and e on each thread
  template<typename RangeTy>
  unsigned int push_initial(const RangeTy &range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  //! pop a value from the queue.
  Galois::optional<value_type> pop() {
    Galois::optional<value_type> result;

    Heap* heap_i, * heap_j;
    int i_ind, j_ind;

    do {
      i_ind = std::rand() % nQ;
      heap_i = &heaps[i_ind].data;

      j_ind = std::rand() % nQ;
      heap_j = &heaps[j_ind].data;

      if (i_ind == j_ind)
        continue;

      if (compare(heap_i->min, heap_j->min))
        heap_i = heap_j;
    } while (!heap_i->try_lock());

    if (heap_i->heap.size() == 1) {
      heap_i->unlock();
      for (j_ind = 1; j_ind < nQ; j_ind++) {
        heap_i = &heaps[(i_ind + j_ind) % nQ].data;
        if (heap_i->min == heap_i->max_val()) continue;
        heap_i->lock();
        if (heap_i->heap.size() > 1)
          goto deq;
        heap_i->unlock();
      }
      // empty
      return result;
    }

    deq:
    result = heap_i->heap.top();
    heap_i->heap.pop();
    heap_i->min = heap_i->heap.top();
    heap_i->unlock();
    return result;
  }

};

} // namespace WorkList
} // namespace Galois

#endif // ADAPTIVE_MULTIQUEUE_H
