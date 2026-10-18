set(TARGET_NAME "settings_code-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        SettingsCodeTests.cpp
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        settings_code::settings_code
)

gtest_discover_tests(${TARGET_NAME})
