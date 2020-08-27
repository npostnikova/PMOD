#ifndef STEALINGx2
#define STEALINGx2
typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 4>, true> SMQ_1_4_x2;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 8>, true> SMQ_1_8_x2;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 16>, true> SMQ_1_16_x2;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 32>, true> SMQ_1_32_x2;

#endif
