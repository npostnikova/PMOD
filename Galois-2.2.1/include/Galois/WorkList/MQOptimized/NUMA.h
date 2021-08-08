// #define MQ_NUMA
#define SOCKET_SIZE 64
#define SOCKETS_NUM 2

#if SOCKETS_NUM==2

static const size_t OTHER_W = 1;
static const size_t socketSize = SOCKET_SIZE;
const size_t node1CntVal = node1Cnt();
const size_t node2CntVal = node2Cnt();

size_t node1Cnt() {
  size_t res = 0;
  if (nT > socketSize) {
    res += socketSize;
    if (socketSize * 2 < nT) {
      res += std::min(nT, socketSize * 3) - socketSize * 2;
    }
    return res;
  } else {
    return nT;
  }
}

size_t node2Cnt() {
  return nT - node1Cnt();
}

bool is1Node(size_t tId) {
  return tId < socketSize || (tId >= socketSize * 2 && tId < socketSize * 3);
}

bool is2Node(size_t tId) {
  return !is1Node(tId);
}

size_t socketIdByTID(size_t tId) {
  return is1Node(tId) ? 0 : 1; // tODO
}

size_t socketIdByQID(size_t qId) {
  if (qId < socketSize * C)
    return 0;
  if (qId < 2 * socketSize * C)
    return 1;
  if (qId < 3 * socketSize * C)
    return 0;
  return 1;
}


size_t map1Node(size_t qId) {
  if (qId < socketSize * C) {
    return qId;
  }
  return qId + socketSize * C;
}

size_t map2Node(size_t qId) {
  if (qId < socketSize * C) {
    return qId + socketSize * C;
  }
  return qId + socketSize * 2 * C;
}

inline size_t rand_heap() {
  static thread_local size_t tId = Galois::Runtime::LL::getTID();

  size_t isFirst = is1Node(tId);
  size_t localCnt = isFirst ? node1CntVal : node2CntVal;
  size_t otherCnt = nT - localCnt;
  const size_t Q = localCnt * LOCAL_NUMA_W * C + otherCnt * OTHER_W * C;
  const size_t r = random() % Q;
  if (r < localCnt * LOCAL_NUMA_W * C) {
    // we are stealing from our node
    auto qId = r / LOCAL_NUMA_W;
    return isFirst ? map1Node(qId) : map2Node(qId);
  } else {
    auto qId = (r - localCnt * LOCAL_NUMA_W * C) / OTHER_W;
    return isFirst ? map2Node(qId) : map1Node(qId);
  }
}

#endif
#if SOCKETS_NUM==4

#endif
