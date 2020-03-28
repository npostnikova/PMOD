/** Galois scheduler and runtime -*- C++ -*-
 * @file
 * @section License
 *
 * Galois, a framework to exploit amorphous data-parallelism in irregular
 * programs.
 *
 * Copyright (C) 2012, The University of Texas at Austin. All rights reserved.
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
 * @section Description
 *
 * Implementation of the Galois foreach iterator. Includes various
 * specializations to operators to reduce runtime overhead.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef GALOIS_RUNTIME_PARALLELWORK_H
#define GALOIS_RUNTIME_PARALLELWORK_H

#include "Galois/Mem.h"
#include "Galois/Statistic.h"
#include "Galois/Runtime/Barrier.h"
#include "Galois/Runtime/Context.h"
#include "Galois/Runtime/ForEachTraits.h"
#include "Galois/Runtime/Range.h"
#include "Galois/Runtime/Support.h"
#include "Galois/Runtime/Termination.h"
#include "Galois/Runtime/ThreadPool.h"
#include "Galois/Runtime/UserContextAccess.h"
#include "Galois/WorkList/GFifo.h"

#include <algorithm>
#include <functional>
#include <cmath>

#ifdef GALOIS_USE_HTM
#include <speculation.h>
#endif

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace Galois {
//! Internal Galois functionality - Use at your own risk.
namespace Runtime {
namespace {

template<bool Enabled>
class LoopStatistics {
  unsigned long conflicts;
  unsigned long iterations;
  unsigned long galois_time;
  unsigned long user_time;
  unsigned long conflict_time;
  unsigned long push_time;
  unsigned long npush;
  unsigned long pop_time;
  unsigned long npop;
  unsigned long empty_pop_time;
  unsigned long nempty_pop;
unsigned long comp_cycles;
  const char* loopname;

#ifdef GALOIS_USE_HTM
  TmReport_s start;
  void init() {
    if (LL::getTID()) return;

    // Dummy transaction to ensure that tm_get_all_stats doesn't return
    // garbage
#pragma tm_atomic
    {
      conflicts = 0;
    }

    tm_get_all_stats(&start);
  }

  void report() {
    if (LL::getTID()) return;
    TmReport_s stop;
    tm_get_all_stats(&stop);
    reportStat(loopname, "HTMTransactions",
        stop.totalTransactions - start.totalTransactions);
    reportStat(loopname, "HTMRollbacks",
        stop.totalRollbacks - start.totalRollbacks);
    reportStat(loopname, "HTMSerializedJMV",
        stop.totalSerializedJMV - start.totalSerializedJMV);
    reportStat(loopname, "HTMSerializedMAXRB",
        stop.totalSerializedMAXRB - start.totalSerializedMAXRB);
    reportStat(loopname, "HTMSerializedOTHER",
        stop.totalSerializedOTHER - start.totalSerializedOTHER);
    tm_print_stats();
  }
#else
  void init() { }
  void report() { }
#endif

public:
  explicit LoopStatistics(const char* ln) :conflicts(0), iterations(0), galois_time(0),
                                           user_time(0), conflict_time(0),
                                           push_time(0), npush(0),
                                           pop_time(0), npop(0),
                                           empty_pop_time(0), nempty_pop(0), comp_cycles(0),
                                           loopname(ln) { init(); }
  ~LoopStatistics() {
    reportStat(loopname, "Conflicts", conflicts);
    reportStat(loopname, "Iterations", iterations);
    reportStat(loopname, "GaloisTime", galois_time);
    reportStat(loopname, "UserTime", user_time);
    reportStat(loopname, "ConflictTime", conflict_time);
    reportStat(loopname, "PushTime", push_time);
    reportStat(loopname, "PopTime", pop_time);
    reportStat(loopname, "EmptyPopTime", empty_pop_time);
    reportStat(loopname, "nPush", npush);
    reportStat(loopname, "nPop", npop);
    reportStat(loopname, "nEmptyPop", nempty_pop);
    //reportStat(loopname, "CompCycles", comp_cycles);
    report();
  }
  inline void inc_iterations(int amount = 1) {
    iterations += amount;
  }
  inline void inc_conflicts() {
    ++conflicts;
  }
  inline void add_galois_time(unsigned long t) {
    galois_time += t;
  }
  inline void add_user_time(unsigned long t) {
    user_time += t;
  }
  inline void add_conflict_time(unsigned long t) {
    conflict_time += t;
  }
  inline void add_push_time(unsigned long t, unsigned int n) {
    push_time += t;
    npush += n;
  }
  inline void add_pop_time(unsigned long t) {
    pop_time += t;
    npop++;
  }
  inline void add_empty_pop_time(unsigned long t) {
    empty_pop_time += t;
    nempty_pop++;
  }
  inline void add_comp_time(unsigned long t) {
  comp_cycles += t;
}
};


template <>
class LoopStatistics<false> {
public:
  explicit LoopStatistics(const char* ln) {}
  inline void inc_iterations(int amount = 1) const { }
  inline void inc_conflicts() const { }
  inline void add_galois_time(unsigned long t) const { }
  inline void add_user_time(unsigned long t) const { }
  inline void add_conflict_time(unsigned long t) const { }
  inline void add_push_time(unsigned long t, unsigned int n) const { }
  inline void add_pop_time(unsigned long t) const { }
  inline void add_empty_pop_time(unsigned long t) const { }
  inline void add_comp_time(unsigned long t) const {}
};

template<typename value_type>
class AbortHandler {
  struct Item { value_type val; int retries; };

  typedef WorkList::GFIFO<Item> AbortedList;
  PerThreadStorage<AbortedList> queues;
  bool useBasicPolicy;

  /**
   * Policy: serialize via tree over packages.
   */
  void basicPolicy(const Item& item) {
    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
  }

  /**
   * Policy: retry work 2X locally, then serialize via tree on package (trying
   * twice at each level), then serialize via tree over packages.
   */
  void doublePolicy(const Item& item) {
    int retries = item.retries - 1;
    if ((retries & 1) == 1) {
      queues.getLocal()->push(item);
      return;
    }

    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    unsigned leader = LL::getLeaderForPackage(package);
    if (tid != leader) {
      unsigned next = leader + (tid - leader) / 2;
      queues.getRemote(next)->push(item);
    } else {
      queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
    }
  }

  /**
   * Policy: retry work 2X locally, then serialize via tree on package but
   * try at most 3 levels, then serialize via tree over packages.
   */
  void boundedPolicy(const Item& item) {
    int retries = item.retries - 1;
    if (retries < 2) {
      queues.getLocal()->push(item);
      return;
    }

    unsigned tid = LL::getTID();
    unsigned package = LL::getPackageForSelf(tid);
    unsigned leader = LL::getLeaderForPackage(package);
    if (retries < 5 && tid != leader) {
      unsigned next = leader + (tid - leader) / 2;
      queues.getRemote(next)->push(item);
    } else {
      queues.getRemote(LL::getLeaderForPackage(package / 2))->push(item);
    }
  }

  /**
   * Retry locally only.
   */
  void eagerPolicy(const Item& item) {
    queues.getLocal()->push(item);
  }

public:
  AbortHandler() {
    // XXX(ddn): Implement smarter adaptive policy
    useBasicPolicy = LL::getMaxPackages() > 2;
  }

  value_type& value(Item& item) const { return item.val; }
  value_type& value(value_type& val) const { return val; }

  void push(const value_type& val) {
    Item item = { val, 1 };
    queues.getLocal()->push(item);
  }

  void push(const Item& item) {
    Item newitem = { item.val, item.retries + 1 };
    if (useBasicPolicy)
      basicPolicy(newitem);
    else
      doublePolicy(newitem);
  }

  AbortedList* getQueue() { return queues.getLocal(); }
};

template<class WorkListTy, class T, class FunctionTy>
class ForEachWork {
protected:
  typedef T value_type;
  typedef typename WorkListTy::template retype<value_type>::type WLTy;

  struct ThreadLocalData {
    FunctionTy function;
    UserContextAccess<value_type> facing;
    SimpleRuntimeContext ctx;
    LoopStatistics<ForEachTraits<FunctionTy>::NeedsStats> stat;
    ThreadLocalData(const FunctionTy& fn, const char* ln): function(fn), stat(ln) {}
  };

  // NB: Place dynamically growing wl after fixed-size PerThreadStorage
  // members to give higher likelihood of reclaiming PerThreadStorage

  AbortHandler<value_type> aborted;
  TerminationDetection& term;

  WLTy wl;
  FunctionTy& origFunction;
  const char* loopname;
  bool broke;

  inline void commitIteration(ThreadLocalData& tld) {
    if (ForEachTraits<FunctionTy>::NeedsPush) {
      auto ii = tld.facing.getPushBuffer().begin();
      auto ee = tld.facing.getPushBuffer().end();
      if (ii != ee) {
	tld.stat.add_galois_time(tld.facing.t.stopwatch());
	unsigned int npush = wl.push(ii, ee);
	tld.stat.add_push_time(tld.facing.t.stopwatch(), npush);
	tld.facing.resetPushBuffer();
      }
    }
    if (ForEachTraits<FunctionTy>::NeedsPIA)
      tld.facing.resetAlloc();
    if (ForEachTraits<FunctionTy>::NeedsAborts)
      tld.ctx.commitIteration();
  }

  template<typename Item>
  GALOIS_ATTRIBUTE_NOINLINE
  void abortIteration(const Item& item, ThreadLocalData& tld) {
    assert(ForEachTraits<FunctionTy>::NeedsAborts);
    tld.ctx.cancelIteration();
    tld.stat.inc_conflicts(); //Class specialization handles opt
    aborted.push(item);
    tld.stat.add_conflict_time(tld.facing.t.stopwatch());
    //clear push buffer
    if (ForEachTraits<FunctionTy>::NeedsPush)
      tld.facing.resetPushBuffer();
    //reset allocator
    if (ForEachTraits<FunctionTy>::NeedsPIA)
      tld.facing.resetAlloc();
  }

#ifdef GALOIS_USE_HTM
# ifndef GALOIS_USE_LONGJMP
#  error "HTM must be used with GALOIS_USE_LONGJMP"
# endif
#endif

  inline void doProcess(value_type& val, ThreadLocalData& tld) {
    tld.stat.inc_iterations();
    if (ForEachTraits<FunctionTy>::NeedsAborts)
      tld.ctx.startIteration();

    tld.facing.u = 0;
    tld.stat.add_galois_time(tld.facing.t.stopwatch());

#ifdef GALOIS_USE_HTM
# ifndef GALOIS_USE_LONGJMP
#  error "HTM must be used with GALOIS_USE_LONGJMP"
# endif
#pragma tm_atomic
    {
#endif
      tld.function(val, tld.facing.data());
#ifdef GALOIS_USE_HTM
    }
#endif
    tld.facing.u += tld.facing.t.stopwatch();
    tld.stat.add_user_time(tld.facing.u);

    clearReleasable();
    commitIteration(tld);
  }

  bool runQueueSimple(ThreadLocalData& tld) {
    bool workHappened = false;
    tld.stat.add_galois_time(tld.facing.t.stopwatch());
    Galois::optional<value_type> p = wl.pop();
    if (p) {
      tld.stat.add_pop_time(tld.facing.t.stopwatch());
      workHappened = true;
    } else {
      tld.stat.add_empty_pop_time(tld.facing.t.stopwatch());
    }
    while (p) {
      doProcess(*p, tld);
      tld.stat.add_galois_time(tld.facing.t.stopwatch());
      p = wl.pop();
      if (p)
        tld.stat.add_pop_time(tld.facing.t.stopwatch());
      else
        tld.stat.add_empty_pop_time(tld.facing.t.stopwatch());
    }
    return workHappened;
  }

  template<int limit, typename WL>
  bool runQueue(ThreadLocalData& tld, WL& lwl) {
    bool workHappened = false;
    tld.stat.add_galois_time(tld.facing.t.stopwatch());
    Galois::optional<typename WL::value_type> p = lwl.pop();
    if (p)
      tld.stat.add_pop_time(tld.facing.t.stopwatch());
    else
      tld.stat.add_empty_pop_time(tld.facing.t.stopwatch());
    unsigned num = 0;
    int result = 0;
    if (p)
      workHappened = true;
#ifdef GALOIS_USE_LONGJMP
    if ((result = setjmp(hackjmp)) == 0) {
#else
    try {
#endif
      while (p) {
	doProcess(aborted.value(*p), tld);
	if (limit) {
	  ++num;
	  if (num == limit)
	    break;
	}
	tld.stat.add_galois_time(tld.facing.t.stopwatch());
	p = lwl.pop();
	if (p)
          tld.stat.add_pop_time(tld.facing.t.stopwatch());
        else
          tld.stat.add_empty_pop_time(tld.facing.t.stopwatch());
      }
#ifdef GALOIS_USE_LONGJMP
    } else {
      clearReleasable();
      clearConflictLock();
    }
#else
    } catch (ConflictFlag const& flag) {
      clearReleasable();
      clearConflictLock();
      result = flag;
    }
#endif
    switch (result) {
    case 0:
      break;
    case CONFLICT:
      abortIteration(*p, tld);
      break;
    default:
      GALOIS_DIE("unknown conflict type");
    }
    return workHappened;
  }

  GALOIS_ATTRIBUTE_NOINLINE
  bool handleAborts(ThreadLocalData& tld) {
    return runQueue<0>(tld, *aborted.getQueue());
  }

  void fastPushBack(ThreadLocalData& tld, typename UserContextAccess<value_type>::PushBufferTy& x) {
    tld.facing.u += tld.facing.t.stopwatch();
    unsigned int npush = wl.push(x.begin(), x.end());
    tld.stat.add_push_time(tld.facing.t.stopwatch(), npush);
    x.clear();
  }

  template<bool couldAbort, bool isLeader>
  void go() {
    Timer tt(true);
    Timer tt_comp(true);
    // Thread-local data goes on the local stack to be NUMA friendly
    ThreadLocalData tld(origFunction, loopname);
    tld.facing.setBreakFlag(&broke);
    tld.facing.t = tt;
    if (couldAbort)
      setThreadContext(&tld.ctx);
    if (ForEachTraits<FunctionTy>::NeedsPush && !couldAbort)
      tld.facing.setFastPushBack(
          std::bind(&ForEachWork::fastPushBack, std::ref(*this), std::ref(tld), std::placeholders::_1));
    bool didWork;
    do {
      didWork = false;
      tt_comp.stopwatch();
      // Run some iterations
      if (couldAbort || ForEachTraits<FunctionTy>::NeedsBreak) {
        if (isLeader)
          didWork = runQueue<32>(tld, wl);
        else
          didWork = runQueue<ForEachTraits<FunctionTy>::NeedsBreak ? 32 : 0>(tld, wl);
        // Check for abort
        if (couldAbort)
          didWork |= handleAborts(tld);
      } else { // No try/catch
        didWork = runQueueSimple(tld);
      }
      tld.stat.add_comp_time(tt_comp.stopwatch());
      // Update node color and prop token
      term.localTermination(didWork);
    } while (!term.globalTermination() && (!ForEachTraits<FunctionTy>::NeedsBreak || !broke));

    tld.stat.add_galois_time(tld.facing.t.stopwatch());
    tld.facing.t.stop();
    if (couldAbort)
      setThreadContext(0);
  }

public:
  ForEachWork(FunctionTy& f, const char* l): term(getSystemTermination()), origFunction(f), loopname(l), broke(false) { }

  template<typename W>
  ForEachWork(W& w, FunctionTy& f, const char* l): term(getSystemTermination()), wl(w), origFunction(f), loopname(l), broke(false) { }

  template<typename RangeTy>
  void AddInitialWork(const RangeTy& range) {
    Timer t(true);
    int npush = wl.push_initial(range);
    t.stop();
    reportStat(loopname, "InitPushTime", t.get());
    reportStat(loopname, "nPushInit", npush);
  }

  void initThread(void) {
    term.initializeThread();
  }

  void invokeBarrier(Barrier &barrier) {
    barrier.before();
    barrier();
    barrier.after(loopname);
  }

  void operator()() {
    bool isLeader = LL::isPackageLeaderForSelf(LL::getTID());
    bool couldAbort = ForEachTraits<FunctionTy>::NeedsAborts && activeThreads > 1;
#ifdef GALOIS_USE_HTM
    couldAbort = false;
#endif
    StatTimer WorkTime("Work", loopname, start_now);

    if (couldAbort && isLeader)
      go<true, true>();
    else if (couldAbort && !isLeader)
      go<true, false>();
    else if (!couldAbort && isLeader)
      go<false, true>();
    else
      go<false, false>();

    WorkTime.stop();
  }
};


static unsigned long clock_usec() {
  struct timespec ts;

  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) < 0)
    GALOIS_SYS_DIE("clock_gettime");

  return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}


static unsigned long usertime_usec() {
  struct rusage ru;

  if (getrusage(RUSAGE_SELF, &ru) < 0)
    GALOIS_SYS_DIE("getrusage");

  return ru.ru_utime.tv_sec * 1000000ull + ru.ru_utime.tv_usec;
}

static unsigned long systime_usec() {
  struct rusage ru;

  if (getrusage(RUSAGE_SELF, &ru) < 0)
    GALOIS_SYS_DIE("getrusage");

  return ru.ru_stime.tv_sec * 1000000ull + ru.ru_stime.tv_usec;
}

static unsigned long getrusage_granularity() {
  struct timespec ts;

  if (clock_getres(CLOCK_REALTIME_COARSE, &ts) < 0)
    GALOIS_SYS_DIE("clock_getres");

  return ts.tv_sec * 1000000ull + ts.tv_nsec / 1000;
}

template<typename WLTy, typename RangeTy, typename FunctionTy>
void for_each_impl(const RangeTy& range, FunctionTy f, const char* loopname) {
  if (inGaloisForEach)
    GALOIS_DIE("Nested for_each not supported");

  inGaloisForEach = true;

  typedef typename RangeTy::value_type T;
  typedef ForEachWork<WLTy, T, FunctionTy> WorkTy;

  // NB: Initialize barrier before creating WorkTy to increase
  // PerThreadStorage reclaimation likelihood
  Barrier& barrier = getSystemBarrier();

  WorkTy W(f, loopname);

  unsigned long t0 = clock_usec();
  unsigned long u0 = usertime_usec();
  unsigned long s0 = systime_usec();

  StatTimer LoopTimer("LoopTime", loopname);
  if (ForEachTraits<FunctionTy>::NeedsStats)
    LoopTimer.start();

  RunCommand w[5] = {
    std::bind(&WorkTy::initThread, std::ref(W)),
    std::bind(&WorkTy::template AddInitialWork<RangeTy>, std::ref(W), range),
    std::bind(&WorkTy::invokeBarrier, std::ref(W), std::ref(barrier)),
    std::ref(W),
    std::bind(&WorkTy::invokeBarrier, std::ref(W), std::ref(barrier))
  };
  getSystemThreadPool().run(&w[0], &w[5], activeThreads);
  if (ForEachTraits<FunctionTy>::NeedsStats)
    LoopTimer.stop();

  unsigned long du = usertime_usec() - u0;
  unsigned long ds = systime_usec() - s0;
  unsigned long dt = clock_usec() - t0;

  reportStat(loopname, "RusageUser", du);
  reportStat(loopname, "RusageSys", ds);

  double sysfrac = (double) ds / (ds+du);
  if (sysfrac > 0.1)
    printf("INVALID RUN?  Loop %s %.3f%% system time\n", loopname, 100*sysfrac);
  double restime = (double) (du + ds) / activeThreads;
  double tfrac = (double) dt / restime;
  if (fabs(dt - restime) > 2*getrusage_granularity() && fabs(tfrac - 1.0) > 0.1)
    printf("INVALID RUN?  Loop %s possible co-runner, execution took %.3f than expected (systime=%ld usertime=%ld measured=%ld\n", loopname, tfrac, ds, du, dt);
  inGaloisForEach = false;
}

template<typename FunctionTy>
struct WOnEach {
  FunctionTy& origFunction;
  WOnEach(FunctionTy& f): origFunction(f) { }
  void operator()(void) {
    FunctionTy fn(origFunction);
    fn(LL::getTID(), activeThreads);
  }
};

template<typename FunctionTy>
void on_each_impl(FunctionTy fn, const char* loopname = 0) {
  if (inGaloisForEach)
    GALOIS_DIE("Nested for_each not supported");

  inGaloisForEach = true;
  RunCommand w[2] = {WOnEach<FunctionTy>(fn),
		     std::ref(getSystemBarrier())};
  getSystemThreadPool().run(&w[0], &w[2], activeThreads);
  inGaloisForEach = false;
}

//! on each executor with simple barrier.
template<typename FunctionTy>
void on_each_simple_impl(FunctionTy fn, const char* loopname = 0) {
  if (inGaloisForEach)
    GALOIS_DIE("Nested for_each not supported");

  inGaloisForEach = true;
  Barrier* b = createSimpleBarrier();
  b->reinit(activeThreads);
  RunCommand w[2] = {WOnEach<FunctionTy>(fn),
		     std::ref(*b)};
  getSystemThreadPool().run(&w[0], &w[2], activeThreads);
  delete b;
  inGaloisForEach = false;
}

} // end namespace anonymous

void preAlloc_impl(int num);

} // end namespace Runtime
} // end namespace Galois

#endif
