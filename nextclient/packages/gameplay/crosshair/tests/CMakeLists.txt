set(TARGET_NAME "crosshair-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        CrosshairTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        crosshair::crosshair
)

gtest_discover_tests(${TARGET_NAME})
