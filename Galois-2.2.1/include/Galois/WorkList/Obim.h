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
#ifndef GALOIS_WORKLIST_OBIM_H
#define GALOIS_WORKLIST_OBIM_H

#include "Galois/config.h"
#include "Galois/FlatMap.h"
#include "Galois/Timer.h"
#include "Galois/Runtime/PerThreadStorage.h"
#include "Galois/WorkList/Fifo.h"
#include "Galois/WorkList/WorkListHelpers.h"

#include "Galois/Statistic.h"

#include GALOIS_CXX11_STD_HEADER(type_traits)
#include <limits>

#include <iostream>

#ifdef GALOIS_USE_PAPI
#include "Galois/Runtime/Sampling.h"
extern "C" {
#include "/home/syesil2/tools/papi-5.6.0/include/papi.h"
#include "/home/syesil2/tools/papi-5.6.0/include/papiStdEventDefs.h"


}
#endif

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
struct OrderedByIntegerMetric : private boost::noncopyable {
  template<bool _concurrent>
  struct rethread { typedef OrderedByIntegerMetric<Indexer, typename Container::template rethread<_concurrent>::type, BlockPeriod, BSP, uniformBSP, T, Index, _concurrent> type; };

  template<typename _T>
  struct retype { typedef OrderedByIntegerMetric<Indexer, typename Container::template retype<_T>::type, BlockPeriod, BSP, uniformBSP, _T, typename std::result_of<Indexer(_T)>::type, Concurrent> type; };

  template<unsigned _period>
  struct with_block_period { typedef OrderedByIntegerMetric<Indexer, Container, _period, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<typename _container>
  struct with_container { typedef OrderedByIntegerMetric<Indexer, _container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<typename _indexer>
  struct with_indexer { typedef OrderedByIntegerMetric<_indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent> type; };

  template<bool _bsp>
  struct with_back_scan_prevention { typedef OrderedByIntegerMetric<Indexer, Container, BlockPeriod, _bsp, uniformBSP, T, Index, Concurrent> type; };

  typedef T value_type;

private:
  typedef typename Container::template rethread<Concurrent>::type CTy;
  typedef Galois::flat_map<Index, CTy*> LMapTy;
  //typedef Galois::flat_map<Index, std::atomic<unsigned int>> CntrMapTy;
  //typedef std::map<Index, CTy*> LMapTy;

  struct perItem {
    LMapTy local;
    //CntrMapTy counter; // for every push increase counter
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

  // NB: Place dynamically growing masterLog after fixed-size PerThreadStorage
  // members to give higher likelihood of reclaiming PerThreadStorage
  Runtime::PerThreadStorage<perItem> current;
  Runtime::LL::PaddedLock<Concurrent> masterLock;
  Galois::Timer clock;
  MasterLog masterLog;


  //counters per priority
  //CntrMapTy perPriorityCntr;


  Runtime::MM::FixedSizeAllocator heap;
  std::atomic<unsigned int> masterVersion;
  Indexer indexer;


  // random number generation
  Runtime::PerThreadStorage<uint32_t> enqState;
  Runtime::PerThreadStorage<uint32_t> deqState;

  inline uint32_t xorshift32(uint32_t * state){
    uint32_t x = (*state);
    x ^= x << 13;
	  x ^= x >> 17;
	  x ^= x << 5; // see note below about this being a typo in Marsaglia
	  (*state) = x;
    return x;
  }

  bool updateLocal(perItem& p) {
    if (p.lastMasterVersion != masterVersion.load(std::memory_order_relaxed)) {
      //masterLock.lock();
      for (; p.lastMasterVersion < masterVersion.load(std::memory_order_relaxed); ++p.lastMasterVersion) {
        // XXX(ddn): Somehow the second block is better than
        // the first for bipartite matching (GCC 4.7.2)
#if 0
        p.local.insert(masterLog[p.lastMasterVersion]);
#else
        std::pair<Index, CTy*> logEntry = masterLog[p.lastMasterVersion];
        p.local[logEntry.first] = logEntry.second;
        assert(logEntry.second);
#endif
      }
      //masterLock.unlock();
      return true;
    }
    return false;
  }

  GALOIS_ATTRIBUTE_NOINLINE
  Galois::optional<T> slowPop(perItem& p) {
    //Failed, find minimum bin
    updateLocal(p);
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

    for (auto ii = p.local.lower_bound(msS), ee = p.local.end(); ii != ee; ++ii) {
      Galois::optional<T> retval;
      if ((retval = ii->second->pop())) {
        p.current = ii->second;
        p.curIndex = ii->first;
        p.scanStart = ii->first;
        return retval;
      }
    }
    return Galois::optional<value_type>();
  }

  GALOIS_ATTRIBUTE_NOINLINE
  CTy* slowUpdateLocalOrCreate(perItem& p, Index i) {
    //update local until we find it or we get the write lock
    do {
      CTy* lC;
      updateLocal(p);
      if ((lC = p.local[i]))
        return lC;
    } while (!masterLock.try_lock());
    //we have the write lock, update again then create
    updateLocal(p);
    CTy*& lC2 = p.local[i];
    if (!lC2) {
      lC2 = new (heap.allocate(sizeof(CTy))) CTy(i);
      p.lastMasterVersion = masterVersion.load(std::memory_order_relaxed) + 1;
      masterLog.push_back(std::make_pair(i, lC2));
      masterVersion.fetch_add(1);
      (*numberOfPris)+=1;
      //perPriorityCntr[i]=0;
    }
    masterLock.unlock();
    return lC2;
  }

  inline CTy* updateLocalOrCreate(perItem& p, Index i) {
    //Try local then try update then find again or else create and update the master log
    CTy* lC;
    if ((lC = p.local[i]))
      return lC;
    //slowpath
    return slowUpdateLocalOrCreate(p, i);
  }

public:
  static Galois::Statistic* numberOfPris;
#ifdef GALOIS_USE_PAPI
  static Galois::Statistic* enqEvent1;
  static Galois::Statistic* enqEvent2;
  static Galois::Statistic* enqEventNum1;
  static Galois::Statistic* enqEventNum2;

  static Galois::Statistic* deqEvent1;
  static Galois::Statistic* deqEvent2;
  static Galois::Statistic* deqEventNum1;
  static Galois::Statistic* deqEventNum2;


#if (NUM_EVENTS >2 )
  static Galois::Statistic* enqEvent3;
  static Galois::Statistic* deqEvent3;
  static Galois::Statistic* enqEventNum3;
  static Galois::Statistic* deqEventNum3;
#endif 
#if (NUM_EVENTS >3 )
  static Galois::Statistic* enqEvent4;
  static Galois::Statistic* deqEvent4;
  static Galois::Statistic* enqEventNum4;
  static Galois::Statistic* deqEventNum4;
#endif
#endif
  OrderedByIntegerMetric(const Indexer& x = Indexer()): heap(sizeof(CTy)), masterVersion(0), indexer(x) {
    clock.start();
    if(numberOfPris == 0){
      numberOfPris = new Galois::Statistic("numberOfPris");
      (*numberOfPris )=0;
      std::cout<<"Number of Pris statistics created\n";
    }
#ifdef GALOIS_USE_PAPI
    srand (time(NULL));
    for (unsigned i = 0; i < Runtime::activeThreads; ++i){
      (*enqState.getRemote(i)) = rand();
      (*deqState.getRemote(i)) = rand();
    }
    if(enqEvent1 == 0){
      std::string s = papiNames[0];
      std::string name = s+ "_enq";
      enqEvent1 = new Galois::Statistic(name);
      (*enqEvent1 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(enqEvent2 == 0){
      std::string s = papiNames[1];
      std::string name = s+ "_enq";
      enqEvent2 = new Galois::Statistic(name);
      (*enqEvent2 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(enqEventNum1 == 0){
      std::string s = papiNames[0];
      std::string name = s+ "_enq_num";
      enqEventNum1 = new Galois::Statistic(name);
      (*enqEventNum1 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(enqEventNum2 == 0){
      std::string s = papiNames[1];
      std::string name = s+ "_enq_num";
      enqEventNum2 = new Galois::Statistic(name);
      (*enqEventNum2 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }

    if(deqEvent1 == 0){
      std::string s = papiNames[0];
      std::string name = s+ "_deq";
      deqEvent1 = new Galois::Statistic(name);
      (*deqEvent1 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEvent2 == 0){
      std::string s = papiNames[1];
      std::string name = s+ "_deq";
      deqEvent2 = new Galois::Statistic(name);
      (*deqEvent2 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEventNum1 == 0){
      std::string s = papiNames[0];
      std::string name = s+ "_deq_num";
      deqEventNum1 = new Galois::Statistic(name);
      (*deqEventNum1 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEventNum2 == 0){
      std::string s = papiNames[1];
      std::string name = s+ "_deq_num";
      deqEventNum2 = new Galois::Statistic(name);
      (*deqEventNum2 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }


#if (NUM_EVENTS > 2)
    if(enqEvent3 == 0){
      std::string s = papiNames[2];
      std::string name = s+ "_enq";
      enqEvent3 = new Galois::Statistic(name);
      (*enqEvent3 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEvent3 == 0){
      std::string s = papiNames[2];
      std::string name = s+ "_deq";
      deqEvent3 = new Galois::Statistic(name);
      (*deqEvent3 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(enqEventNum3 == 0){
      std::string s = papiNames[2];
      std::string name = s+ "_enq_num";
      enqEventNum3 = new Galois::Statistic(name);
      (*enqEventNum3 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEventNum3 == 0){
      std::string s = papiNames[2];
      std::string name = s+ "_deq_num";
      deqEventNum3 = new Galois::Statistic(name);
      (*deqEventNum3 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
#endif
    
#if (NUM_EVENTS>3)
    if(enqEvent4 == 0){
      std::string s = papiNames[3];
      std::string name = s+ "_enq";
      enqEvent4 = new Galois::Statistic(name);
      (*enqEvent4 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEvent4 == 0){
      std::string s = papiNames[3];
      std::string name = s+ "_deq";
      deqEvent4 = new Galois::Statistic(name);
      (*deqEvent4 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(enqEventNum4 == 0){
      std::string s = papiNames[3];
      std::string name = s+ "_enq_num";
      enqEventNum4 = new Galois::Statistic(name);
      (*enqEventNum4 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
    if(deqEventNum4 == 0){
      std::string s = papiNames[3];
      std::string name = s+ "_deq_num";
      deqEventNum4 = new Galois::Statistic(name);
      (*deqEventNum4 )=0;
      // std::cout<<"Number of Pris statistics created\n";
    }
#endif    
#endif
  }

  ~OrderedByIntegerMetric() {
    perItem& p = *current.getLocal();
    updateLocal(p);
    // Deallocate in LIFO order to give opportunity for simple garbage
    // collection
    //Print stats for priroity counts here
    for (auto ii = masterLog.rbegin(), ei = masterLog.rend(); ii != ei; ++ii) {
      CTy* lC = ii->second;
      lC->~CTy();
      heap.deallocate(lC);
    }

    if(numberOfPris!=0){
      std::cout<<"Number of Pris statistics dealloced\n";
      delete numberOfPris;
      //numberOfPris=NULL;
    }
#ifdef GALOIS_USE_PAPI
    if(enqEvent1 != 0){
      delete enqEvent1 ;
     
    }
    if(enqEvent2 != 0){
      delete enqEvent2 ;
    }
    if(enqEventNum1 != 0){
      delete enqEventNum1 ;
    }
    if(enqEventNum2 != 0){
      delete enqEventNum2 ;
    }

    if(deqEvent1 != 0){
      delete deqEvent1 ;
    }
    if(deqEvent2 != 0){
      delete deqEvent2 ;
    }
    if(deqEventNum1 != 0){
      delete deqEventNum1 ;
    }
    if(deqEventNum2 != 0){
      delete deqEventNum2 ;
    }

#if (NUM_EVENTS > 2)
    if(enqEvent3 != 0){
      delete enqEvent3 ;
    }
    if(deqEvent3 != 0){
      delete deqEvent3 ;
    }
    if(enqEventNum3 != 0){
      delete enqEventNum3 ;
    }
    if(deqEventNum3 != 0){
      delete deqEventNum3 ;
    }
#endif
#if (NUM_EVENTS > 3)    
    if(enqEvent4 != 0){
      delete enqEvent4 ;
    }
    if(deqEvent4 != 0){
      delete deqEvent4 ;
    }
    if(enqEventNum4 != 0){
      delete enqEventNum4 ;
    }
    if(deqEventNum4 != 0){
      delete deqEventNum4 ;
    }
#endif    
#endif
  }

  void push(const value_type& val){
  #ifdef GALOIS_USE_PAPI
    long_long values1[NUM_EVENTS];
    long_long values2[NUM_EVENTS];
    int num_counters=NUM_EVENTS;
    int a;
    unsigned int rand_num = xorshift32(enqState.getLocal())%DIVIDER;
    if( rand_num == 0 ){
    // if( papi::isEventSetInit ){
          // std::cout<<"-- "<<papi::isEventSetInit<<std::endl;

      a = PAPI_read(papiEventSet, values1);
      if( a != PAPI_OK){
        
        std::cout<<"error "<<PAPI_strerror(a)<<std::endl;
      }
    }
    // }
  #endif

    push_profiled(val);

  #ifdef GALOIS_USE_PAPI
    // if( papi::isEventSetInit ){
    if( rand_num == 0){
      a = PAPI_read(papiEventSet, values2);
      if( a != PAPI_OK){
        
        std::cout<<"error "<<PAPI_strerror(a)<<std::endl;
      }
      if(values1[0]<values2[0]){
        // assert((*enqEvent1)<=(*enqEvent1)+(values2[0]-values1[0]));

        (*enqEventNum1)+=1;
        (*enqEvent1) += (values2[0]-values1[0]);
      }
      if(values1[1]<values2[1]){
        // assert((*enqEvent2)<=(*enqEvent2)+(values2[1]-values1[1]));

        (*enqEventNum2)+=1;
        (*enqEvent2) += (values2[1]-values1[1]);
      }
    }
#if (NUM_EVENTS>2)
    if(values1[2]<values2[2]){
      // assert((*deqEvent1)<=(*deqEvent1)+(values2[0]-values1[0]));
      (*enqEventNum3)+=1;
      (*enqEvent3) += (values2[2]-values1[2]);
    }
#endif
#if (NUM_EVENTS>3)
    if(values1[3]<values2[3]){
      // assert((*deqEvent1)<=(*deqEvent1)+(values2[0]-values1[0]));
      (*enqEventNum4)+=1;
      (*enqEvent4) += (values2[3]-values1[3]);
    }
#endif
    // }
#endif
  }

  inline void push_profiled(const value_type& val) {

    Index index = indexer(val);
    perItem& p = *current.getLocal();

    //perPriorityCntr[index].fetch_add(1);
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
#ifdef GALOIS_USE_PAPI

    long_long values1[NUM_EVENTS];
    long_long values2[NUM_EVENTS];
    int num_counters=NUM_EVENTS;
    int a;
    unsigned int rand_num = xorshift32(deqState.getLocal())%DIVIDER;
    if( rand_num == 0 ){
    // if( papi::isEventSetInit ){
          // std::cout<<"-- "<<papi::isEventSetInit<<std::endl;

      a = PAPI_read(papiEventSet, values1);
      if( a != PAPI_OK){
        
        std::cout<<"error "<<PAPI_strerror(a)<<std::endl;
      }
    }
    // }
  #endif

    Galois::optional<value_type> returnValue = pop_profiled();

#ifdef GALOIS_USE_PAPI
    // if( papi::isEventSetInit ){
  if( rand_num == 0){
    a = PAPI_read(papiEventSet, values2);
    if( a != PAPI_OK){
      
      std::cout<<"error "<<PAPI_strerror(a)<<std::endl;
    }
    if(values1[0]<values2[0]){
      // assert((*deqEvent1)<=(*deqEvent1)+(values2[0]-values1[0]));
      (*deqEventNum1)+=1;
      (*deqEvent1) += (values2[0]-values1[0]);
    }
    if(values1[1]<values2[1]){
      // assert((*deqEvent2)<=(*deqEvent2)+(values2[1]-values1[1]));
      (*deqEventNum2)+=1;
      (*deqEvent2) += (values2[1]-values1[1]);
    }
  }
#if (NUM_EVENTS>2)
    if(values1[2]<values2[2]){
      // assert((*deqEvent1)<=(*deqEvent1)+(values2[0]-values1[0]));
      (*deqEventNum3)+=1;
      (*deqEvent3) += (values2[2]-values1[2]);
    }
#endif
#if (NUM_EVENTS>3)
    if(values1[3]<values2[3]){
      // assert((*deqEvent1)<=(*deqEvent1)+(values2[0]-values1[0]));
      (*deqEventNum4)+=1;
      (*deqEvent4) += (values2[3]-values1[3]);
    }
#endif
    // }
#endif

    return returnValue;
  }

  inline Galois::optional<value_type> pop_profiled() {

    // Find a successful pop
    perItem& p = *current.getLocal();
    CTy* C = p.current;
    if (BlockPeriod && (BlockPeriod < 0 || (p.numPops++ & (((1ull << BlockPeriod) - 1) == 0))))
      return slowPop(p);

    Galois::optional<value_type> retval;
    if (C && (retval = C->pop())) {
      return retval;
    }

    // Slow path
    Galois::optional<value_type> returnValue =  slowPop(p);


    return returnValue;
  }
};
GALOIS_WLCOMPILECHECK(OrderedByIntegerMetric)
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::numberOfPris;
#ifdef GALOIS_USE_PAPI
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEvent1;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEvent2;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEventNum1;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEventNum2;

template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEvent1;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEvent2;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEventNum1;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEventNum2;


#if (NUM_EVENTS > 2)
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEvent3;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEvent3;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEventNum3;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEventNum3;

#endif

#if (NUM_EVENTS>3)
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEvent4;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEvent4;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::enqEventNum4;
template<class Indexer, typename Container,
  int BlockPeriod,
  bool BSP,
  bool uniformBSP,
  typename T,
  typename Index,
  bool Concurrent>
Statistic* OrderedByIntegerMetric<Indexer, Container, BlockPeriod, BSP, uniformBSP, T, Index, Concurrent>::deqEventNum4;
#endif
#endif
} // end namespace WorkList
} // end namespace Galois

#endif
