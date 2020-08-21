#ifdef STEALING
if (wl == "smq_1_10")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_10>());
if (wl == "smq_1_25")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_25>());
if (wl == "smq_1_64")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_64>());
if (wl == "smq_1_100")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_100>());
if (wl == "smq_1_256")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_256>());
if (wl == "smq_1_1000")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_1000>());
if (wl == "smq_1_10000")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_10000>());
#endif
