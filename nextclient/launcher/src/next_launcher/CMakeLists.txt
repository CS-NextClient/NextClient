set(TARGET_NAME "next_launcher")
set(OUTPUT_NAME "cstrike")

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

file(GLOB_RECURSE LAUNCHER_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h"
        "resources.rc"
        "app.manifest"
)

add_executable(${TARGET_NAME} WIN32
        ${LAUNCHER_SOURCES}
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include
)

target_link_libraries(${TARGET_NAME} PRIVATE
        nitro_api::nitro_api
        NextClient::client_mini
        NextClient::engine_mini
        NextClient::gameui
        NextClient::steam_api_proxy
        data_encoding::data_encoding
        ncl_utils::ncl_utils
        next_launcher::utils
        magic_enum::magic_enum
        easyloggingpp::easyloggingpp
        TaskCoro::TaskCoro
        cpr::cpr
        CURL::libcurl
        ZLIB::ZLIB
        Dbghelp.lib
)

target_compile_definitions(${TARGET_NAME} PRIVATE
        MAX_LOGFILE_SIZE=524288 #bytes
)

set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".exe"
        PDB_NAME ${OUTPUT_NAME}
)

if (NEXT_LAUNCHER_ENABLE_UPDATER)
    target_compile_definitions(${TARGET_NAME} PRIVATE
            UPDATER_ENABLE
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
            next_launcher::gui_app_core
            next_launcher::updater
            CURL::libcurl
            cpr::cpr
    )
endif ()

if (NEXT_LAUNCHER_ENABLE_SENTRY)
    find_package(ZLIB REQUIRED)
    find_package(sentry CONFIG REQUIRED)

    target_compile_definitions(${TARGET_NAME} PRIVATE
            SENTRY_ENABLE
            SENTRY_PROJECT_NAME="${NEXT_LAUNCHER_SENTRY_PROJECT_NAME}"
            SENTRY_UPLOAD_URL="${NEXT_LAUNCHER_SENTRY_UPLOAD_URL}"
            SENTRY_ENV="${NEXT_LAUNCHER_SENTRY_ENV}"
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
            sentry::sentry
            ZLIB::ZLIB
    )
endif ()

if (NEXT_LAUNCHER_ENABLE_GAMEANALYTICS)
    find_package(ZLIB REQUIRED)
    find_package(CURL COMPONENTS HTTP HTTPS REQUIRED)

    target_compile_definitions(${TARGET_NAME} PRIVATE
            GAMEANALYTICS_ENABLE
            GAMEANALYTICS_GAME_KEY="${NEXT_LAUNCHER_GAMEANALYTICS_GAME_KEY}"
            GAMEANALYTICS_SECRET_KEY="${NEXT_LAUNCHER_GAMEANALYTICS_SECRET_KEY}"
    )

    set_property(TARGET GameAnalytics PROPERTY CXX_STANDARD 17)
    set_property(TARGET Sqlite PROPERTY CXX_STANDARD 17)

    target_include_directories(${TARGET_NAME} PRIVATE ${NEXT_LAUNCHER_SOURCE_DIR}/dep/gs-sdk/source)

    target_link_libraries(${TARGET_NAME} PRIVATE
            CURL::libcurl
            ZLIB::ZLIB
            GameAnalytics
            Sqlite
    )
endif ()

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(next_launcher_export INTERFACE)
add_library(NextClient::next_launcher ALIAS next_launcher_export)

target_include_directories(next_launcher_export INTERFACE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher
)
