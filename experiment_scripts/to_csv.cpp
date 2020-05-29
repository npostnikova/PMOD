#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <map>
#include <iomanip>

using namespace std;

vector<string> split(string const &s, string const &delimiter) {
  size_t pos_start = 0;
  size_t pos_end;
  size_t delim_len = delimiter.length();
  string token;
  vector<string> res;

  while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }
  res.push_back(s.substr(pos_start));
  return res;
}

class Data {
  vector<uint64_t> processed;
  vector<uint64_t> time;
  uint64_t timeSum = 0;
  size_t attempts = 0;

  uint64_t avg(vector<uint64_t> const& v) {
    uint64_t sum = 0;
    for (auto val: v)
      sum += val;

    return v.empty() ? 0 : sum / v.size();
  }

public:

  void addProcessed(uint64_t proc) {
    processed.push_back(proc);
  }

  void addTime(uint64_t t) {
    time.push_back(t);
  }

  uint64_t avgTime() {
    return avg(time);
  }

  uint64_t avgProcessed() {
    return avg(processed);
  }

  uint64_t standDevProcessed() {
    if (processed.empty())
      return 0;
    auto avgVal = avgProcessed();
    uint64_t sum = 0;
    for (auto p : processed)
      sum += (p - avgVal) * (p - avgVal);
    return sqrt((double)sum / (processed.size() - 1 ? processed.size() - 1 : 1));
  }

  uint64_t standDevTime() {
    if (time.empty())
      return 0;
    auto avgVal = avgTime();
    uint64_t sum = 0;
    for (auto p : time)
      sum += (p - avgVal) * (p - avgVal);
    return sqrt((double)sum / (time.size() - 1 > 0 ? processed.size() - 1 : 1));
  }
};

void printProbs(ostream& out, vector<string> probs) {
  out << "0";
  for (auto p : probs)
    out << ", " << p;
  out << endl;
}

int main(int argc, char* argv[]) {
  if (argc <= 3) {
    cout << "Specify input and result files and number of nodes";
    return 0;
  }
  string in = argv[1];
  string out = argv[2];
  size_t V = stoul(argv[3]);
  vector<string> probs = {"1", "0.95", "0.9", "0.5", "0.1", "0.05", "0.03", "0.01", "0.005", "0.001"};
  map<string, size_t> prob_map;
  for (size_t i = 0; i < probs.size(); i++)
    prob_map.insert({probs[i], i});

  string id = (argc >= 1) ? argv[1] : "";
  std::ifstream input(in);
  string line;

  auto n = probs.size();
  vector<vector<Data>> amq2(n, vector<Data>(n, Data()));

  while (getline(input, line)) {
    stringstream s;
    s << line;

    string name;
    uint64_t v;
    uint64_t time = 0; // todo
    s >> time >> name >> v;// >> time;

    auto parts = split(name, "_"); // amqc_push_pop
    auto i = prob_map[parts[1]];
    auto j = prob_map[parts[2]];

    amq2[i][j].addProcessed(v);
    amq2[i][j].addTime(time);
  }

  ofstream outProcessed(out, ios::app);
//  ofstream outTime("time" + id + ".csv");
//  ofstream outStandDev("stand_dev" + id + ".csv");
  auto parts = split(in, "_");
  outProcessed << parts[0] << ", t=" << parts[1] << ", c=" << parts[2] << endl << endl;
  printProbs(outProcessed, probs);
//  printProbs(outTime, probs);
//  printProbs(outStandDev, probs);

  for (size_t i = 0; i < amq2.size(); i++) {
    outProcessed << probs[i];
//    outTime << probs[i];
//    outStandDev << probs[i];

    for (size_t j = 0; j < amq2.size(); j++) {
      double pg = (double)amq2[i][j].standDevProcessed() / (amq2[i][j].avgProcessed() * 0.01);
      outProcessed << fixed << setprecision(3) << ", " << (double)amq2[i][j].avgProcessed() / V;
      if (pg > 0.5) outProcessed << fixed << setprecision(1) << "+-" << pg << "%";
      auto t = (double) amq2[i][j].standDevTime() /  (amq2[i][j].avgTime() * 0.01 );
      outProcessed << "    " << fixed << setprecision(3) << (double)amq2[0][0].avgTime() / amq2[i][j].avgTime();
      if (t > 0.5) outProcessed << fixed << setprecision(1) << "+-" << t << "%";

      //outTime << ", " << amq2[i][j].avgTime();
      //outStandDev << ", " << (double)amq2[i][j].standDevProcessed() / 10703.76;
    }
    outProcessed << endl;
    //outTime << endl;
    //outStandDev << endl;
  }
  outProcessed << "\n\n\n";
  input.close();
  outProcessed.close();
  //outTime.close();
  //outStandDev.close();
}

