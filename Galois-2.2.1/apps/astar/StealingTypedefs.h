#ifndef STEALING
#define STEALING
typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 4>, true> SMQ_1_4;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 8>, true> SMQ_1_8;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 16>, true> SMQ_1_16;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 32>, true> SMQ_1_32;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 64>, true> SMQ_1_64;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 128>, true> SMQ_1_128;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 256>, true> SMQ_1_256;

#endif
