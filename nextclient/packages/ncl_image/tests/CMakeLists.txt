find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ncl_image-tests DecodeTests.cpp)
target_link_libraries(ncl_image-tests PRIVATE GTest::gtest_main ncl_image::ncl_image)
gtest_discover_tests(ncl_image-tests)
