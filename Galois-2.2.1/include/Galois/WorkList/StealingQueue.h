#ifndef GALOIS_STEALINGQUEUE_H
#define GALOIS_STEALINGQUEUE_H

#include <deque>
#include <queue>

namespace smq {

template<typename T = int, typename Compare = std::greater<int>>
struct StealingQueue {
  typedef size_t index_t;

  Compare cmp;

  std::queue<T, std::deque<T>> queue;
  std::atomic<T> min;
  static T usedT;

  size_t qInd;

  void set_id(size_t id) {
    qInd = id;
  }

  StealingQueue() : min(usedT) {}

  bool isUsed(T const &element) {
    return element == usedT;
  }

  T getMin() {
    return min.load(std::memory_order_relaxed);
  }

  T steal() {
    return min.exchange(usedT, std::memory_order_acq_rel);
  }


  // When current min is stolen
  T updateMin() {
    if (queue.size() == 0) return usedT;
    auto val = extractMinLocally();
    min.store(val, std::memory_order_release);
    return val;
  }

  T extractMinLocally() {
    auto minVal = queue.front();
    queue.pop();
    return minVal;
  }

  T extractMin() {
    if (queue.size() > 0) {
      auto firstMin = getMin();
      auto secondMin = extractMinLocally();

      if (isUsed(firstMin)) {
        if (queue.size() > 0) {
          min.store(extractMinLocally(), std::memory_order_release);
        }
        return secondMin;
      } else if (cmp(secondMin, firstMin)) {
        // first min is less
        firstMin = min.exchange(secondMin, std::memory_order_acq_rel);
        if (isUsed(firstMin)) {
          return queue.size() > 0 ? extractMinLocally() : usedT;
        } else {
          return firstMin;
        }
      }
      return secondMin;
    } else {
      // No elements in the heap, just take min if we can
      return steal();
    }
  }

  template<typename Iter>
  int pushRange(Iter b, Iter e) {
    if (b == e)
      return 0;

    int npush = 0;

    while (b != e) {
      npush++;
      queue.push(*b++);
    }

    auto curMin = getMin();
    if (isUsed(curMin) && queue.size() > 0) {
      min.store(extractMinLocally(), std::memory_order_release);
    }
    return npush;
  }
};


template<typename T,
typename Compare>
T StealingQueue<T, Compare>::usedT;

}

#endif //GALOIS_STEALINGQUEUE_H
