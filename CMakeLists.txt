cmake_minimum_required(VERSION 3.27)
project(practica_final)

set(CMAKE_CXX_STANDARD 17)

add_executable(bench_tiered
        src/DialsortVsTieredSort.cpp)

target_compile_options(bench_tiered PRIVATE -O3)

target_link_libraries(bench_tiered PRIVATE pthread)