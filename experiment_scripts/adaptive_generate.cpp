#include <istream>
#include <fstream>
#include <string>
#include <vector>


using namespace std;

int main() {
  vector<int> rightBorder;
  vector<int> leftBorder;
  for (int i = 128; i < 3000; i *= 2) {
    rightBorder.push_back(i);
    leftBorder.push_back(-i);
  }
  vector<size_t> windowSize;
  for (size_t i = 32; i < 1024; i *= 2) {
    windowSize.push_back(i);
  }
  vector<size_t> emptyWeight = {1};
  ofstream typedefs("AdapTypedefs.h");
  ofstream ifs("AdapIfs.h");
  ofstream script("adap_script.sh");

  typedefs << "#ifndef ADAPEXP" << endl;
  typedefs << "#define ADAPEXP" << endl;

  ifs << "#ifdef ADAPEXP" << endl;

  for (auto r : rightBorder) {
    for (auto l : leftBorder) {
      for (auto ws: windowSize) {
        for (auto e : emptyWeight) {
          string suff = "_" + to_string(e) + "_" + to_string(-l) + "_" + to_string(r) + "_" + to_string(ws);
          typedefs
          << "typedef AdaptiveMultiQueue<UpdateRequest, Comparer, 2, false, void, true, false, Prob <1, 1>, Prob <1, 1>, 0, 1, 1, "
          <<
          e << ", " << l << ", " << r << ", " << ws << "> AMQ2" << suff << ";" << endl;
          ifs << "if (wl == \"amq2" + suff + "\")\n"
                                             "\tGalois::for_each_local(initial, Process(this, graph), Galois::wl<AMQ2" +
                 suff + ">());" << endl;
          script << "$APP -t=${THREADS} -wl=amq2" << suff << " --resultFile=${RES}_adap_${THREADS} $GRAPH_PATH" << endl;
        }
      }
    }
  }

  typedefs << "#endif" << endl;
  ifs << "#endif" << endl;
}