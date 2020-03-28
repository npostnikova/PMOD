/** Scalable priority worklist -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2013, The University of Texas at Austin. All rights reserved.
 * UNIVERSITY EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES CONCERNING THIS
 * SOFTWARE AND DOCUMENTATION, INCLUDING ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR ANY PARTICULAR PURPOSE, NON-INFRINGEMENT AND WARRANTIES OF
 * PERFORMANCE, AND ANY WARRANTY THAT MIGHT OTHERWISE ARISE FROM COURSE OF
 * DEALING OR USAGE OF TRADE.  NO WARRANTY IS EITHER EXPRESS OR IMPLIED WITH
 * RESPECT TO THE USE OF THE SOFTWARE OR DOCUMENTATION. Under no circumstances
 * shall University be liable for incidental, special, indirect, direct or
 * consequential damages or loss of profits, interruption of business, or
 * related expenses which may arise from use of Software or Documentation,
 * including but not limited to those resulting from defects in Software and/or
 * Documentation, or loss or inaccuracy of data of any kind.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_WORKLIST_SL_OBIM_H
#define GALOIS_WORKLIST_SL_OBIM_H

#include "Galois/config.h"
#include "Galois/FlatMap.h"
#include "Galois/Timer.h"
#include "Galois/Runtime/PerThreadStorage.h"
#include "Galois/WorkList/Fifo.h"
#include "Galois/WorkList/WorkListHelpers.h"

#include GALOIS_CXX11_STD_HEADER(type_traits)
#include <limits>

#include <iostream>

namespace Galois {
namespace WorkList {

/**
 * Approximate priority scheduling. Indexer is a default-constructable class
 * whose instances conform to <code>R r = indexer(item)</code> where R is
 * some type with a total order defined by <code>operator&lt;</code> and <code>operator==</code>
 * and item is an element from the Galois set iterator.
 *
 * An example:
 * \code
 * struct Item { int index; };
 *
 * struct Indexer {
 *   int operator()(Item i) const { return i.index; }
 * };
 *
 * typedef Galois::WorkList::OrderedByIntegerMetric<Indexer> WL;
 * Galois::for_each<WL>(items.begin(), items.end(), Fn);
 * \endcode
 *
 * @tparam Indexer Indexer class
 * @tparam Container Scheduler for each bucket
 * @tparam BlockPeriod Check for higher priority work every 2^BlockPeriod
 *                     iterations
 * @tparam BSP Use back-scan prevention
 */
template<class Indexer = DummyIndexer<int>, typename Container = FIFO<>,
  int BlockPeriod=0,
  bool BSP=true,
  bool uniformBSP=false,
  typename T=int,
  typename Index=int,
  bool Concurrent=true>
struct SkipListOrderedByIntegerMetric : private boost::noncopyable {
  template<bool _concurrent>
  struct rethread { typedef SkipListOrderedByIntegerMetric<Indexer, typename Container::template rethread<_concurrent>::type, BlockPeriod, BSP, uniformBSP, T, Index, _concurrent> type; };

  template<typename _T>
  struct retype { typedef SkipListOrderedByIntegerMetric<Indexer, typename Container::template retype<_T>::type, BlockPeriod, BSP, uniformBSP, _T, typename std::result_of<Indexer(_T)>::type, Concurrent> type; };

  template<unsigned _period>
  struct with_block_period { typedef SkipListOrderedByIntegerMetric<Indexer, Container, _period, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<typename _container>
  struct with_container { typedef SkipListOrderedByIntegerMetric<Indexer, _container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<typename _indexer>
  struct with_indexer { typedef SkipListOrderedByIntegerMetric<_indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<bool _bsp>
  struct with_back_scan_prevention { typedef SkipListOrderedByIntegerMetric<Indexer, Container, BlockPeriod, _bsp, uniformBSP, T, Index, Concurrent> type; };

  typedef T value_type;

private:
  typedef typename Container::template rethread<Concurrent>::type CTy;
  typedef Galois::flat_map<Index, CTy*> LMapTy;
  //typedef std::map<Index, CTy*> LMapTy;

  struct perItem {
    LMapTy local;
    Index curIndex;
    Index scanStart;
    CTy* current;
    unsigned int lastMasterVersion;
    unsigned int numPops;

    perItem() :
      curIndex(std::numeric_limits<Index>::min()), 
      scanStart(std::numeric_limits<Index>::min()),
      current(0), lastMasterVersion(0), numPops(0) { }
  };

  typedef std::deque<std::pair<Index, CTy*> > MasterLog;
  typedef SkipListSetNode<Index,CTy*> sl_node_t;

  // NB: Place dynamically growing masterLog after fixed-size PerThreadStorage
  // members to give higher likelihood of reclaiming PerThreadStorage
  Runtime::PerThreadStorage<perItem> current;
  Runtime::LL::PaddedLock<Concurrent> masterLock;
  Galois::Timer clock;

  struct Comparer: public std::binary_function<const int, const int, unsigned> {
    unsigned operator()(const int x, const int y) const {
      return x > y;
    }
  };
  LockFreeSkipListSet<Comparer, Index, CTy*> Q;

  Runtime::MM::FixedSizeAllocator heap;
  std::atomic<unsigned int> masterVersion;
  Indexer indexer;

  GALOIS_ATTRIBUTE_NOINLINE
  Galois::optional<T> slowPop(perItem& p) {
    //Failed, find minimum bin
    unsigned myID = Runtime::LL::getTID();
    bool localLeader = Runtime::LL::isPackageLeaderForSelf(myID);

    Index msS = std::numeric_limits<Index>::min();
    if (BSP) {
      msS = p.scanStart;
      if (localLeader || uniformBSP) {
        for (unsigned i = 0; i < Runtime::activeThreads; ++i)
          msS = std::min(msS, current.getRemote(i)->scanStart);
      } else {
        msS = std::min(msS, current.getRemote(Runtime::LL::getLeaderForThread(myID))->scanStart);
      }
    }

    sl_node_t *ii, *next;

    for (ii = Q.lower_bound(msS); ii->next[0]; ii = LockFreeSkipListSet<Comparer, Index, CTy*>::unset_mark(next)) {
      next = ii->next[0];
      if (LockFreeSkipListSet<Comparer, Index, CTy*>::is_marked(next))
        continue;

      Galois::optional<T> retval;
      if (ii->val && (retval = ii->val->pop())) {
        p.current = ii->val;
        p.curIndex = ii->key;
        p.scanStart = ii->key;
        return retval;
      }
    }
    return Galois::optional<value_type>();
  }

  GALOIS_ATTRIBUTE_NOINLINE
  CTy* slowUpdateLocalOrCreate(perItem& p, Index i) {
    CTy* lC = new (heap.allocate(sizeof(CTy))) CTy(i);
    if (Q.push(i, lC))
      return lC;
    return Q.get(i);
  }

  inline CTy* updateLocalOrCreate(perItem& p, Index i) {
    //Try local then try update then find again or else create and update the master log
    CTy* lC;
    if ((lC = Q.get(i)))
      return lC;
    //slowpath
    return slowUpdateLocalOrCreate(p, i);
  }

public:
  SkipListOrderedByIntegerMetric(const Indexer& x = Indexer()): heap(sizeof(CTy)), masterVersion(0), indexer(x) {
    clock.start();
  }

  ~SkipListOrderedByIntegerMetric() {
    sl_node_t *ii, *next;

    for (ii = Q.head->next[0]; ii->next[0]; ii = LockFreeSkipListSet<Comparer, Index, CTy*>::unset_mark(next)) {
      next = ii->next[0];
      CTy* lC = ii->val;
      lC->~CTy();
      heap.deallocate(lC);
    }
  }

  void push(const value_type& val) {
    Index index = indexer(val);
    perItem& p = *current.getLocal();
    // Fast path
    if (index == p.curIndex && p.current) {
      p.current->push(val);
      return;
    }

    // Slow path
    CTy* lC = updateLocalOrCreate(p, index);
    if (BSP && index < p.scanStart)
      p.scanStart = index;
    // Opportunistically move to higher priority work
    if (index < p.curIndex) {
      p.curIndex = index;
      p.current = lC;
    }
    lC->push(val);
  }

  template<typename Iter>
  unsigned int push(Iter b, Iter e) {
    int npush;
    for (npush = 0; b != e; npush++)
      push(*b++);
    return npush;
  }

  template<typename RangeTy>
  unsigned int push_initial(const RangeTy& range) {
    auto rp = range.local_pair();
    return push(rp.first, rp.second);
  }

  Galois::optional<value_type> pop() {
    // Find a successful pop
    perItem& p = *current.getLocal();
    CTy* C = p.current;
    if (BlockPeriod && (BlockPeriod < 0 || ((p.numPops++ & ((1ull << BlockPeriod) - 1)) == 0)))
      return slowPop(p);

    Galois::optional<value_type> retval;
    if (C && (retval = C->pop())) {
      return retval;
    }

    // Slow path
    return slowPop(p);
  }
};
GALOIS_WLCOMPILECHECK(SkipListOrderedByIntegerMetric)

} // end namespace WorkList
} // end namespace Galois

#endif
