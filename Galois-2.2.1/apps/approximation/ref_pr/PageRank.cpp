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
 * @author Donald Nguyen <ddn@cs.utexas.edu>
 */
#include "Galois/config.h"
#include "Galois/Galois.h"
#include "Galois/Accumulator.h"
#include "Galois/Bag.h"
#include "Galois/Statistic.h"
#include "Galois/Graph/LCGraph.h"
#include "Galois/Graph/TypeTraits.h"
#include "Lonestar/BoilerPlate.h"

#include GALOIS_CXX11_STD_HEADER(atomic)
#include <string>
#include <sstream>
#include <limits>
#include <iostream>
#include <fstream>

#include "PageRank.h"
#include "PageRankAsyncPri.h"
#ifdef GALOIS_USE_EXP
#include "GraphLabAlgo.h"
#include "LigraAlgo.h"
#endif

namespace cll = llvm::cl;

static const char* name = "Page Rank";
static const char* desc = "Computes page ranks a la Page and Brin";
static const char* url = 0;

enum Algo {
  graphlab,
  graphlabAsync,
  ligra,
  ligraChi,
  pull,
  async_prt,
  serial
};
bool outOnly;
std::string worklistname_;
// new statistics added
static Galois::Statistic* BadWork;
static Galois::Statistic* WLEmptyWork;
static Galois::Statistic* nBad;
static Galois::Statistic* nEmpty;
static Galois::Statistic* nOverall;
static Galois::Statistic* nEdgesProcessed;


cll::opt<std::string> filename(cll::Positional, cll::desc("<input graph>"), cll::Required);
static cll::opt<std::string> transposeGraphName("graphTranspose", cll::desc("Transpose of input graph"));
static cll::opt<bool> symmetricGraph("symmetricGraph", cll::desc("Input graph is symmetric"));
static cll::opt<std::string> outputPullFilename("outputPull", cll::desc("Precompute data for Pull algorithm to file"));
cll::opt<unsigned int> maxIterations("maxIterations", cll::desc("Maximum iterations"), cll::init(100));
cll::opt<unsigned int> memoryLimit("memoryLimit", cll::desc("Memory limit for out-of-core algorithms (in MB)"), cll::init(~0U));
cll::opt<bool> skipPrintPageRanks("skipPrintPageRanks", cll::desc("If false, output a reference file, if true, output only stats"), cll::init(false));


//  new options for asynchronous execution

static cll::opt<float> amp("amp", cll::desc("amp for priority"), cll::init(100));
//tolerance already defined with default value of 0.01 -- don't change it for now
static cll::opt<float> tolerance("tolerance", cll::desc("tolerance"), cll::init(0.01));
static cll::opt<bool> outOnlyP("outdeg", cll::desc("Out degree only for priority"), cll::init(false));
static cll::opt<std::string> worklistname("wl", cll::desc("Worklist to use"), cll::value_desc("worklist"), cll::init("obim"));

//end of new options

static cll::opt<Algo> algo("algo", cll::desc("Choose an algorithm:"),
    cll::values(
      clEnumValN(Algo::pull, "pull", "Use precomputed data perform pull-based algorithm"),
      clEnumValN(Algo::async_prt, "async_prt", "Prioritized (degree biased residual) version..."),
      clEnumValN(Algo::serial, "serial", "Compute PageRank in serial"),
#ifdef GALOIS_USE_EXP
      clEnumValN(Algo::graphlab, "graphlab", "Use GraphLab programming model"),
      clEnumValN(Algo::graphlabAsync, "graphlabAsync", "Use GraphLab-Asynchronous programming model"),
      clEnumValN(Algo::ligra, "ligra", "Use Ligra programming model"),
      clEnumValN(Algo::ligraChi, "ligraChi", "Use Ligra and GraphChi programming model"),
#endif
      clEnumValEnd), cll::init(Algo::pull));

struct SerialAlgo {
  typedef Galois::Graph::LC_CSR_Graph<PNode,void>
    ::with_no_lockable<true>::type Graph;
  typedef Graph::GraphNode GNode;

  std::string name() const { return "Serial"; }

  void readGraph(Graph& graph) { Galois::Graph::readGraph(graph, filename); }

  struct Initialize {
    Graph& g;
    Initialize(Graph& g): g(g) { }
    void operator()(Graph::GraphNode n) {
      g.getData(n).value = 1.0-alpha;
      g.getData(n).accum.write(0.0);
    }
  };

  void operator()(Graph& graph) {
    unsigned int iteration = 0;
    unsigned int numNodes = graph.size();

    while (true) {
      float max_delta = std::numeric_limits<float>::min();
      unsigned int small_delta = 0;

      for (auto ii = graph.begin(), ei = graph.end(); ii != ei; ++ii) {
        GNode src = *ii;
        PNode& sdata = graph.getData(src);
        int neighbors = std::distance(graph.edge_begin(src), graph.edge_end(src));
        for (auto jj = graph.edge_begin(src), ej = graph.edge_end(src); jj != ej; ++jj) {
          GNode dst = graph.getEdgeDst(jj);
          PNode& ddata = graph.getData(dst);
          float delta =  sdata.value / neighbors;
          ddata.accum.write(ddata.accum.read() + delta);
        }
      }

      for (auto ii = graph.begin(), ei = graph.end(); ii != ei; ++ii) {
        GNode src = *ii;
        PNode& sdata = graph.getData(src, Galois::MethodFlag::NONE);
        float value = (1.0 - alpha) * sdata.accum.read() + alpha;
        float diff = std::fabs(value - sdata.value);
        if (diff <= tolerance)
          ++small_delta;
        if (diff > max_delta)
          max_delta = diff;
        sdata.value = value;
        sdata.accum.write(0);
      }

      iteration += 1;

      std::cout << "iteration: " << iteration
                << " max delta: " << max_delta
                << " small delta: " << small_delta
                << " (" << small_delta / (float) numNodes << ")"
                << "\n";

      if (max_delta <= tolerance || iteration >= maxIterations)
        break;
    }

    if (iteration >= maxIterations) {
      std::cout << "Failed to converge\n";
    }
  }
};

struct PullAlgo {
  struct LNode {
    float value[2];

    float getPageRank() { return value[1]; }
    float getPageRank(unsigned int it) { return value[it & 1]; }
    void setPageRank(unsigned it, float v) { value[(it+1) & 1] = v; }
  };
  typedef Galois::Graph::LC_InlineEdge_Graph<LNode,float>
    ::with_compressed_node_ptr<true>::type
    ::with_no_lockable<true>::type
    ::with_numa_alloc<true>::type
    Graph;
  typedef Graph::GraphNode GNode;

  std::string name() const { return "Pull"; }

  Galois::GReduceMax<double> max_delta;
  Galois::GAccumulator<unsigned int> small_delta;

  void readGraph(Graph& graph) {
    if (transposeGraphName.size()) {
      Galois::Graph::readGraph(graph, transposeGraphName);
    } else {
      std::cerr << "Need to pass precomputed graph through -graphTranspose option\n";
      abort();
    }
  }

  struct Initialize {
    Graph& g;
    Initialize(Graph& g): g(g) { }
    void operator()(Graph::GraphNode n) {
      LNode& data = g.getData(n, Galois::MethodFlag::NONE);
      data.value[0] = 1.0;
      data.value[1] = 1.0;
    }
  };

  struct Copy {
    Graph& g;
    Copy(Graph& g): g(g) { }
    void operator()(Graph::GraphNode n) {
      LNode& data = g.getData(n, Galois::MethodFlag::NONE);
      data.value[1] = data.value[0];
    }
  };


  struct Process {
    PullAlgo* self;
    Graph& graph;
    unsigned int iteration;

    Process(PullAlgo* s, Graph& g, unsigned int i): self(s), graph(g), iteration(i) { }

    void operator()(const GNode& src, Galois::UserContext<GNode>& ctx) {
      (*this)(src);
    }

    void operator()(const GNode& src) {
      LNode& sdata = graph.getData(src, Galois::MethodFlag::NONE);
      double sum = 0;

      for (auto jj = graph.edge_begin(src, Galois::MethodFlag::NONE), ej = graph.edge_end(src, Galois::MethodFlag::NONE); jj != ej; ++jj) {
        GNode dst = graph.getEdgeDst(jj);
        float w = graph.getEdgeData(jj);

        LNode& ddata = graph.getData(dst, Galois::MethodFlag::NONE);
        sum += ddata.getPageRank(iteration) * w;
      }

      float value = sum * (1.0 - alpha) + alpha;
      float diff = std::fabs(value - sdata.getPageRank(iteration));

      if (diff <= tolerance)
        self->small_delta += 1;
      self->max_delta.update(diff);
      sdata.setPageRank(iteration, value);
    }
  };

  void operator()(Graph& graph) {
    unsigned int iteration = 0;

    while (true) {
      Galois::for_each_local(graph, Process(this, graph, iteration));
      iteration += 1;

      float delta = max_delta.reduce();
      size_t sdelta = small_delta.reduce();

      std::cout << "iteration: " << iteration
                << " max delta: " << delta
                << " small delta: " << sdelta
                << " (" << sdelta / (float) graph.size() << ")"
                << "\n";

      if (delta <= tolerance || iteration >= maxIterations)
        break;
      max_delta.reset();
      small_delta.reset();
    }

    if (iteration >= maxIterations) {
      std::cout << "Failed to converge\n";
    }

    if (iteration & 1) {
      // Result already in right place
    } else {
      Galois::do_all_local(graph, Copy(graph));
    }
  }
};

//! Transpose in-edges to out-edges
static void precomputePullData() {
  typedef Galois::Graph::LC_CSR_Graph<size_t, void>
    ::with_no_lockable<true>::type InputGraph;
  typedef InputGraph::GraphNode InputNode;
  typedef Galois::Graph::FileGraphWriter OutputGraph;
  //typedef OutputGraph::GraphNode OutputNode;

  InputGraph input;
  OutputGraph output;
  Galois::Graph::readGraph(input, filename);

  size_t node_id = 0;
  for (auto ii = input.begin(), ei = input.end(); ii != ei; ++ii) {
    InputNode src = *ii;
    input.getData(src) = node_id++;
  }

  output.setNumNodes(input.size());
  output.setNumEdges(input.sizeEdges());
  output.setSizeofEdgeData(sizeof(float));
  output.phase1();

  for (auto ii = input.begin(), ei = input.end(); ii != ei; ++ii) {
    InputNode src = *ii;
    size_t sid = input.getData(src);
    assert(sid < input.size());

    //size_t num_neighbors = std::distance(input.edge_begin(src), input.edge_end(src));

    for (auto jj = input.edge_begin(src), ej = input.edge_end(src); jj != ej; ++jj) {
      InputNode dst = input.getEdgeDst(jj);
      size_t did = input.getData(dst);
      assert(did < input.size());

      output.incrementDegree(did);
    }
  }

  output.phase2();
  std::vector<float> edgeData;
  edgeData.resize(input.sizeEdges());

  for (auto ii = input.begin(), ei = input.end(); ii != ei; ++ii) {
    InputNode src = *ii;
    size_t sid = input.getData(src);
    assert(sid < input.size());

    size_t num_neighbors = std::distance(input.edge_begin(src), input.edge_end(src));

    float w = 1.0/num_neighbors;
    for (auto jj = input.edge_begin(src), ej = input.edge_end(src); jj != ej; ++jj) {
      InputNode dst = input.getEdgeDst(jj);
      size_t did = input.getData(dst);
      assert(did < input.size());

      size_t idx = output.addNeighbor(did, sid);
      edgeData[idx] = w;
    }
  }

  float* t = output.finish<float>();
  memcpy(t, &edgeData[0], sizeof(edgeData[0]) * edgeData.size());

  output.structureToFile(outputPullFilename);
  std::cout << "Wrote " << outputPullFilename << "\n";
}

//! Make values unique
template<typename GNode>
struct TopPair {
  float value;
  GNode id;

  TopPair(float v, GNode i): value(v), id(i) { }

  bool operator<(const TopPair& b) const {
    if (value == b.value)
      return id > b.id;
    return value < b.value;
  }
};

template<typename Graph>
static void printTop(Graph& graph, int topn) {
  typedef typename Graph::GraphNode GNode;
  typedef typename Graph::node_data_reference node_data_reference;
  typedef TopPair<GNode> Pair;
  typedef std::map<Pair,GNode> Top;

  Top top;

  for (auto ii = graph.begin(), ei = graph.end(); ii != ei; ++ii) {
    GNode src = *ii;
    node_data_reference n = graph.getData(src);
    float value = n.getPageRank();
    Pair key(value, src);

    if ((int) top.size() < topn) {
      top.insert(std::make_pair(key, src));
      continue;
    }

    if (top.begin()->first < key) {
      top.erase(top.begin());
      top.insert(std::make_pair(key, src));
    }
  }

  int rank = 1;
  std::cout << "Rank PageRank Id\n";
  for (typename Top::reverse_iterator ii = top.rbegin(), ei = top.rend(); ii != ei; ++ii, ++rank) {
    std::cout << rank << ": " << ii->first.value << " " << graph.getId(ii->first.id) << "\n";
  }
}

struct AsyncPri{
  struct LNode {
    PRTy value;
    std::atomic<PRTy> residual;
    void init() { value = 1.0 -alpha2 ; residual = 0.0; }
    PRTy getPageRank(int x = 0) { return value; }
    friend std::ostream& operator<<(std::ostream& os, const LNode& n) {
      // os << "{PR " << n.value << ", residual " << n.residual << "}";
      return os;
    }
  };

  typedef Galois::Graph::LC_CSR_Graph<LNode,void>::with_numa_alloc<true>::type InnerGraph;
  typedef Galois::Graph::LC_InOut_Graph<InnerGraph> Graph;
  typedef Graph::GraphNode GNode;

  std::string name() const { return "AsyncPri"; }

  void readGraph(Graph& graph, std::string filename, std::string transposeGraphName) {
    if (transposeGraphName.size()) {
      Galois::Graph::readGraph(graph, filename, transposeGraphName);
    } else {
      std::cerr << "Need to pass precomputed graph through -graphTranspose option\n";
      abort();
    }
  }

  struct PRPri {
    Graph& graph;
    PRTy tolerance;
    PRPri(Graph& g, PRTy t) : graph(g), tolerance(t) {}
    int operator()(const GNode& src, PRTy d) const {
      if (outOnly)
        d /= (1 + nout(graph, src, Galois::MethodFlag::NONE));
      else
        d /= ninout(graph, src, Galois::MethodFlag::NONE);
      d /= tolerance;
      //older one inf
      //if (d > 100)
      //  return -100;
      //return -d; //d*amp; //std::max((int)floor(d*amp), 0);
      //std::cout<<"amp: "<<amp<<" pri: "<<(-1*d*amp)<<std::endl;
      return -1*d*amp;
    }
    int operator()(const GNode& src) const {
      PRTy d = graph.getData(src, Galois::MethodFlag::NONE).residual;
      return operator()(src, d);
    }
  };

  struct sndPri {
    int operator()(const std::pair<GNode, int>& n) const {
      return n.second;
    }
  };


  struct fpPRPri {
    Graph& graph;
    PRTy tolerance;
    fpPRPri(Graph& g, PRTy t) : graph(g), tolerance(t) {}
    PRTy operator()(const GNode& src, PRTy d) const {
      if (outOnly)
        d /= (1 + nout(graph, src, Galois::MethodFlag::NONE));
      else
        d /= ninout(graph, src, Galois::MethodFlag::NONE);
      d /= tolerance;
      //older one inf
      //if (d > 100)
      //  return -100;
      //return -d; //d*amp; //std::max((int)floor(d*amp), 0);
      //std::cout<<"amp: "<<amp<<std::endl;
      return -1*d;
    }
    PRTy operator()(const GNode& src) const {
      PRTy d = graph.getData(src, Galois::MethodFlag::NONE).residual;
      return operator()(src, d);
    }
  };

  struct fpsndPri {
    PRTy operator()(const std::pair<GNode, PRTy>& n) const {
      return n.second;
    }
  };

  struct ProcessWithBreaks {
    typedef int tt_needs_parallel_break;

    Graph& graph;
    PRTy tolerance;
    fpPRPri pri;

    ProcessWithBreaks(Graph& g, PRTy t, PRTy a): graph(g), tolerance(t), pri(g,t) { }

    void operator()(const std::pair<GNode,PRTy>& srcn, Galois::UserContext<std::pair<GNode,PRTy>>& ctx) const {
      //ctx.t.stopwatch();
      GNode src = srcn.first;
      LNode& sdata = graph.getData(src);

      (*nOverall)+=1;
      if(sdata.residual < tolerance ||pri(src) < srcn.second){ // (pri(src) < srcn.second ||  (removed)
        *nEmpty += 1;
        *WLEmptyWork += ctx.t.stopwatch();
        return;
      }
      (*nEdgesProcessed)+=ninout(graph, src, Galois::MethodFlag::NONE);;
      Galois::MethodFlag lockflag = Galois::MethodFlag::NONE;

      PRTy oldResidual = sdata.residual.exchange(0.0);
      PRTy pr = computePageRankInOut(graph, src, 0, lockflag);
      PRTy diff = std::fabs(pr - sdata.value);
      sdata.value = pr;
      int src_nout = nout(graph,src, lockflag);
      PRTy delta = diff*alpha2/src_nout;



      // for each out-going neighbors
      for (auto jj = graph.edge_begin(src, lockflag), ej = graph.edge_end(src, lockflag); jj != ej; ++jj) {
        GNode dst = graph.getEdgeDst(jj);
        LNode& ddata = graph.getData(dst, lockflag);
        PRTy old = atomicAdd(ddata.residual, delta);
        // if the node is not in the worklist and the residual is greater than tolerance
        if(old + delta >= tolerance && (old <= tolerance || (pri(dst, old) > pri(dst,old+delta))))// || pri(dst, old) > pri(dst,old+delta))))
        {
          //std::cerr << "pri:"<<(pri(dst, old))<<" "<<(pri(dst, old+delta)) << "\n";
          ctx.push(std::make_pair(dst, pri(dst, old+delta)));

        }
      }
    }
  };

  struct Process {
    Graph& graph;
    PRTy tolerance;
    PRPri pri;

    Process(Graph& g, PRTy t, PRTy a): graph(g), tolerance(t), pri(g,t) { }

    void operator()(const std::pair<GNode,int>& srcn, Galois::UserContext<std::pair<GNode,int>>& ctx) const {
      //ctx.t.stopwatch();
      GNode src = srcn.first;
      LNode& sdata = graph.getData(src);

      (*nOverall)+=1;
      if(sdata.residual < tolerance || pri(src) != srcn.second){
        *nEmpty += 1;
        *WLEmptyWork += ctx.t.stopwatch();
        return;
      }
      (*nEdgesProcessed)+=ninout(graph, src, Galois::MethodFlag::NONE);;
      Galois::MethodFlag lockflag = Galois::MethodFlag::NONE;

      PRTy oldResidual = sdata.residual.exchange(0.0);
      PRTy pr = computePageRankInOut(graph, src, 0, lockflag);
      PRTy diff = std::fabs(pr - sdata.value);
      sdata.value = pr;
      int src_nout = nout(graph,src, lockflag);
      PRTy delta = diff*alpha2/src_nout;



      // for each out-going neighbors
      for (auto jj = graph.edge_begin(src, lockflag), ej = graph.edge_end(src, lockflag); jj != ej; ++jj) {
        GNode dst = graph.getEdgeDst(jj);
        LNode& ddata = graph.getData(dst, lockflag);
        PRTy old = atomicAdd(ddata.residual, delta);
        // if the node is not in the worklist and the residual is greater than tolerance
        if(old + delta >= tolerance && (old <= tolerance || pri(dst, old) != pri(dst,old+delta))) {
          //std::cerr << pri(dst, old+delta) << " ";
          ctx.push(std::make_pair(dst, pri(dst, old+delta)));

        }
      }
    }
  };

  void cleanup(){

  }
  void operator()(Graph& graph, PRTy tolerance, PRTy amp) {
    std::cout<<"Here\n";
    std::cout<<"AMP:"<<amp<<" \n";


    initResidual(graph);
    typedef Galois::WorkList::dChunkedFIFO<32> WL;
    typedef Galois::WorkList::OrderedByIntegerMetric<sndPri,WL>::with_block_period<8>::type OBIM;
    Galois::InsertBag<std::pair<GNode, int> > bag;

    using namespace Galois::WorkList;
    typedef dChunkedFIFO<32> Chunk;
    typedef dVisChunkedFIFO<32> visChunk;
    typedef dChunkedPTFIFO<1> noChunk;
    typedef ChunkedFIFO<32> globChunk;
    typedef ChunkedFIFO<1> globNoChunk;
    //typedef OrderedByIntegerMetric<sndPri, Chunk, 10> OBIM;
    typedef OrderedByIntegerMetric<sndPri, dChunkedLIFO<32>, 10> OBIM_LIFO;
    typedef OrderedByIntegerMetric<sndPri, Chunk, 4> OBIM_BLK4;
    typedef OrderedByIntegerMetric<sndPri, Chunk, 10, false> OBIM_NOBSP;
    typedef OrderedByIntegerMetric<sndPri, noChunk, 10> OBIM_NOCHUNK;
    typedef OrderedByIntegerMetric<sndPri, globChunk, 10> OBIM_GLOB;
    typedef OrderedByIntegerMetric<sndPri, globNoChunk, 10> OBIM_GLOB_NOCHUNK;
    typedef OrderedByIntegerMetric<sndPri, noChunk, -1, false> OBIM_STRICT;
    typedef OrderedByIntegerMetric<sndPri, Chunk, 10, true, true> OBIM_UBSP;
    typedef OrderedByIntegerMetric<sndPri, visChunk, 10, true, true> OBIM_VISCHUNK;
    typedef UpdateRequestComparer<std::pair<GNode, int>> Comparer;
    typedef UpdateRequestNodeComparer<std::pair<GNode, int>> NodeComparer;
    typedef UpdateRequestHasher<std::pair<GNode, int>> Hasher;
  //  typedef GlobPQ<std::pair<GNode, int>, LockFreeSkipList<Comparer, std::pair<GNode, int>>> GPQ;
  //  typedef GlobPQ<std::pair<GNode, int>, LockFreeSkipList<NodeComparer, std::pair<GNode, int>>> GPQ_NC;
  //  typedef GlobPQ<std::pair<GNode, int>, SprayList<NodeComparer, std::pair<GNode, int>>> SL;
  //  typedef GlobPQ<std::pair<GNode, int>, MultiQueue<Comparer, std::pair<GNode, int>, 1>> MQ1;
  //  typedef GlobPQ<std::pair<GNode, int>, MultiQueue<Comparer, std::pair<GNode, int>, 4>> MQ4;
  //  typedef GlobPQ<std::pair<GNode, int>, MultiQueue<NodeComparer, std::pair<GNode, int>, 4>> MQ4_NC;
  //  typedef GlobPQ<std::pair<GNode, int>, HeapMultiQueue<Comparer, std::pair<GNode, int>, 1>> HMQ1;
  //  typedef GlobPQ<std::pair<GNode, int>, HeapMultiQueue<Comparer, std::pair<GNode, int>, 4>> HMQ4;
  //  typedef GlobPQ<std::pair<GNode, int>, DistQueue<Comparer, std::pair<GNode, int>, false>> PTSL;
  //  typedef GlobPQ<std::pair<GNode, int>, DistQueue<Comparer, std::pair<GNode, int>, true>> PPSL;
  //  typedef GlobPQ<std::pair<GNode, int>, LocalPQ<Comparer, std::pair<GNode, int>>> LPQ;
  //  typedef GlobPQ<std::pair<GNode, int>, SwarmPQ<Comparer, std::pair<GNode, int>>> SWARMPQ;
  //  typedef GlobPQ<std::pair<GNode, int>, SwarmPQ<NodeComparer, std::pair<GNode, int>>> SWARMPQ_NC;
  //  typedef GlobPQ<std::pair<GNode, int>, HeapSwarmPQ<Comparer, std::pair<GNode, int>>> HSWARMPQ;
  //  typedef GlobPQ<std::pair<GNode, int>, PartitionPQ<Comparer, Hasher, std::pair<GNode, int>>> PPQ;
    typedef SkipListOrderedByIntegerMetric<sndPri, Chunk, 10> SLOBIM;
    typedef SkipListOrderedByIntegerMetric<sndPri, noChunk, 10> SLOBIM_NOCHUNK;
    typedef SkipListOrderedByIntegerMetric<sndPri, visChunk, 10> SLOBIM_VISCHUNK;
    typedef VectorOrderedByIntegerMetric<sndPri, Chunk, 10> VECOBIM;
    typedef VectorOrderedByIntegerMetric<sndPri, noChunk, 10> VECOBIM_NOCHUNK;
    typedef VectorOrderedByIntegerMetric<sndPri, globNoChunk, 10> VECOBIM_GLOB_NOCHUNK;



    PRPri pri(graph, tolerance);
    fpPRPri fppri(graph, tolerance);
    // Galois::do_all_local(graph, [&graph, &bag, &pri] (const GNode& node) {
    //     bag.push(std::make_pair(node, pri(node)));
    //   });
    // Galois::for_each_local(bag, Process(graph, tolerance, amp), Galois::wl<OBIM>());

    auto fn = [&pri] (const GNode& node) { return std::make_pair(node, pri(node)); };
    auto fn2 = [&fppri] (const GNode& node) { return std::make_pair(node, fppri(node)); };
    std::string wl = worklistname_;
    if (wl == "obim")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM>());
    else if (wl == "slobim")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<SLOBIM>());
    else if (wl == "slobim-nochunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<SLOBIM_NOCHUNK>());
    else if (wl == "slobim-vischunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<SLOBIM_VISCHUNK>());
    else if (wl == "vecobim")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<VECOBIM>());
    else if (wl == "vecobim-nochunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<VECOBIM_NOCHUNK>());
    else if (wl == "vecobim-glob-nochunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<VECOBIM_GLOB_NOCHUNK>());
    else if (wl == "obim-strict")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_STRICT>());
    else if (wl == "obim-ubsp")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_UBSP>());
    else if (wl == "obim-lifo")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_LIFO>());
    else if (wl == "obim-blk4")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_BLK4>());
    else if (wl == "obim-nobsp")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_NOBSP>());
    else if (wl == "obim-nochunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_NOCHUNK>());
    else if (wl == "obim-vischunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_VISCHUNK>());
    else if (wl == "obim-glob")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_GLOB>());
    else if (wl == "obim-glob-nochunk")
      Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<OBIM_GLOB_NOCHUNK>());
    //else if (wl == "skiplist")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<GPQ>());
    //else if (wl == "skiplist-nc")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<GPQ_NC>());
    //else if (wl == "spraylist")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<SL>());
    //else if (wl == "multiqueue1")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<MQ1>());
    //else if (wl == "multiqueue4")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<MQ4>());
    //else if (wl == "multiqueue4-nc")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<MQ4_NC>());
    //else if (wl == "heapmultiqueue1")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<HMQ1>());
    //else if (wl == "heapmultiqueue4")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<HMQ4>());
    //else if (wl == "thrskiplist")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<PTSL>());
    //else if (wl == "pkgskiplist")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<PPSL>());
    //else if (wl == "lpq")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<LPQ>());
    //else if (wl == "swarm")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<SWARMPQ>());
    //else if (wl == "swarm-nc")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn)),
    //                 Process(graph, tolerance, amp), Galois::wl<SWARMPQ_NC>());
    //else if (wl == "heapswarm")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<HSWARMPQ>());
    //else if (wl == "ppq")
    //  Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn2)),
    //                 boost::make_transform_iterator(graph.end(), std::ref(fn2)),
    //                 ProcessWithBreaks(graph, tolerance, amp), Galois::wl<PPQ>());
    else
      std::cerr << "No work list!" << "\n";
    /*Galois::for_each(boost::make_transform_iterator(graph.begin(), std::ref(fn)),
                     boost::make_transform_iterator(graph.end(), std::ref(fn)),
                     Process(graph, tolerance, amp), Galois::wl<SL>());*/
    std::cout<< "here2\n";

  }

  void verify(Graph& graph, PRTy tolerance) {
    verifyInOut(graph, tolerance);
  }
};

// //! Make values unique
// template<typename GNode>
// struct TopPair {
//   float value;
//   GNode id;
//
//   TopPair(float v, GNode i): value(v), id(i) { }
//
//   bool operator<(const TopPair& b) const {
//     if (value == b.value)
//       return id > b.id;
//     return value < b.value;
//   }
// };



template<typename Graph>
static void printTop(Graph& graph, int topn, const char *algo_name, int numThreads) {
  typedef typename Graph::GraphNode GNode;
  typedef typename Graph::node_data_reference node_data_reference;
  typedef TopPair<GNode> Pair;
  typedef std::map<Pair,GNode> Top;

  // normalize the PageRank value so that the sum is equal to one
  float sum=0;
  for (auto ii = graph.begin(), ei = graph.end(); ii != ei; ++ii) {
    GNode src = *ii;
    node_data_reference n = graph.getData(src);
    float value = n.getPageRank(0);
    sum += value;
  }

  Top top;

  std::ofstream myfile;
  // if(dbg){
  //   char filename[256];
  //   int tamp = amp;
  //   float ttol = tolerance;
  //   sprintf(filename,"/scratch/01982/joyce/tmp/%s_t_%d_tol_%f_amp_%d", algo_name,numThreads,ttol,tamp);
  //   myfile.open (filename);
  // }

  //std::cout<<"print PageRank\n";
  for (auto ii = graph.begin(), ei = graph.end(); ii != ei; ++ii) {
    GNode src = *ii;
    node_data_reference n = graph.getData(src);
    float value = n.getPageRank(0)/sum; // normalized PR (divide PR by sum)
    //float value = n.getPageRank(); // raw PR
    //std::cout<<value<<" ";
    // if(dbg){
    //   myfile << value <<" ";
    // }
    Pair key(value, src);

    if ((int) top.size() < topn) {
      top.insert(std::make_pair(key, src));
      continue;
    }

    if (top.begin()->first < key) {
      top.erase(top.begin());
      top.insert(std::make_pair(key, src));
    }
  }
  // if(dbg){
  //   myfile.close();
  // }
  //std::cout<<"\nend of print\n";

  int rank = 1;
  std::cout << "Rank PageRank Id\n";
  for (typename Top::reverse_iterator ii = top.rbegin(), ei = top.rend(); ii != ei; ++ii, ++rank) {
    std::cout << rank << ": " << ii->first.value << " " << graph.getId(ii->first.id) << "\n";
  }

  if (!skipPrintPageRanks) { 
    std::ofstream fd;
    fd.open(filename+".ref");
    if (!fd.is_open())
    {
      std::cout << "Can't open " << filename+".ref\n";
    }
    for(auto N : graph) {
      auto& data = graph.getData(N);
      //auto residual = std::fabs(data.getPageRank() - computePageRankInOut(graph, N, 1, Galois::MethodFlag::NONE));
      fd << N << "\t" << data.getPageRank() << "\n";
    }   
  }
  
}

template<typename Algo>
void runAsync() {

  typedef typename Algo::Graph Graph;

  Algo algo;
  Graph graph;

  algo.readGraph(graph, filename, transposeGraphName);

  Galois::preAlloc(numThreads + 3 * (graph.size() * 64) / Galois::Runtime::MM::pageSize);
  Galois::reportPageAlloc("MeminfoPre");

  Galois::StatTimer T;
  auto eamp = -amp;///tolerance;
  std::cout << "Running " << algo.name() << " version\n";
  std::cout << "tolerance: " << tolerance << "\n";
  std::cout << "effective amp: " << eamp << "\n";
  T.start();
 // clock_t start = clock();
  //omp_get_wtime();
  time_t start,end;
	time (&start);
  Galois::do_all_local(graph, [&graph] (typename Graph::GraphNode n) { graph.getData(n).init(); });
  std::cout<<"Initialization is done\n";
  algo(graph, tolerance, eamp);
  time (&end);
	double dif = difftime (end,start);
	printf ("Elapsed time is %.2lf seconds.\n", dif );
 // clock_t end = clock();
  //omp_get_wtime();
  //std::cout<<"Time: "<<(double(end - start) / CLOCKS_PER_SEC)<<std::endl;
  T.stop();

  Galois::reportPageAlloc("MeminfoPost");

  Galois::Runtime::reportNumaAlloc("NumaPost");
  algo.cleanup();
  if (!skipVerify) {
    algo.verify(graph, tolerance);
    printTop(graph, 10, algo.name().c_str(), numThreads);
  }

}
template<typename Algo>
void run() {
  typedef typename Algo::Graph Graph;

  Algo algo;
  Graph graph;


  algo.readGraph(graph);

  Galois::preAlloc(numThreads + 3 * (graph.size() * 64) / Galois::Runtime::MM::pageSize);
  Galois::reportPageAlloc("MeminfoPre");

  Galois::StatTimer T;
  std::cout << "Running " << algo.name() << " version\n";
  std::cout << "Target max delta: " << tolerance << "\n";
  T.start();
  Galois::do_all_local(graph, typename Algo::Initialize(graph));
  algo(graph);
  T.stop();

  Galois::reportPageAlloc("MeminfoPost");

  if (!skipVerify)
    printTop(graph, 10);
}

int main(int argc, char **argv) {
  LonestarStart(argc, argv, name, desc, url);
  Galois::StatManager statManager;

  //outOnly p
  outOnly = outOnlyP;
  worklistname_ = worklistname;
  //new statistics

  BadWork = new Galois::Statistic("BadWork");
  WLEmptyWork = new Galois::Statistic("EmptyWork");
  nBad = new Galois::Statistic("nBad");
  nEmpty = new Galois::Statistic("nEmpty");
  nOverall = new Galois::Statistic("nOverall");
  nEdgesProcessed = new Galois::Statistic("nEdgesProcessed");


  if (outputPullFilename.size()) {
    precomputePullData();
    return 0;
  }

  Galois::StatTimer T("TotalTime");
  T.start();
  switch (algo) {
    case Algo::pull: run<PullAlgo>(); break;
    //new algo priority based
    case Algo::async_prt: runAsync<AsyncPri>(); break;
#ifdef GALOIS_USE_EXP
    case Algo::ligra: run<LigraAlgo<false> >(); break;
    case Algo::ligraChi: run<LigraAlgo<true> >(); break;
    case Algo::graphlab: run<GraphLabAlgo<false,false> >(); break;
    case Algo::graphlabAsync: run<GraphLabAlgo<true,true> >(); break;
#endif
    case Algo::serial: run<SerialAlgo>(); break;
    default: std::cerr << "Unknown algorithm\n"; abort();
  }
  T.stop();

  delete BadWork;
  delete WLEmptyWork;
  delete nBad;
  delete nEmpty;
  delete nOverall;
  delete nEdgesProcessed;

  return 0;
}
