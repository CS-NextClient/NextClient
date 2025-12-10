set(TARGET_NAME "updater-test")

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(taocpp-json CONFIG REQUIRED)
find_package(libuv CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

find_path(UWEBSOCKETS_INCLUDE_DIR NAMES uwebsockets/WebSocket.h REQUIRED)
find_path(USOCKETS_INCLUDE_DIR NAMES libusockets.h REQUIRED)
find_library(USOCKETS_LIBRARY NAMES uSockets REQUIRED)

include(GoogleTest)

file(GLOB_RECURSE UPDATER_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h"
        "${NEXT_LAUNCHER_SOURCE_DIR}/src/updater/*.cpp"
        "${NEXT_LAUNCHER_SOURCE_DIR}/src/updater/*.hpp"
        "${NEXT_LAUNCHER_SOURCE_DIR}/src/updater/*.h"
        "${NEXT_LAUNCHER_SOURCE_DIR}/dep/easyloggingpp/src/easylogging++.cc"
)

add_executable(${TARGET_NAME} ${UPDATER_SOURCES})

gtest_add_tests(${TARGET_NAME} ${UPDATER_SOURCES})

target_include_directories(${TARGET_NAME} PRIVATE
        ${NITRO_API_INCLUDE_PATHS}
        ${NEXT_LAUNCHER_SOURCE_DIR}/include
        ${NEXT_LAUNCHER_SOURCE_DIR}/src/updater
        ${NEXT_LAUNCHER_SOURCE_DIR}/dep/easyloggingpp/src
        # for tests
        ${GTEST_INCLUDE_DIR}
        ${LIBUV_INCLUDE_DIR}
        ${USOCKETS_INCLUDE_DIR}
        ${UWEBSOCKETS_INCLUDE_DIR}
)

target_link_libraries(${TARGET_NAME} PRIVATE
        nitro_utils
        magic_enum::magic_enum
        cpr::cpr
        CURL::libcurl
        ZLIB::ZLIB
        taocpp::json
        easyloggingpp::easyloggingpp
        next_launcher::gui_app_core
        data_encoding::data_encoding
        next_launcher::utils
        NextClient::engine_mini
        ncl_utils::ncl_utils
        # for tests
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        $<IF:$<TARGET_EXISTS:libuv::uv_a>,libuv::uv_a,libuv::uv>
        ${USOCKETS_LIBRARY}
)

target_compile_definitions(${TARGET_NAME} PRIVATE LAUNCHER_BUILD_TESTS)
