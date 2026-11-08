find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_library(nitro-utils-test-module SHARED test_module.cpp)
set_target_properties(nitro-utils-test-module PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/CS 1.6 NextClient")

add_library(nitro-utils-test-long-path-module SHARED test_module.cpp)
string(REPEAT "a" 200 long_component)
set_target_properties(nitro-utils-test-long-path-module PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/CS 1.6 NextClient/${long_component}/${long_component}/${long_component}")

add_executable(nitro_utils-tests LinuxModulePathTests.cpp)
target_link_libraries(nitro_utils-tests PRIVATE
        GTest::gtest_main
        nitro_utils::nitro_utils
        ${CMAKE_DL_LIBS})
target_compile_definitions(nitro_utils-tests PRIVATE
        NITRO_UTILS_TEST_MODULE_PATH="$<TARGET_FILE:nitro-utils-test-module>"
        NITRO_UTILS_TEST_LONG_MODULE_PATH="$<TARGET_FILE:nitro-utils-test-long-path-module>")
add_dependencies(nitro_utils-tests nitro-utils-test-module nitro-utils-test-long-path-module)

gtest_discover_tests(nitro_utils-tests NO_PRETTY_VALUES)
