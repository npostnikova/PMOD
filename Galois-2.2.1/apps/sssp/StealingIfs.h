#ifdef STEALING
if (wl == "smq_1_4")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_4>());
if (wl == "smq_1_8")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_8>());
if (wl == "smq_1_16")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_16>());
if (wl == "smq_1_32")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_32>());
if (wl == "smq_1_64")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_64>());
if (wl == "smq_1_128")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_128>());
if (wl == "smq_1_256")
  Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_256>());
#endif
