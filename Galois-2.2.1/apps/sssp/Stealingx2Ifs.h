#ifdef STEALINGx2
if (wl == "smq_1_4_x2")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_4_x2>());
if (wl == "smq_1_8_x2")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_8_x2>());
if (wl == "smq_1_16_x2")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_16_x2>());
if (wl == "smq_1_32_x2")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_32_x2>());
#endif
