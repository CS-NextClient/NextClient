set(TARGET_NAME "view-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        ViewBobReferenceTests.cpp
        ViewBobTests.cpp
        ViewFovTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        view::view
)

gtest_discover_tests(${TARGET_NAME})
