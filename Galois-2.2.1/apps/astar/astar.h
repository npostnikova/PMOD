/** Single source shortest paths -*- C++ -*-
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
 * @section Description
 *
 * Single source shortest paths.
 *
 * @author Andrew Lenharth <andrewl@lenharth.org>
 */
#ifndef APPS_SSSP_SSSP_H
#define APPS_SSSP_SSSP_H

#include "llvm/Support/CommandLine.h"

#include <limits>
#include <string>
#include <sstream>
#include <stdint.h>

typedef unsigned long Dist;
typedef int Coord;
static const Dist DIST_INFINITY = std::numeric_limits<Dist>::max() - 1;

template<typename GrNode>
struct UpdateRequestCommon {
  GrNode n;
  Dist w;

  UpdateRequestCommon(const GrNode& N, Dist W): n(N), w(W) {}

  UpdateRequestCommon(): n(), w(0) {}

  bool operator>(const UpdateRequestCommon& rhs) const {
    if (w > rhs.w) return true;
    if (w < rhs.w) return false;
    return n > rhs.n;
  }

  bool operator<(const UpdateRequestCommon& rhs) const {
    if (w < rhs.w) return true;
    if (w > rhs.w) return false;
    return n < rhs.n;
  }

  bool operator!=(const UpdateRequestCommon& other) const {
    if (w != other.w) return true;
    return n != other.n;
  }

  bool operator==(const UpdateRequestCommon& other) const {
    return w == other.w && n == other.n;
  }

  uintptr_t getID() const {
    return reinterpret_cast<uintptr_t>(n);
  }

  unsigned int operator() () const {
    return w;
  }
};

struct SNode {
  Dist dist;
  Coord x;
  Coord y;

  std::atomic<uint64_t> index = {0};
};

template <typename WorkItem>
struct DecreaseKeyIndexer {
  static int get_queue(WorkItem const& wi) {
    return get_pair(wi).first;
  }

  static void set_pair(WorkItem const& wi, int q, uint32_t ind) {
    wi.n->getData().index.store((int64_t (ind) << 32) | (uint32_t (q + 1)), std::memory_order_release);
  }

  static std::pair<int, uint32_t> get_pair(WorkItem const& wi) {
    auto index = wi.n->getData().index.load(std::memory_order_acquire);
    static const uint32_t& mask = (1ull << 32) - 1;
    int q = index & mask;
    return {q - 1, index >> 32};
  }
};

template<typename Graph>
void readInOutGraph(Graph& graph);

Dist calculate_heu(Coord x1, Coord y1, Coord x2, Coord y2){
  return sqrt(pow(abs(x1-x2),2)+pow(abs(y1-y2),2));
}
extern llvm::cl::opt<unsigned int> memoryLimit;


#endif
