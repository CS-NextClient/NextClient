cmake_minimum_required(VERSION 3.21)
project(matgen)

find_package(ZLIB REQUIRED)
find_package(imageinfo CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME}
        src/BakeEdges.cpp
        src/Check.cpp
        src/main.cpp
        src/Manifests.cpp
        src/Model.cpp
        src/Png.cpp
        src/UvTemplate.cpp
)

# The same manifest files the engine module embeds, from the same place. A copy that had
# drifted would make this tool answer differently from the client it exists to predict.
cmrc_add_resource_library(
        ${PROJECT_NAME}_rc
        ALIAS ${PROJECT_NAME}::rc
        WHENCE ${NEXTCLIENT_ROOT}/nextclient/engine_mini

        ${NEXTCLIENT_ROOT}/nextclient/engine_mini/assets/shaders/weapon_finish.json
)

# Consumes the packages from outside the engine exactly as any other user of them would,
# which is also the check: if this is awkward, a package is holding onto something it
# should not.
target_link_libraries(${PROJECT_NAME} PRIVATE
        ncl_material::ncl_material
        ncl_math::ncl_math
        ncl_studio::ncl_studio
        # The header the client's own upload path parses images with, so that this tool
        # accepts exactly the formats the client decodes rather than everything a decoder
        # happens to recognise.
        imageinfo::imageinfo
        stb::stb
        ZLIB::ZLIB
        ${PROJECT_NAME}::rc
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
)
