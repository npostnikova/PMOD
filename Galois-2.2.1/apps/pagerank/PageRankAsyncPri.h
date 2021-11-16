/** Page rank application -*- C++ -*-
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
 * @author Joyce Whang <joyce@cs.utexas.edu>
 * @author Andrew Lenharth <andrewl@lenharth.org>
 * @author Donald Nguyen <ddn@cs.utexas.edu>
 */
#include "Galois/Galois.h"
#include "chunk_size.h"
#include <string>
typedef double PRTy;
extern bool outOnly;
extern std::string worklistname_;


template<typename Graph>
unsigned nout(Graph& g, typename Graph::GraphNode n, Galois::MethodFlag flag) {
  return std::distance(g.edge_begin(n, flag), g.edge_end(n, flag));
}

template<typename Graph>
unsigned ninout(Graph& g, typename Graph::GraphNode n, Galois::MethodFlag flag) {
  return std::distance(g.in_edge_begin(n, flag), g.in_edge_end(n, flag)) + nout(g, n, flag);
}

template<typename Graph>
double computePageRankInOut(Graph& g, typename Graph::GraphNode src, int prArg, Galois::MethodFlag lockflag) {
  double sum = 0;
  for (auto jj = g.in_edge_begin(src, lockflag), ej = g.in_edge_end(src, lockflag); jj != ej; ++jj) {
    auto dst = g.getInEdgeDst(jj);
    auto& ddata = g.getData(dst, lockflag);
    sum += ddata.getPageRank(prArg) / nout(g, dst, lockflag);
  }
  return alpha2*sum + (1.0 - alpha2);
}

template<typename Graph>
void initResidual(Graph& graph) {
  Galois::do_all_local(graph, [&graph] (const typename Graph::GraphNode& src) {
      auto& data = graph.getData(src);
      // for each in-coming neighbour, add residual
      PRTy sum = 0.0;
      for (auto jj = graph.in_edge_begin(src), ej = graph.in_edge_end(src); jj != ej; ++jj){
        auto dst = graph.getInEdgeDst(jj);
        sum += 1.0/nout(graph, dst, Galois::MethodFlag::NONE);
      }
      data.residual = sum * alpha2 * (1.0-alpha2);
    }, Galois::do_all_steal(true));
}

template<typename Graph, typename PriFn>
void initResidual(Graph& graph, Galois::InsertBag<std::pair<typename Graph::GraphNode, int> >& b, const PriFn& pri) {
  Galois::do_all_local(graph, [&graph, &b, &pri] (const typename Graph::GraphNode& src) {
      auto& data = graph.getData(src);
      // for each in-coming neighbour, add residual
      PRTy sum = 0.0;
      for (auto jj = graph.in_edge_begin(src), ej = graph.in_edge_end(src); jj != ej; ++jj){
        auto dst = graph.getInEdgeDst(jj);
        auto& ddata = graph.getData(dst);
        sum += 1.0/nout(graph,dst, Galois::MethodFlag::NONE);
      }
      data.residual = sum * alpha2 * (1.0-alpha2);
      b.push(std::make_pair(src, pri(graph, src)));
    }, Galois::do_all_steal(true));
}

PRTy atomicAdd(std::atomic<PRTy>& v, PRTy delta) {
  PRTy old;
  do {
    old = v;
  } while (!v.compare_exchange_strong(old, old + delta));
  return old;
}

template<typename Graph>
void verifyInOut(Graph& graph, PRTy tolerance) {
  for(auto N : graph) {
    auto& data = graph.getData(N);
    auto residual = std::fabs(data.getPageRank() - computePageRankInOut(graph, N, 1, Galois::MethodFlag::NONE));
    if (residual > tolerance) {
      //std::cout << N << " residual " << residual << " pr " << data.getPageRank() << " data " << data << "\n";
    }
  }
}


template<typename UpdateRequest>
struct UpdateRequestComparer: public std::binary_function<const UpdateRequest&, const UpdateRequest&, unsigned> {
  unsigned operator()(const UpdateRequest& x, const UpdateRequest& y) const {
    return x.second > y.second;
  }
};

template<typename UpdateRequest>
struct UpdateRequestNodeComparer: public std::binary_function<const UpdateRequest&, const UpdateRequest&, unsigned> {
  unsigned operator()(const UpdateRequest& x, const UpdateRequest& y) const {
    if (x.second > y.second) return true;
    if (x.second < y.second) return false;
    return x.first>y.first;
  }
};

template<typename UpdateRequest>
struct UpdateRequestHasher: public std::unary_function<UpdateRequest, unsigned long> {
  unsigned long operator() (const UpdateRequest& val) const {
    return (unsigned long) val.first;
  }
};

/*
  Async priority based implementation
*/
