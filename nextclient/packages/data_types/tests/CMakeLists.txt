set(TARGET_NAME "data_types-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        MessageReaderTests.cpp
        MessageWriterTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        data_types::data_types
)

gtest_discover_tests(${TARGET_NAME})
