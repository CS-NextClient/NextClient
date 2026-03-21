if (TARGET SDL2::SDL2)
    return()
endif()

include(FindPackageHandleStandardArgs)

###
### Find includes and libraries
###

find_path(SDL2_INCLUDE_DIR
        NAMES SDL.h
        PATHS ${NEXTCLIENT_ROOT}/dep/SDL2/include
        NO_DEFAULT_PATH
        NO_CACHE
)

find_library(SDL2_LIBRARY
        NAMES SDL2.lib
        PATHS ${NEXTCLIENT_ROOT}/dep/SDL2/lib
        NO_DEFAULT_PATH
        NO_CACHE
)

find_package_handle_standard_args(SDL2 REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR)

###
### Setup library
###

add_library(SDL2::SDL2 STATIC IMPORTED GLOBAL)

set_target_properties(SDL2::SDL2 PROPERTIES
        IMPORTED_LOCATION "${SDL2_LIBRARY}"
)

target_include_directories(SDL2::SDL2 INTERFACE
        ${SDL2_INCLUDE_DIR}
)
