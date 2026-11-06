cmake_minimum_required(VERSION 3.21)
project(ncl_perf)

set(CMAKE_CXX_STANDARD 23)
add_library(ncl_perf STATIC src/PerfCpuAccounting.cpp)
add_library(ncl_perf::ncl_perf ALIAS ncl_perf)
target_include_directories(ncl_perf PUBLIC include)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
