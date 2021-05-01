#ifndef STEALING_DK
#define STEALING_DK
typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 2>, true, true, DecreaseKeyIndexer<UpdateRequest>> SMQ_1_2_dk;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 4>, true, true, DecreaseKeyIndexer<UpdateRequest>> SMQ_1_4_dk;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 8>, true, true, DecreaseKeyIndexer<UpdateRequest>> SMQ_1_8_dk;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 16>, true, true, DecreaseKeyIndexer<UpdateRequest>> SMQ_1_16_dk;

typedef StealingMultiQueue<UpdateRequest, Comparer, Prob<1, 32>, true, true, DecreaseKeyIndexer<UpdateRequest>> SMQ_1_32_dk;

#endif
