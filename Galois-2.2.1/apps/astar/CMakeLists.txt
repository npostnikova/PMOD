set(CMAKE_CXX_STANDARD 17)
app(astar astar.cpp)
target_link_libraries(astar libatomic.so)

