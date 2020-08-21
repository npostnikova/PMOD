#ifndef STEALING
#define STEALING
typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 10>, true> SMQ_1_10;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 25>, true> SMQ_1_25;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 64>, true> SMQ_1_64;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 100>, true> SMQ_1_100;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 256>, true> SMQ_1_256;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 1000>, true> SMQ_1_1000;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 10000>, true> SMQ_1_10000;

#endif
