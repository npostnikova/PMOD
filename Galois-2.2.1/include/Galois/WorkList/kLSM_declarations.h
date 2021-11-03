// Declaration of kLSM worklist with different parameters to be used in applications.
#ifndef GALOIS_KLSM_DECLARATIONS_H
#define GALOIS_KLSM_DECLARATIONS_H

typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 1>> kLSM_1;
if (wl == "kLSM_1") RUN_WL(kLSM_1);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 2>> kLSM_2;
if (wl == "kLSM_2") RUN_WL(kLSM_2);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 4>> kLSM_4;
if (wl == "kLSM_4") RUN_WL(kLSM_4);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 8>> kLSM_8;
if (wl == "kLSM_8") RUN_WL(kLSM_8);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 16>> kLSM_16;
if (wl == "kLSM_16") RUN_WL(kLSM_16);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 32>> kLSM_32;
if (wl == "kLSM_32") RUN_WL(kLSM_32);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 64>> kLSM_64;
if (wl == "kLSM_64") RUN_WL(kLSM_64);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 128>> kLSM_128;
if (wl == "kLSM_128") RUN_WL(kLSM_128);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 256>> kLSM_256;
if (wl == "kLSM_256") RUN_WL(kLSM_256);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 512>> kLSM_512;
if (wl == "kLSM_512") RUN_WL(kLSM_512);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 1024>> kLSM_1024;
if (wl == "kLSM_1024") RUN_WL(kLSM_1024);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 2048>> kLSM_2048;
if (wl == "kLSM_2048") RUN_WL(kLSM_2048);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 4096>> kLSM_4096;
if (wl == "kLSM_4096") RUN_WL(kLSM_4096);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 16384>> kLSM_16384;
if (wl == "kLSM_16384") RUN_WL(kLSM_16384);
typedef GlobPQ<element_t, kLSMQ<element_t, Indexer, 4194304>> kLSM_4194304;
if (wl == "kLSM_4194304") RUN_WL(kLSM_4194304);

#endif //GALOIS_KLSM_DECLARATIONS_H
