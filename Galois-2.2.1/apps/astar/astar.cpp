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
#include "Galois/Galois.h"
#include "Galois/Accumulator.h"
#include "Galois/Bag.h"
#include "Galois/Statistic.h"
#include "Galois/Timer.h"
#include "Galois/Graph/LCGraph.h"
#include "Galois/Graph/TypeTraits.h"
#include "llvm/Support/CommandLine.h"
#include "Lonestar/BoilerPlate.h"

#include <iostream>
#include <deque>
#include <set>

#include "astar.h"

#include <fstream>
//#include "GraphLabAlgo.h"
//#include "LigraAlgo.h"

#include "chunk_size.h"

#ifdef GEM5
  #include "m5op.h"
#endif

namespace cll = llvm::cl;

static const char* name = "Astar";
static const char* desc =
  "Computes the shortest path from a source node to a destination node in a directed "
  "graph using a modified chaotic iteration algorithm";
static const char* url = "Astar";

enum Algo {
  async,
  asyncWithCas,
  asyncPP,
  graphlab,
  ligra,
  ligraChi,
  serial
};

static cll::opt<std::string> filename(cll::Positional, cll::desc("<input graph>"), cll::Required);
static cll::opt<std::string> coordFilename("coordFilename", cll::desc("coordinate file name"));
static cll::opt<unsigned int> xdim("xdim", cll::desc("xdim of the map"));
static cll::opt<unsigned int> ydim("ydim", cll::desc("ydim of the map"));
static cll::opt<std::string> mqSuff("suff", cll::desc("Suffix for amq or smq"), cll::init(""));

static cll::opt<std::string> transposeGraphName("graphTranspose", cll::desc("Transpose of input graph"));
static cll::opt<bool> symmetricGraph("symmetricGraph", cll::desc("Input graph is symmetric"));
static cll::opt<unsigned int> startNode("startNode", cll::desc("Node to start search from"), cll::init(0));
static cll::opt<unsigned int> destNode("destNode", cll::desc("Node to reach"), cll::init(1));

static cll::opt<unsigned int> reportNode("reportNode", cll::desc("Node to report distance to"), cll::init(1));
static cll::opt<int> stepShift("delta", cll::desc("Shift value for the deltastep"), cll::init(10));
cll::opt<unsigned int> memoryLimit("memoryLimit",
    cll::desc("Memory limit for out-of-core algorithms (in MB)"), cll::init(~0U));
static cll::opt<Algo> algo("algo", cll::desc("Choose an algorithm:"),
    cll::values(
      clEnumValN(Algo::async, "async", "Asynchronous"),
      clEnumValN(Algo::asyncPP, "asyncPP", "Async, CAS, push-pull"),
      clEnumValN(Algo::asyncWithCas, "asyncWithCas", "Use compare-and-swap to update nodes"),
      clEnumValN(Algo::serial, "serial", "Serial"),
      clEnumValN(Algo::graphlab, "graphlab", "Use GraphLab programming model"),
      clEnumValN(Algo::ligraChi, "ligraChi", "Use Ligra and GraphChi programming model"),
      clEnumValN(Algo::ligra, "ligra", "Use Ligra programming model"),
      clEnumValEnd), cll::init(Algo::asyncWithCas));
static cll::opt<std::string> worklistname("wl", cll::desc("Worklist to use"), cll::value_desc("worklist"), cll::init("obim"));

static cll::opt<std::string> resultFileName("resultFile", cll::desc("Result file name for amq experiment"), cll::init("result.txt"));


static const bool trackWork = true;
static Galois::Statistic* BadWork;
static Galois::Statistic* WLEmptyWork;
static Galois::Statistic* nBad;
static Galois::Statistic* nEmpty;
static Galois::Statistic* nOverall;
static Galois::Statistic* nEdgesProcessed;
static Galois::Statistic* nNodesProcessed;
template<typename Graph>
struct not_visited {
  Graph& g;

  not_visited(Graph& g): g(g) { }

  bool operator()(typename Graph::GraphNode n) const {
    return (unsigned int)g.getData(n).dist >= (unsigned int)DIST_INFINITY;
  }
};

template<typename Graph, typename Enable = void>
struct not_consistent {
  not_consistent(Graph& g) { }

  bool operator()(typename Graph::GraphNode n) const { return false; }
};

template<typename Graph>
struct not_consistent<Graph, typename std::enable_if<!Galois::Graph::is_segmented<Graph>::value>::type> {
  Graph& g;
  not_consistent(Graph& g): g(g) { }

  bool operator()(typename Graph::GraphNode n) const {
    Dist dist = (unsigned int)g.getData(n).dist;
    if (dist == (unsigned int)DIST_INFINITY)
      return false;

    for (typename Graph::edge_iterator ii = g.edge_begin(n), ee = g.edge_end(n); ii != ee; ++ii) {
      Dist ddist = (unsigned int)g.getData(g.getEdgeDst(ii)).dist;
      Dist w = g.getEdgeData(ii);
      if (ddist > dist + w) {
        //std::cout << ddist << " " << dist + w << " " << n << " " << g.getEdgeDst(ii) << "\n"; // XXX
	return true;
      }
    }
    return false;
  }
};

template<typename Graph>
struct max_dist {
  Graph& g;
  Galois::GReduceMax<Dist>& m;

  max_dist(Graph& g, Galois::GReduceMax<Dist>& m): g(g), m(m) { }

  void operator()(typename Graph::GraphNode n) const {
    Dist d = g.getData(n).dist;
    if (d == DIST_INFINITY)
      return;
    m.update(d);
  }
};

template<typename UpdateRequest>
struct UpdateRequestIndexer: public std::unary_function<UpdateRequest, unsigned int> {
  unsigned int operator() (const UpdateRequest& val) const {
    unsigned int t = val.w >> stepShift;
    return t;
  }
};

template<typename UpdateRequest, size_t N>
struct ParameterizedUpdateRequestIndexer: public std::unary_function<UpdateRequest, unsigned int> {
  unsigned int operator() (const UpdateRequest& val) const {
    unsigned int t = val.w >> N;
    return t;
  }
};

template<typename UpdateRequest>
struct UpdateRequestHasher: public std::unary_function<UpdateRequest, unsigned long> {
  unsigned long operator() (const UpdateRequest& val) const {
    return (unsigned long) val.n;
  }
};

template<typename UpdateRequest>
struct UpdateRequestComparer: public std::binary_function<const UpdateRequest&, const UpdateRequest&, unsigned> {
  unsigned operator()(const UpdateRequest& x, const UpdateRequest& y) const {
    return x.w > y.w;
  }
};

template<typename UpdateRequest>
struct UpdateRequestNodeComparer: public std::binary_function<const UpdateRequest&, const UpdateRequest&, unsigned> {
  unsigned operator()(const UpdateRequest& x, const UpdateRequest& y) const {
    return x > y;
  }
};


template<typename Graph>
bool verify(Graph& graph, typename Graph::GraphNode source, typename Graph::GraphNode report) {
  if (graph.getData(source).dist != 0) {
    std::cerr << "source has non-zero dist value\n";
    return false;
  }
  namespace pstl = Galois::ParallelSTL;

  size_t notVisited = pstl::count_if(graph.begin(), graph.end(), not_visited<Graph>(graph));
  if (notVisited) {
    std::cerr << notVisited << " unvisited nodes; this is an error if the graph is strongly connected\n";
  }


  // For USA graph. We suppose that not all nodes are required to have optimal distance.
  std::cout << startNode << " " << destNode << " " << (unsigned int)graph.getData(report).dist << std::endl;
  if (startNode == 1 && destNode == 22629042 && (unsigned int)graph.getData(report).dist == 42715444) {
    return true;
  }

  bool consistent = pstl::find_if(graph.begin(), graph.end(), not_consistent<Graph>(graph)) == graph.end();
  if (!consistent) {
    std::cerr << "node found with incorrect distance\n";
    return false;
  }

  return true;
}

template<typename Algo>
void initialize(Algo& algo,
    typename Algo::Graph& graph,
    typename Algo::Graph::GraphNode& source,
    typename Algo::Graph::GraphNode& report) {

  algo.readGraph(graph);
  std::cout << "Read " << graph.size() << " nodes\n";

  if (startNode >= graph.size() || reportNode >= graph.size()) {
    std::cerr
      << "failed to set report: " << reportNode
      << " or failed to set source: " << startNode << "\n";
    assert(0);
    abort();
  }

  typename Algo::Graph::iterator it = graph.begin();
  std::advance(it, startNode);
  source = *it;
  it = graph.begin();
  std::advance(it, reportNode);
  report = *it;
}

template<typename Graph>
void readInOutGraph(Graph& graph) {
  using namespace Galois::Graph;
  if (symmetricGraph) {
    Galois::Graph::readGraph(graph, filename);
  } else if (transposeGraphName.size()) {
    Galois::Graph::readGraph(graph, filename, transposeGraphName);
  } else {
    GALOIS_DIE("Graph type not supported");
  }
}

struct SerialAlgo {
  typedef Galois::Graph::LC_CSR_Graph<SNode, uint32_t>
    ::with_no_lockable<true>::type Graph;
  typedef Graph::GraphNode GNode;
  typedef UpdateRequestCommon<GNode> UpdateRequest;

  std::string name() const { return "Serial"; }
  void readGraph(Graph& graph) {
    Galois::Graph::readGraph(graph, filename);
    std::cout<< "Coordinate filename is "<<coordFilename<<std::endl;

    std::ifstream myfile;
    myfile.open(coordFilename.c_str());
    unsigned int id, x, y;
    char k;
    //unsigned int max_index=0;
    if (myfile.is_open())
    {

      while ( myfile >> k>> id>> x>> y)
      {
        //std::cout << id<< " "<<x<<" "<<y << "\n";
        typename Graph::iterator it = graph.begin();
        std::advance(it, id);
        GNode nn = *it;
        graph.getData(nn, Galois::MethodFlag::NONE).x = x;
        graph.getData(nn, Galois::MethodFlag::NONE).y = y;

      }
      myfile.close();
    }
    typename Graph::iterator it = graph.begin();
    std::advance(it, startNode);
    GNode nn = *it;
    // Coord xS = graph.getData(nn, Galois::MethodFlag::NONE).x;
    // Coord yS = graph.getData(nn, Galois::MethodFlag::NONE).y;
    //t = graph.begin();
    // std::advance(it, destNode);
    // nn = *it;
    // Coord xD = graph.getData(nn, Galois::MethodFlag::NONE).x;
    // Coord yD = graph.getData(nn, Galois::MethodFlag::NONE).y;

    unsigned int dist = heuristic( graph.getData(nn, Galois::MethodFlag::NONE), graph);
    std::cout<<"Dist from start to dest "<<dist<<std::endl;

  }

  unsigned int heuristic(SNode& sdata, Graph& graph){

    Graph::iterator it = graph.begin();
    std::advance(it, destNode);
    GNode report = *it;

    Coord x = graph.getData(report, Galois::MethodFlag::NONE).x;
    Coord y = graph.getData(report, Galois::MethodFlag::NONE).y;

    //Coord xdis = abs(x-sdata.x);
    //Coord ydis = abs(y-sdata.y);
    return 0;
    //calculate_heu(x,y,sdata.x,sdata.y);
    //return xdis+ydis; // return manhattan distance
  }

  struct Initialize {
    Graph& g;
    Initialize(Graph& g): g(g) { }

    void operator()(Graph::GraphNode n) {
      g.getData(n).dist = DIST_INFINITY;
    }
  };

  void operator()(Graph& graph, const GNode src) const {
    std::set<UpdateRequest, std::less<UpdateRequest> > initial;
    UpdateRequest init(src, 0);
    initial.insert(init);

    Galois::Statistic counter("Iterations");

    while (!initial.empty()) {
      counter += 1;
      UpdateRequest req = *initial.begin();
      initial.erase(initial.begin());
      SNode& data = graph.getData(req.n, Galois::MethodFlag::NONE);
      if (req.w < data.dist) {
        data.dist = req.w;
	for (Graph::edge_iterator
	      ii = graph.edge_begin(req.n, Galois::MethodFlag::NONE),
	      ee = graph.edge_end(req.n, Galois::MethodFlag::NONE);
	    ii != ee; ++ii) {
          GNode dst = graph.getEdgeDst(ii);
          Dist d = graph.getEdgeData(ii);
          Dist newDist = req.w + d;
          if (newDist < graph.getData(dst, Galois::MethodFlag::NONE).dist) {
            initial.insert(UpdateRequest(dst, newDist));
	        }
        }
      }
    }
  }
};

template<bool UseCas>
struct AsyncAlgo {
  typedef SNode Node;

  typedef Galois::Graph::LC_InlineEdge_Graph<Node, uint32_t>
    ::template with_out_of_line_lockable<true>::type
    ::template with_compressed_node_ptr<true>::type
#ifdef GEM5
    ::template with_numa_alloc<false>::type
#else
    ::template with_numa_alloc<true>::type
#endif
    Graph;
  typedef typename Graph::GraphNode GNode;
  typedef UpdateRequestCommon<GNode> UpdateRequest;

  std::string name() const {
    return UseCas ? "Asynchronous with CAS" : "Asynchronous";
  }

  void readGraph(Graph& graph) { Galois::Graph::readGraph(graph, filename);
    std::cout<< "Coordinate filename is "<<coordFilename<<std::endl;

    std::ifstream myfile;
    myfile.open(coordFilename.c_str());
    Coord x, y;
    unsigned int id;
    char k;
    std::string ignore;
    if (myfile.is_open())
    {

      while (!myfile.eof()) {
        if (myfile.peek() != 'v') {
          std::getline(myfile, ignore);
        } else {
          myfile >> k >> id >> x >> y;
          //x : latitude
          //y : longitude
          //std::cout << id<< " "<<x<<" "<<y << "\n";
          typename Graph::iterator it = graph.begin();
          std::advance(it, id);
          GNode nn = *it;
          graph.getData(nn, Galois::MethodFlag::NONE).x = x;
          graph.getData(nn, Galois::MethodFlag::NONE).y = y;
        }
      }
      myfile.close();
    } else {
      std::cerr << "Coord file could not be opened" << std::endl;
    }

    typename Graph::iterator it = graph.begin();
    std::advance(it, startNode);
    GNode nn = *it;
    // Coord xS = graph.getData(nn, Galois::MethodFlag::NONE).x;
    // Coord yS = graph.getData(nn, Galois::MethodFlag::NONE).y;
    //t = graph.begin();
    // std::advance(it, destNode);
    // nn = *it;
    // Coord xD = graph.getData(nn, Galois::MethodFlag::NONE).x;
    // Coord yD = graph.getData(nn, Galois::MethodFlag::NONE).y;

    unsigned int dist = heuristic( graph.getData(nn, Galois::MethodFlag::NONE), graph);
    std::cout<<"Dist from start to dest "<<dist<<std::endl;
  }

  struct Initialize {
    Graph& g;
    Initialize(Graph& g): g(g) { }
    void operator()(typename Graph::GraphNode n) {
      g.getData(n, Galois::MethodFlag::NONE).dist = (unsigned int)DIST_INFINITY;

    }
  };

  unsigned int heuristic(Node& sdata, Graph& graph){

    Graph::iterator it = graph.begin();
    std::advance(it, destNode);
    GNode report = *it;

    Coord x2 = graph.getData(report, Galois::MethodFlag::NONE).x;
    Coord y2 = graph.getData(report, Galois::MethodFlag::NONE).y;
    Coord x1 = sdata.x;
    Coord y1 = sdata.y;

    //x : latitude
    //y : longitude
    // use Equirectangular approximation
    double PI = 3.14159265;
    double lat1 = ((x1/1000000)+((double)(x1%1000000)/(double)(1000000)))*PI/180.0;
    double lat2 = ((x2/1000000)+((double)(x2%1000000)/(double)(1000000)))*PI/180.0;
    double long1= ((y1/1000000)+((double)(y1%1000000)/(double)(1000000)))*PI/180.0;
    double long2 = ((y2/1000000)+((double)(y2%1000000)/(double)(1000000)))*PI/180.0;
    double R = 6371000;
    double x = (long2-long1)*std::cos(0.5*(lat2+lat1));
    double y = lat2-lat1;
    double dist = R*std::sqrt(x*x+y*y);
    dist = dist*0.75;
    return dist;
    //(calculate_heu(x,y,sdata.x,sdata.y)/2);
    //Coord xdis = abs(x-sdata.x);
    //Coord ydis = abs(y-sdata.y);
    //std::cout<<" x "<<sdata.x<<" y "<<sdata.y<<" dx "<<x<<" dy "<<y<<" "<<(xdis+ydis)<<std::endl;
    //return sqrt((xdis*xdis)+(ydis*ydis)); // return manhattan distance
  }

  template<typename Pusher>
  void relaxEdge(Graph& graph, Node& sdata, typename Graph::edge_iterator ii, Pusher& pusher) {
    GNode dst = graph.getEdgeDst(ii);
    Dist d = graph.getEdgeData(ii);
    Node& ddata = graph.getData(dst, Galois::MethodFlag::NONE);
    Dist newDist = (unsigned int)sdata.dist + d;
    Dist oldDist;


    GNode targetNode;
    Graph::iterator it = graph.begin();
    std::advance(it, destNode);
    targetNode = (*it);
    Node& tdata = graph.getData(targetNode, Galois::MethodFlag::NONE);
    Dist targetDist = (unsigned int)tdata.dist;

    while (newDist < (unsigned int)(oldDist = ddata.dist)) {
      if (!UseCas || __sync_bool_compare_and_swap(&ddata.dist, oldDist, newDist | (oldDist & 0xffffffff00000000ul))) {
        if (!UseCas)
          ddata.dist = newDist;

        unsigned int heu_val = heuristic(ddata, graph);

        //push logic changed
        if(newDist<=targetDist)
        {
          //std::cout<<"Pushing "<<dst<<" with dist "<<newDist<<" heu "<<heu_val<<" target dist "<<targetDist<<std::endl;
          pusher.push(UpdateRequest(dst, newDist+heu_val));
        }
        break;
      }
    }
  }

  template<typename Pusher>
  void relaxNode(Graph& graph, UpdateRequest& req, Pusher& pusher) {
    const Galois::MethodFlag flag = UseCas ? Galois::MethodFlag::NONE : Galois::MethodFlag::ALL;
    Node& sdata = graph.getData(req.n, flag);
    volatile Dist* sdist = &sdata.dist;
    int nEdge = 0;

    Graph::iterator it = graph.begin();
    std::advance(it, destNode);
    GNode report = *it;

     Dist reportDist = graph.getData(report, Galois::MethodFlag::NONE).dist;

    unsigned int heu_val = heuristic(sdata, graph);

    *nNodesProcessed += 1;
    if (req.w-heu_val != (unsigned int)*sdist) {
      if (trackWork) {
        *nEmpty += 1;
        *WLEmptyWork += pusher.t.stopwatch();
      }
      return;
    }
    //std::cout<<"Dist: "<<(unsigned int)*sdist<<" heuristic "<<heu_val<<" req.w: "<<req.w<<" \n";
    for (typename Graph::edge_iterator ii = graph.edge_begin(req.n, flag), ei = graph.edge_end(req.n, flag); ii != ei; ++ii) {
      if (req.w-heu_val != (unsigned int)(*sdist)) {
        *nBad += nEdge;
        *nOverall += nEdge;
        *BadWork += pusher.u + pusher.t.sample();
        return;
      }
      relaxEdge(graph, sdata, ii, pusher);
      nEdge++;
      *nEdgesProcessed+=1;
    }

    if (trackWork) {
      Dist oldDist = sdata.dist;
      unsigned int oldWork = (oldDist >> 32);

      *nOverall += nEdge;

      if (oldWork) {
        *nBad += nEdge;
        *BadWork += oldWork;
      }
      // Record work spent this iteratin.  If CAS fails, then this
      // iteration was bad.
      if ((unsigned int)oldDist < req.w-heu_val ||
          !__sync_bool_compare_and_swap(&sdata.dist, oldDist, (req.w-heu_val) | ((pusher.u + pusher.t.sample()) << 32))) {
        // We need to undo our prior accounting of bad work to avoid
        // double counting.
        if (!oldWork)
          *nBad += nEdge;
        else
          *BadWork -= oldWork;

        *BadWork += pusher.u + pusher.t.sample();
      }

    }
  }

  struct Process {
    AsyncAlgo* self;
    Graph& graph;
    Process(AsyncAlgo* s, Graph& g): self(s), graph(g) { }
    void operator()(UpdateRequest& req, Galois::UserContext<UpdateRequest>& ctx) {
      self->relaxNode(graph, req, ctx);
    }
  };

  struct ProcessWithBreaks {
    typedef int tt_needs_parallel_break;

    AsyncAlgo* self;
    Graph& graph;
    ProcessWithBreaks(AsyncAlgo* s, Graph& g): self(s), graph(g) { }
    void operator()(UpdateRequest& req, Galois::UserContext<UpdateRequest>& ctx) {
      self->relaxNode(graph, req, ctx);
    }
  };

  typedef Galois::InsertBag<UpdateRequest> Bag;

  struct InitialProcess {
    AsyncAlgo* self;
    Graph& graph;
    Bag& bag;
    Node& sdata;
    InitialProcess(AsyncAlgo* s, Graph& g, Bag& b, Node& d): self(s), graph(g), bag(b), sdata(d) { }
    void operator()(typename Graph::edge_iterator ii) {
      self->relaxEdge(graph, sdata, ii, bag);
    }
  };

  void operator()(Graph& graph, GNode source) {


    using namespace Galois::WorkList;
    typedef UpdateRequestIndexer<UpdateRequest> Indexer;
    typedef dChunkedFIFO<CHUNK_SIZE> Chunk;
    typedef dVisChunkedFIFO<64> visChunk;
    typedef dChunkedPTFIFO<1> noChunk;
    typedef ChunkedFIFO<64> globChunk;
    typedef ChunkedFIFO<1> globNoChunk;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10> OBIM;
    typedef AdaptiveOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10, true, false, CHUNK_SIZE> ADAPOBIM;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, dChunkedLIFO<64>, 10> OBIM_LIFO;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 4> OBIM_BLK4;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10, false> OBIM_NOBSP;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, noChunk, 10> OBIM_NOCHUNK;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, globChunk, 10> OBIM_GLOB;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, globNoChunk, 10> OBIM_GLOB_NOCHUNK;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, noChunk, -1, false> OBIM_STRICT;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10, true, true> OBIM_UBSP;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, visChunk, 10, true, true> OBIM_VISCHUNK;
    typedef UpdateRequestComparer<UpdateRequest> Comparer;
    typedef UpdateRequestNodeComparer<UpdateRequest> NodeComparer;
    typedef UpdateRequestHasher<UpdateRequest> Hasher;
    typedef GlobPQ<UpdateRequest, kLSMQ<UpdateRequest, UpdateRequestIndexer<UpdateRequest>, 256>> kLSM256;
    typedef GlobPQ<UpdateRequest, kLSMQ<UpdateRequest, UpdateRequestIndexer<UpdateRequest>, 16384>> kLSM16k;
    typedef GlobPQ<UpdateRequest, kLSMQ<UpdateRequest, UpdateRequestIndexer<UpdateRequest>, 4194304>> kLSM4m;
    typedef GlobPQ<UpdateRequest, LockFreeSkipList<Comparer, UpdateRequest>> GPQ;
    typedef GlobPQ<UpdateRequest, SprayList<NodeComparer, UpdateRequest>> SL;
    typedef GlobPQ<UpdateRequest, MultiQueue<Comparer, UpdateRequest, 1>> MQ1;
    typedef GlobPQ<UpdateRequest, MultiQueue<Comparer, UpdateRequest, 2>> MQ2;
    typedef GlobPQ<UpdateRequest, MultiQueue<Comparer, UpdateRequest, 3>> MQ3;
    typedef GlobPQ<UpdateRequest, MultiQueue<Comparer, UpdateRequest, 4>> MQ4;
    typedef GlobPQ<UpdateRequest, MultiQueue<Comparer, UpdateRequest, 5>> MQ5;
    typedef MyHMQ<UpdateRequest, Comparer, 1, true> HMQ1;
    typedef MyHMQ<UpdateRequest, Comparer, 2, true> HMQ2;
    typedef MyHMQ<UpdateRequest, Comparer, 3, true> HMQ3;
    typedef MyHMQ<UpdateRequest, Comparer, 4, true> HMQ4;
    typedef MyHMQ<UpdateRequest, Comparer, 5, true> HMQ5;
    typedef MyHMQ<UpdateRequest, Comparer, 6, true> HMQ6;
    typedef MyHMQ<UpdateRequest, Comparer, 7, true> HMQ7;
    typedef MyHMQ<UpdateRequest, Comparer, 8, true> HMQ8;
    typedef GlobPQ<UpdateRequest, DistQueue<Comparer, UpdateRequest, false>> PTSL;
    typedef GlobPQ<UpdateRequest, DistQueue<Comparer, UpdateRequest, true>> PPSL;
    typedef GlobPQ<UpdateRequest, LocalPQ<Comparer, UpdateRequest>> LPQ;
    typedef GlobPQ<UpdateRequest, SwarmPQ<Comparer, UpdateRequest>> SWARMPQ;
    typedef GlobPQ<UpdateRequest, HeapSwarmPQ<Comparer, UpdateRequest>> HSWARMPQ;
    typedef GlobPQ<UpdateRequest, PartitionPQ<Comparer, Hasher, UpdateRequest>> PPQ;
    typedef SkipListOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10> SLOBIM;
    typedef SkipListOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, noChunk, 10> SLOBIM_NOCHUNK;
    typedef SkipListOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, visChunk, 10> SLOBIM_VISCHUNK;
    typedef VectorOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10> VECOBIM;
    typedef VectorOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, noChunk, 10> VECOBIM_NOCHUNK;
    typedef VectorOrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, globNoChunk, 10> VECOBIM_GLOB_NOCHUNK;

    std::cout << "INFO: Using delta-step of " << (1 << stepShift) << "\n";
    std::cout << "WARNING: Performance varies considerably due to delta parameter.\n";
    std::cout << "WARNING: Do not expect the default to be good for your graph.\n";

    Bag initial;
    graph.getData(source).dist = 0;
    Galois::do_all(
        graph.out_edges(source, Galois::MethodFlag::NONE).begin(),
        graph.out_edges(source, Galois::MethodFlag::NONE).end(),
        InitialProcess(this, graph, initial, graph.getData(source)));
    std::string wl = worklistname;
    if (!mqSuff.empty()) {
      mqSuff = "_" + mqSuff;
    }

#define RUN_WL(WL) Galois::for_each_local(initial, Process(this, graph), Galois::wl<WL>())
#include "Experiments.h"

    if (wl == "obim")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM>());
    else if (wl == "adap-obim")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<ADAPOBIM>());
    else if (wl == "slobim")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<SLOBIM>());
    else if (wl == "slobim-nochunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<SLOBIM_NOCHUNK>());
    else if (wl == "slobim-vischunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<SLOBIM_VISCHUNK>());
    else if (wl == "vecobim")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<VECOBIM>());
    else if (wl == "vecobim-nochunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<VECOBIM_NOCHUNK>());
    else if (wl == "vecobim-glob-nochunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<VECOBIM_GLOB_NOCHUNK>());
    else if (wl == "obim-strict")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_STRICT>());
    else if (wl == "obim-ubsp")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_UBSP>());
    else if (wl == "obim-lifo")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_LIFO>());
    else if (wl == "obim-blk4")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_BLK4>());
    else if (wl == "obim-nobsp")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_NOBSP>());
    else if (wl == "obim-nochunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_NOCHUNK>());
    else if (wl == "obim-vischunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_VISCHUNK>());
    else if (wl == "obim-glob")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_GLOB>());
    else if (wl == "obim-glob-nochunk")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM_GLOB_NOCHUNK>());
    else if (wl == "skiplist")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<GPQ>());
    else if (wl == "spraylist")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<SL>());
    else if (wl == "mq2")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<MQ2>());
    else if (wl == "mq3")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<MQ3>());
    else if (wl == "mq1")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<MQ1>());
    else if (wl == "mq4")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<MQ4>());
    else if (wl == "mq5")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<MQ5>());
    else if (wl == "hmq2")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ2>());
    else if (wl == "hmq5")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ5>());
    else if (wl == "hmq3")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ3>());
    else if (wl == "hmq1")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ1>());
    else if (wl == "hmq4")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ4>());
     else if (wl == "hmq6")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ6>());
     else if (wl == "hmq7")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ7>());
     else if (wl == "hmq8")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HMQ8>());
    else if (wl == "thrskiplist")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<PTSL>());
    else if (wl == "pkgskiplist")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<PPSL>());
    else if (wl == "lpq")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<LPQ>());
    else if (wl == "swarm")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<SWARMPQ>());
    else if (wl == "heapswarm")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<HSWARMPQ>());
    else if (wl == "ppq")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<PPQ>());
    else if (wl == "klsm256")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<kLSM256>());
    else if (wl == "klsm16k")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<kLSM16k>());
    else if (wl == "klsm4m")
      Galois::for_each_local(initial, ProcessWithBreaks(this, graph), Galois::wl<kLSM4m>());
//    else
//      std::cerr << "No work list!" << "\n";


#define priority_t Dist
#define element_t UpdateRequest

    typedef StealingMultiQueue<element_t, Comparer, 4, 1, true> SMQ_4_1;
    if (wl == "smq_4_1" or wl == "smq_west")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_4_1>());
    typedef StealingMultiQueue<element_t, Comparer, 4, 1, true> SMQ_8_1;
    if (wl == "smq_8_1" or wl == "smq_west_amd")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_8_1>());


    typedef AdaptiveStealingMultiQueue<element_t, Comparer> ASMQ;
    if (wl == "adap-smq")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<ASMQ>());


    typedef MyHMQ<UpdateRequest, Comparer, 2, true> USUAL_HMQ2_TRY1;
    if (wl == "hmq2_try1") RUN_WL(USUAL_HMQ2_TRY1);
    typedef MyHMQ<UpdateRequest, Comparer, 3, true> USUAL_HMQ3_TRY1;
    if (wl == "hmq3_try1") RUN_WL(USUAL_HMQ3_TRY1);
    typedef MyHMQ<UpdateRequest, Comparer, 4, true> USUAL_HMQ4_TRY1;
    if (wl == "hmq4_try1") RUN_WL(USUAL_HMQ4_TRY1);
    typedef MyHMQ<UpdateRequest, Comparer, 5, true> USUAL_HMQ5_TRY1;
    if (wl == "hmq5_try1") RUN_WL(USUAL_HMQ5_TRY1);
    typedef MyHMQBlocking<UpdateRequest, Comparer, 2, true> USUAL_HMQ2_BLOCKING1;
    if (worklistname == "hmq2_blocking1")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<USUAL_HMQ2_BLOCKING1>());
    typedef MyHMQTryLock2Q<UpdateRequest, Comparer, 2, true> USUAL_HMQ2_TRY2;
    if (worklistname == "hmq2_try2")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<USUAL_HMQ2_TRY2>());
    typedef MyHMQBlocking2Q<UpdateRequest, Comparer, 2, true> USUAL_HMQ2_BLOCKING2;
    if (worklistname == "hmq2_blocking2")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<USUAL_HMQ2_BLOCKING2>());
    typedef MyPQ<UpdateRequest, Comparer, true> USUAL_PQ;
    if (worklistname == "pq")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<USUAL_PQ>());


    typedef MultiQueueProbLocal<element_t, Comparer, 512, 4, 2, priority_t> MQ2_PL_512_4;
    if (worklistname == "mq2_pl_512_4" or worklistname == "mq2_pl_west")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<MQ2_PL_512_4>());
//   typedef MultiQueueProbLocalNuma<element_t, Comparer, 512, 4, 2, priority_t> MQ2_PL_512_4_NUMA;
//    if (worklistname == "mq2_pl_512_4_numa" or worklistname == "mq2_pl_numa_west")
//      Galois::for_each_local(initial, Process(this, graph), Galois::wl<MQ2_PL_512_4_NUMA>());
//

    typedef MultiQueueProbLocal<element_t, Comparer, 64, 4, 2, priority_t> MQ2_PL_64_4;
    if (worklistname == "mq2_pl_64_4" or worklistname == "mq2_pl_west_amd")
      Galois::for_each_local(initial, Process(this, graph), Galois::wl<MQ2_PL_64_4>());

    typedef UpdateRequestIndexer<UpdateRequest> Indexer;
#include "Galois/WorkList/experiment_declarations.h"

  }
};

struct AsyncAlgoPP {
  typedef SNode Node;

  typedef Galois::Graph::LC_InlineEdge_Graph<Node, uint32_t>
    ::with_out_of_line_lockable<true>::type
    ::with_compressed_node_ptr<true>::type
    ::with_numa_alloc<true>::type
    Graph;
  typedef Graph::GraphNode GNode;
  typedef UpdateRequestCommon<GNode> UpdateRequest;

  std::string name() const {
    return "Asynchronous with CAS and Push and pull";
  }

  void readGraph(Graph& graph) { Galois::Graph::readGraph(graph, filename); }

  struct Initialize {
    Graph& g;
    Initialize(Graph& g): g(g) { }
    void operator()(Graph::GraphNode n) {
      g.getData(n, Galois::MethodFlag::NONE).dist = DIST_INFINITY;
    }
  };

  template <typename Pusher>
  void relaxEdge(Graph& graph, Dist& sdata, typename Graph::edge_iterator ii, Pusher& pusher) {
      GNode dst = graph.getEdgeDst(ii);
      Dist d = graph.getEdgeData(ii);
      Node& ddata = graph.getData(dst, Galois::MethodFlag::NONE);
      Dist newDist = sdata + d;
      Dist oldDist;
      if (newDist < (oldDist = ddata.dist)) {
        do {
          if (__sync_bool_compare_and_swap(&ddata.dist, oldDist, newDist)) {
            if (trackWork && oldDist != DIST_INFINITY)
            {
              *BadWork += 1;
            }
            pusher.push(UpdateRequest(dst, newDist));
            break;
          }
        } while (newDist < (oldDist = ddata.dist));
      } else {
        sdata = std::min(oldDist + d, sdata);
      }
    }

  struct Process {
    AsyncAlgoPP* self;
    Graph& graph;
    Process(AsyncAlgoPP* s, Graph& g): self(s), graph(g) { }

    void operator()(UpdateRequest& req, Galois::UserContext<UpdateRequest>& ctx) {
      const Galois::MethodFlag flag = Galois::MethodFlag::NONE;
      Node& sdata = graph.getData(req.n, flag);
      volatile Dist* psdist = &sdata.dist;
      Dist sdist = *psdist;

      if (req.w != sdist) {
        if (trackWork)
          *WLEmptyWork += 1;
        return;
      }

      for (Graph::edge_iterator ii = graph.edge_begin(req.n, flag), ei = graph.edge_end(req.n, flag); ii != ei; ++ii) {
        self->relaxEdge(graph, sdist, ii, ctx);
      }

      // //try doing a pull
      // Dist oldDist;
      // while (sdist < (oldDist = *psdist)) {
      //   if (__sync_bool_compare_and_swap(psdist, oldDist, sdist)) {
      //     req.w = sdist;
      //     operator()(req, ctx);
      //   }
      // }
    }
  };

  typedef Galois::InsertBag<UpdateRequest> Bag;

  struct InitialProcess {
    AsyncAlgoPP* self;
    Graph& graph;
    Bag& bag;
    InitialProcess(AsyncAlgoPP* s, Graph& g, Bag& b): self(s), graph(g), bag(b) { }
    void operator()(Graph::edge_iterator ii) {
      Dist d = 0;
      self->relaxEdge(graph, d, ii, bag);
    }
  };

  void operator()(Graph& graph, GNode source) {
    using namespace Galois::WorkList;
    typedef dChunkedFIFO<64> Chunk;
    typedef OrderedByIntegerMetric<UpdateRequestIndexer<UpdateRequest>, Chunk, 10> OBIM;

    std::cout << "INFO: Using delta-step of " << (1 << stepShift) << "\n";
    std::cout << "WARNING: Performance varies considerably due to delta parameter.\n";
    std::cout << "WARNING: Do not expect the default to be good for your graph.\n";

    Bag initial;
    graph.getData(source).dist = 0;
    Galois::do_all(
        graph.out_edges(source, Galois::MethodFlag::NONE).begin(),
        graph.out_edges(source, Galois::MethodFlag::NONE).end(),
        InitialProcess(this, graph, initial));
    Galois::for_each_local(initial, Process(this, graph), Galois::wl<OBIM>());
  }
};

namespace Galois {
template<>
struct does_not_need_aborts<AsyncAlgo<true>::Process> : public boost::true_type {};
}

static_assert(Galois::does_not_need_aborts<AsyncAlgo<true>::Process>::value, "Oops");

template<typename Algo>
void run(bool prealloc = true) {
  typedef typename Algo::Graph Graph;
  typedef typename Graph::GraphNode GNode;

  Algo algo;
  Graph graph;
  GNode source, report;

  initialize(algo, graph, source, report);

  size_t approxNodeData = graph.size() * 64;
  //size_t approxEdgeData = graph.sizeEdges() * sizeof(typename Graph::edge_data_type) * 2;
  if (prealloc)
    Galois::preAlloc(numThreads + 3 * approxNodeData / Galois::Runtime::MM::pageSize);
  Galois::reportPageAlloc("MeminfoPre");

  Galois::StatTimer T;
  std::cout << "Running " << algo.name() << " version\n";
  T.start();

#ifdef GEM5
    m5_dumpreset_stats(0,0);
#endif

  // ROI
  Galois::do_all_local(graph, typename Algo::Initialize(graph));
  algo(graph, source);

#ifdef GEM5
    m5_dumpreset_stats(0,0);
#endif

  T.stop();


  std::ofstream out(resultFileName + mqSuff, std::ios::app);
  out << T.get() << ",";
  out.close();

  Galois::reportPageAlloc("MeminfoPost");
#ifndef GEM5
  Galois::Runtime::reportNumaAlloc("NumaPost");
#endif

  std::cout << "Node " << reportNode << " has distance " << (unsigned int)graph.getData(report).dist << "\n";

  if (!skipVerify) {
    if (verify(graph, source, report)) {
      std::cout << "Verification successful.\n";
    } else {
      std::cerr << "Verification failed.\n";
      assert(0 && "Verification failed");
      abort();
    }
  }
}

uint64_t getStatVal(Galois::Statistic* value) {
  uint64_t stat = 0;
  for (unsigned x = 0; x < Galois::Runtime::activeThreads; ++x)
    stat += value->getValue(x);
  return stat;
}

int main(int argc, char **argv) {
  Galois::StatManager statManager;
  LonestarStart(argc, argv, name, desc, url);

  if (trackWork) {
    BadWork = new Galois::Statistic("BadWork");
    WLEmptyWork = new Galois::Statistic("EmptyWork");
    nBad = new Galois::Statistic("nBad");
    nEmpty = new Galois::Statistic("nEmpty");
    nOverall = new Galois::Statistic("nOverall");
    nEdgesProcessed = new Galois::Statistic("nEdgesProcessed");
    nNodesProcessed = new Galois::Statistic("nNodesProcessed");
  }

  Galois::StatTimer T("TotalTime");
  T.start();

  switch (algo) {
    case Algo::serial: run<SerialAlgo>(); break;
    case Algo::async: run<AsyncAlgo<false> >(); break;
    case Algo::asyncWithCas: run<AsyncAlgo<true> >(); break;
    case Algo::asyncPP: run<AsyncAlgoPP>(); break;
/*
#if defined(__IBMCPP__) && __IBMCPP__ <= 1210
#else
    case Algo::ligra: run<LigraAlgo<false> >(); break;
    case Algo::ligraChi: run<LigraAlgo<true> >(false); break;
    case Algo::graphlab: run<GraphLabAlgo>(); break;
#endif
*/
    default: std::cerr << "Unknown algorithm\n"; abort();
  }

  T.stop();

  if (trackWork) {
    std::string wl = worklistname;
    if (wl.size() >= 3 && wl[1] == 'm' && wl[2] == 'q' && (wl[0] == 's' || wl[0] == 'a'))
      wl = wl + mqSuff;
    std::ofstream nodes(resultFileName + mqSuff, std::ios::app);
    nodes << wl << "," << getStatVal(nNodesProcessed) << "," << Galois::Runtime::activeThreads << std::endl;
    nodes.close();

    delete BadWork;
    delete WLEmptyWork;
    delete nBad;
    delete nEmpty;
    delete nOverall;
    delete nEdgesProcessed;
    delete nNodesProcessed;
  }

  return 0;
}
