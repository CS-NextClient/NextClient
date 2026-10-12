if (TARGET optic::optic)
    return()
endif()

include(FindPackageHandleStandardArgs)

###
### Find includes and libraries
###

find_path(OPTIC_INCLUDE_DIR
        NAMES optick.h
        PATHS ${NEXTCLIENT_ROOT}/dep/optic/include
        NO_DEFAULT_PATH
        NO_CACHE
)

if (WIN32)
    find_library(OPTIC_LIBRARY
            NAMES OptickCore.lib
            PATHS ${NEXTCLIENT_ROOT}/dep/optic/lib
            NO_DEFAULT_PATH
            NO_CACHE
    )

    find_package_handle_standard_args(optic REQUIRED_VARS OPTIC_LIBRARY OPTIC_INCLUDE_DIR)

    ###
    ### Setup library
    ###

    add_library(optic::optic STATIC IMPORTED GLOBAL)

    set_target_properties(optic::optic PROPERTIES
            IMPORTED_LOCATION "${OPTIC_LIBRARY}"
    )

    target_include_directories(optic::optic INTERFACE
            ${OPTIC_INCLUDE_DIR}
    )
else ()
    # Optick (OptickCore) is only vendored as Windows binaries here, and nothing on
    # Linux depends on real profiling data yet. optick.h's own macros (OPTICK_EVENT()
    # etc.) fully no-op when USE_OPTICK=0, so a header-only stub with that define is
    # enough to satisfy callers without needing a real Linux build of Optick.
    find_package_handle_standard_args(optic REQUIRED_VARS OPTIC_INCLUDE_DIR)

    add_library(optic::optic INTERFACE IMPORTED GLOBAL)

    target_include_directories(optic::optic INTERFACE
            ${OPTIC_INCLUDE_DIR}
    )

    target_compile_definitions(optic::optic INTERFACE
            USE_OPTICK=0
    )
endif ()
