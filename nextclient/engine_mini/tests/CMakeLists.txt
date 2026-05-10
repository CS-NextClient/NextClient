set(TARGET_NAME "engine_mini-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        #CmdCheckerBindTests.cpp
        CmdCheckerTests.cpp
        mocks/CmdCheckerConfigProviderMock.h
        mocks/LoggerMock.h
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        engine_mini
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEXTCLIENT_ROOT}/test_assets $<TARGET_FILE_DIR:${TARGET_NAME}>
)

gtest_discover_tests(${TARGET_NAME})
