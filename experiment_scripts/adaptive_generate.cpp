#include <istream>
#include <fstream>
#include <string>
#include <vector>


using namespace std;

int main() {
  vector<int> rightBorder;
  vector<int> leftBorder;
  for (int i = 32; i <= 1024; i *= 2) {
    rightBorder.push_back(i);
    leftBorder.push_back(-i);
  }
  vector<size_t> windowSize;
  for (size_t i = 8; i < 1024; i *= 2) {
    windowSize.push_back(i);
  }
  vector<size_t> emptyWeight = {1, 2};
  vector<double> percent = { 50, 60, 70, 80, 85, 90 };
  ofstream typedefs("AdapTypedefs.h");
  ofstream ifs("AdapIfs.h");
  ofstream script("adap_script.sh");

  typedefs << "#ifndef ADAPWPEXP" << endl;
  typedefs << "#define ADAPWPEXP" << endl;

  ifs << "#ifdef ADAPWPEXP" << endl;

  for (auto r : rightBorder) {
    for (auto l : leftBorder) {
      for (auto ws: windowSize) {
        for (auto e : emptyWeight) {
          for (auto p : percent) {
            string suff = "_" + to_string(e) + "_" + to_string(-l) + "_" + to_string(r) + "_" + to_string(ws) + "_" + to_string(p);
            typedefs
            << "typedef AdaptiveMultiQueue<UpdateRequest, Comparer, 2, false, void, true, false, Prob <1, 1>, Prob <1, 1>, 0, 1, 1, "
            <<
            e << ", " << l << ", " << r << ", " << ws << ", " << p /100 << "> AMQ2WP" << suff << ";" << endl;
            ifs << "if (wl == \"amqwp2" + suff + "\")\n"
                                               "\tGalois::for_each_local(initial, Process(this, graph), Galois::wl<AMQWP2" +
                   suff + ">());" << endl;
            script << "$APP -t=${THREADS} -wl=amqwp2" << suff << " --resultFile=${RES}_adap_${THREADS} $GRAPH_PATH"
                   << endl;
          }
        }
      }
    }
  }

  typedefs << "#endif" << endl;
  ifs << "#endif" << endl;
}