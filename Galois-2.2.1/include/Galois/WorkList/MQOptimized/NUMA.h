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
static const size_t NEIGH_WEIGHT = 2;
static const size_t DIAG_WEIGHT = 1;
static const size_t LOCAL_WEIGHT = LOCAL_NUMA_W * 2;
static const size_t socketSize = SOCKET_SIZE;
const std::vector<size_t> nodesCnt = cntSockets();

size_t* cntSockets() {
  std::vector<size_t> result = { 0, 0, 0, 0 };
  size_t nTCnt = nT;
  for (size_t j = 0; j < 2; j++) {
    for (size_t i = 0; i < SOCKETS_NUM && nTCnt > 0; i++) {
      result[i] += std::min(nTCnt, SOCKET_SIZE);
      nTCnt -= std::min(nTCnt, SOCKET_SIZE);
    }
  }
  return result;
}


size_t socketIdByTID(size_t tId) {
  for (size_t j = 0; j < 2; j++) {
    for (size_t i = 0; i < SOCKETS_NUM; i++) {
      if (tId < SOCKET_SIZE) {
        return i;
      }
      tId -= SOCKET_SIZE;
    }
  }
  return -1;
}

size_t socketIdByQID(size_t qId) {
  for (size_t j = 0; j < 2; j++) {
    for (size_t i = 0; i < SOCKETS_NUM; i++) {
      if (qId < SOCKET_SIZE * C) {
        return i;
      }
      qId -= SOCKET_SIZE * C;
    }
  }
  return -1;
}


size_t mapQID(size_t socketId, size_t qId) {
  if (qId < SOCKET_SIZE * C) {
    return socketId * SOCKET_SIZE * C + qId;
  }
  return socketId * SOCKET_SIZE * C + qId + (SOCKETS_NUM - 1) * SOCKET_SIZE * C;
}

inline size_t rand_heap() {
  static thread_local size_t tId = Galois::Runtime::LL::getTID();

  size_t socketId = socketIdByTID(tId);
  size_t localCnt = nodesCnt[socketId];
  size_t neighId1 = (socketId + 1) % SOCKETS_NUM;
  size_t neighId2 = (socketId + SOCKETS_NUM - 1) % SOCKETS_NUM;
  size_t diagId = (socketId + 2) % SOCKETS_NUM;
  size_t neighCnt = nodesCnt[neighId1]
                  + nodesCnt[neighId2];
  size_t diagCnt = nT - localCnt - neighCnt;
  const size_t Q = localCnt * C * LOCAL_WEIGHT + neighCnt * C * NEIGH_WEIGHT + DIAG_WEIGHT * C * diagCnt;
  size_t r = random() % Q;
  if (r < localCnt * LOCAL_WEIGHT * C) {
    // we are stealing from our node
    auto qId = r / LOCAL_WEIGHT;
    return mapQID(socketId, qId);
  }
  r -= localCnt * LOCAL_WEIGHT * C;
  if (r < nodesCnt[neighId1] * NEIGH_WEIGHT * C) {
    auto qId = r / NEIGH_WEIGHT;
    return mapQID(neighId1, qId);
  }
  r -= nodesCnt[neighId1] * NEIGH_WEIGHT * C;
  if (r < nodesCnt[neighId2] * NEIGH_WEIGHT * C) {
    auto qId = r / NEIGH_WEIGHT;
    return mapQID(neighId2, qId);
  }
  r -= nodesCnt[neighId2] * NEIGH_WEIGHT * C;
  auto qId = r / DIAG_WEIGHT;
  return mapQID(diagId, qId);
}

#endif
