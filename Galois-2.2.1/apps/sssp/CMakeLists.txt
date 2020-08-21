set(CMAKE_CXX_STANDARD 17)
app(sssp-all SSSPall.cpp)
app(sssp SSSP.cpp)
target_link_libraries(sssp libatomic.so)
