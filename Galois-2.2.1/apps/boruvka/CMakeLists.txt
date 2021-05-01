set(CMAKE_CXX_STANDARD 17)
app(boruvka Boruvka.cpp)
app(boruvka-merge BoruvkaMerge.cpp)
target_link_libraries(boruvka-merge libatomic.so)
