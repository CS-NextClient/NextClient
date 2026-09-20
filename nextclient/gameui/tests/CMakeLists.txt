set(TARGET_NAME "gameui-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        ServerBrowserTextTests.cpp
        ServerFilterCountsTests.cpp
        ServerGameModeNamesTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ServerBrowser/ServerBrowserText.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ServerBrowser/ServerFilterCounts.cpp
)

target_compile_definitions(${TARGET_NAME} PRIVATE
        _WIN32
        WIN32
        _WIN32_WINNT=0x0600
        WINVER=0x0600
        NOMINMAX
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NEXTCLIENT_ASSETS_DIR="${NEXTCLIENT_ROOT}/assets"
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        hl1_source_sdk::hl1_source_sdk
        nitro_utils::nitro_utils
        NextClient::engine_mini
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEXTCLIENT_ROOT}/test_assets $<TARGET_FILE_DIR:${TARGET_NAME}>
)

gtest_discover_tests(${TARGET_NAME})
