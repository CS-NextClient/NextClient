if (TARGET SDL2::SDL2)
    return()
endif()

if (WIN32)
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
else ()
    # Headers come from the same vendored SDL 2.0.0 copy as on Windows, so nothing
    # newer than the engine's own SDL gets called. vcpkg's sdl2 (built shared, see
    # custom-vcpkg-triplets/x86-linux.cmake) is only linked for its
    # libSDL2-2.0.so.0 SONAME.
    find_path(SDL2_INCLUDE_DIR
            NAMES SDL.h
            PATHS ${NEXTCLIENT_ROOT}/dep/SDL2/include
            NO_DEFAULT_PATH
            NO_CACHE
    )

    find_library(SDL2_LIBRARY
            NAMES SDL2-2.0 SDL2
            NO_CACHE
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(SDL2 REQUIRED_VARS SDL2_LIBRARY SDL2_INCLUDE_DIR)

    add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)

    set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${SDL2_LIBRARY}"
    )

    target_include_directories(SDL2::SDL2 INTERFACE
            ${SDL2_INCLUDE_DIR}
    )

    # The vendored SDL_config.h was generated on Windows and typedefs the stdint
    # types itself unless told the system header exists
    target_compile_definitions(SDL2::SDL2 INTERFACE
            HAVE_STDINT_H=1
    )
endif ()
