if (TARGET ZLIB::ZLIB)
    return()
endif()

find_path(ZLIB_INCLUDE_DIR
        NAMES zlib.h
        PATHS ${NEXTCLIENT_ROOT}/dep/zlib/include NO_DEFAULT_PATH)

find_library(ZLIB_LIBRARY
        NAMES zlib_a.lib libz.a
        PATHS ${NEXTCLIENT_ROOT}/dep/zlib/lib NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR)

add_library(ZLIB::ZLIB STATIC IMPORTED GLOBAL)
set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${ZLIB_LIBRARY}"
)
