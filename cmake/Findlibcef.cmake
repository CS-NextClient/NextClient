#
# Finds libcef
#
# On Windows this is the real CEF (Chromium Embedded Framework) shared library that
# backs GameUI's HTML panels. It isn't ported to Linux yet, so on non-Windows this
# provides a header-only stub target instead: enough for hl1_source_sdk's
# libcef_dll wrapper to compile and link against, with no actual browser behind it.
#
if (WIN32)
    find_library( LIBCEF_LIB libcef PATHS ${SOURCE_SDK_ROOT}/lib/public NO_DEFAULT_PATH )

    include( FindPackageHandleStandardArgs )
    find_package_handle_standard_args( libcef DEFAULT_MSG LIBCEF_LIB )

    if( LIBCEF_LIB )
        add_library( libcef SHARED IMPORTED GLOBAL )

        if( MSVC )
            set_property( TARGET libcef PROPERTY IMPORTED_IMPLIB ${LIBCEF_LIB} )
        else()
            set_property( TARGET libcef PROPERTY IMPORTED_LOCATION ${LIBCEF_LIB} )
        endif()
    endif()

    unset( LIBCEF_LIB CACHE )
else ()
    include( FindPackageHandleStandardArgs )
    find_package_handle_standard_args( libcef DEFAULT_MSG SOURCE_SDK_ROOT )

    add_library( libcef INTERFACE IMPORTED GLOBAL )
endif ()
