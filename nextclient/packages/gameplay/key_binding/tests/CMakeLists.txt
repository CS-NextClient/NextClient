set(TARGET_NAME "key-binding-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        KeyBindingTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        key_binding::key_binding
)

gtest_discover_tests(${TARGET_NAME})
