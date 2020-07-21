#include <istream>
#include <fstream>
#include <string>
#include <vector>


using namespace std;

int main() {
  vector<vector<size_t>> windowSizes = { {32, 64, 128}, {256, 512} };
  vector<size_t> weights = {1, 2, 3 };
  vector<size_t> percent = { 80, 90, 95 };
  vector<size_t> constt = {0, 1, 2};


  for (size_t i = 0; i < windowSizes.size(); i++) {
    auto v = i + 1;
    auto windowSize = windowSizes[i];
    for (auto c :constt) {
      ofstream typedefs("../Galois-2.2.1/apps/sssp/FixedSegmentTypedefs" + to_string(v) + to_string(c) + ".h");
      ofstream ifs("../Galois-2.2.1/apps/sssp/FixedSegmentIfs" + to_string(v) + to_string(c) + ".h");
      ofstream script("fixed_segment_script" + to_string(v) + to_string(c) + ".sh");

      typedefs << "#ifndef ADAPFSEXP" << v << c << endl;
      typedefs << "#define ADAPFSEXP" << v << c << endl;

      ifs << "#ifdef ADAPFSEXP" << v << c << endl;

      for (auto ws: windowSize) {
        for (auto p : percent) {
          for (auto sw : weights) {
            for (auto fw : weights) {
              for (auto ew : weights) {
                string suff = to_string(c) +
                              "_" + to_string(sw) + to_string(fw) + to_string(ew) + "_" + to_string(ws) + "_" +
                              to_string(p);
                typedefs
                << "typedef AdaptiveMultiQueue<UpdateRequest, Comparer, " << c
                << ", false, void, true, false, Prob <1, 1>, Prob <1, 1>, 0, "
                <<
                sw << ", " << fw << ", " << ew << ", " << ws << ", " << p << "> AMQ" << suff << ";" << endl;
                ifs << "if (wl == \"amq" + suff + "\")\n"
                                                  "\tGalois::for_each_local(initial, Process(this, graph), Galois::wl<AMQ" +
                       suff + ">());" << endl;
                script << "$APP -t=${THREADS} -wl=amq" << suff
                       << " --resultFile=${RES}${CONST}_fs_${THREADS} $GRAPH_PATH"
                       << endl;
              }
            }
          }
        }
      }
      typedefs << "#endif" << endl;
      ifs << "#endif" << endl;
      typedefs.close();
      ifs.close();
      script.close();
    }
  }

}