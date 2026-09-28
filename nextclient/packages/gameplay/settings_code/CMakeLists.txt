cmake_minimum_required(VERSION 3.21)
project(settings_code)

set(CMAKE_CXX_STANDARD 23)

add_library(${PROJECT_NAME} STATIC
        src/settings_code.cpp
)
add_library(settings_code::settings_code ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC include)

target_link_libraries(${PROJECT_NAME}
        PUBLIC cvars::cvars
        PRIVATE data_encoding::data_encoding
)

if (NEXTCLIENT_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
