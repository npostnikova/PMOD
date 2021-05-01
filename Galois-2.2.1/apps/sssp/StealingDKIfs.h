#ifdef STEALING_DK
if (wl == "smq_1_2_dk")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_2_dk>());
if (wl == "smq_1_4_dk")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_4_dk>());
if (wl == "smq_1_8_dk")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_8_dk>());
if (wl == "smq_1_16_dk")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_16_dk>());
if (wl == "smq_1_32_dk")
Galois::for_each_local(initial, Process(this, graph), Galois::wl<SMQ_1_32_dk>());
#endif
