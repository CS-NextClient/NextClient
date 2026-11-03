find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(ncl_math-tests trigonometry_tests.cpp vector_tests.cpp quaternion_matrix_tests.cpp)
target_compile_features(ncl_math-tests PRIVATE cxx_std_20)
target_link_libraries(ncl_math-tests PRIVATE ncl_math::ncl_math GTest::gtest_main)
target_compile_options(ncl_math-tests PRIVATE
    "$<$<CXX_COMPILER_ID:MSVC>:/fp:precise>"
    "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math;-ffp-contract=off>"
)
gtest_discover_tests(ncl_math-tests)
