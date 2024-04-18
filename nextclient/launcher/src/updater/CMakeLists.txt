set(TARGET_NAME "updater")

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(taocpp-json CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(concurrencpp CONFIG REQUIRED)

configure_file(${NEXT_LAUNCHER_SOURCE_DIR}/src/updater/NextUpdater/NextUpdaterHttpServiceConfig.h.in ${NEXT_LAUNCHER_SOURCE_DIR}/src/updater/NextUpdater/NextUpdaterHttpServiceConfig.h)

file(GLOB_RECURSE UPDATER_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h")

add_library(${TARGET_NAME} STATIC ${UPDATER_SOURCES})
add_library(next_launcher::updater ALIAS ${TARGET_NAME})

target_include_directories(${TARGET_NAME} PRIVATE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include
        ${NEXT_LAUNCHER_SOURCE_DIR}/dep/easyloggingpp/src
)

target_link_libraries(${TARGET_NAME} PRIVATE
        nitro_api::nitro_api
        next_launcher::gui_app_core
        NextClient::engine_mini
        data_encoding::data_encoding
        magic_enum::magic_enum
        cpr::cpr
        CURL::libcurl
        ZLIB::ZLIB
        taocpp::json
        concurrencpp::concurrencpp
)
