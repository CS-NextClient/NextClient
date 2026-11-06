find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ncl_perf-tests CpuAccountingTests.cpp)
target_link_libraries(ncl_perf-tests PRIVATE GTest::gtest_main ncl_perf::ncl_perf)
gtest_discover_tests(ncl_perf-tests)
