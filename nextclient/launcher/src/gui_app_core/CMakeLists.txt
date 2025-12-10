set(TARGET_NAME "gui_app_core")

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(CMakeRC REQUIRED)

file(GLOB_RECURSE GUI_APP_CORE_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h"
        "${NEXT_LAUNCHER_SOURCE_DIR}/include/gui_app_core/imgui/*.cpp"
        "${NEXT_LAUNCHER_SOURCE_DIR}/include/gui_app_core/imgui/*.h"
)

add_library(${TARGET_NAME} STATIC ${GUI_APP_CORE_SOURCES})
add_library(next_launcher::gui_app_core ALIAS ${TARGET_NAME})

cmrc_add_resource_library(
        ${TARGET_NAME}_rc
        ALIAS ${TARGET_NAME}::rc

        assets/ArialMT.ttf
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include
)

target_link_libraries(${TARGET_NAME} PRIVATE
        OpenGL::GL
        glfw
        TaskCoro::TaskCoro
        ncl_utils::ncl_utils
        easyloggingpp::easyloggingpp
        ${TARGET_NAME}::rc
)
