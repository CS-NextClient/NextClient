set(TARGET_NAME "utils")

file(GLOB_RECURSE LAUNCHER_UTILS_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h")

add_library(${TARGET_NAME} STATIC ${LAUNCHER_UTILS_SOURCES})
add_library(next_launcher::utils ALIAS ${TARGET_NAME})

target_include_directories(${TARGET_NAME} PRIVATE ${NEXT_LAUNCHER_SOURCE_DIR}/include)
