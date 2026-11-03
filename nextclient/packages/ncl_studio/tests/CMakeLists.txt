find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ncl_studio-tests StudioBoneAnimationTests.cpp)
target_compile_features(ncl_studio-tests PRIVATE cxx_std_20)
target_link_libraries(ncl_studio-tests PRIVATE ncl_studio::ncl_studio GTest::gtest_main)
gtest_discover_tests(ncl_studio-tests)
