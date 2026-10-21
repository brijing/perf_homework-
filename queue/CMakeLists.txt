cmake_minimum_required(VERSION 3.15)
set(CMAKE_CXX_STANDARD 20)
project(MyProject)

find_package(benchmark REQUIRED)



add_executable(scsp scsp.cpp)
target_link_libraries(scsp benchmark::benchmark)
