if (TARGET CURL::libcurl)
    return()
endif()

find_path(CURL_INCLUDE_DIR
        NAMES curl/curl.h
        PATHS ${NEXTCLIENT_ROOT}/dep/curl/include NO_DEFAULT_PATH)

find_library(CURL_LIBRARY
        NAMES libcurl_a.lib libcurl.a
        PATHS ${NEXTCLIENT_ROOT}/dep/curl/lib NO_DEFAULT_PATH)

find_library(CURL_LIBRARY_DEBUG
        NAMES libcurl_a_debug.lib libcurl.a
        PATHS ${NEXTCLIENT_ROOT}/dep/curl/lib NO_DEFAULT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CURL REQUIRED_VARS CURL_LIBRARY CURL_LIBRARY_DEBUG CURL_INCLUDE_DIR)

include(CMakeFindDependencyMacro)
find_dependency(ZLIB)

set(_supported_components HTTP HTTPS SSL FTP FTPS)
foreach(_comp ${curl_FIND_COMPONENTS})
    message(_comp)
    if (NOT ";${_supported_components};" MATCHES ";${_comp};")
        set(CURL_FOUND False)
        set(CURL_NOT_FOUND_MESSAGE "Unsupported component: ${_comp}")
    endif()
endforeach()

add_library(CURL::libcurl STATIC IMPORTED GLOBAL)
set_property(TARGET CURL::libcurl APPEND PROPERTY IMPORTED_CONFIGURATIONS Release)
set_property(TARGET CURL::libcurl APPEND PROPERTY IMPORTED_CONFIGURATIONS Debug)
set_target_properties(CURL::libcurl PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "CURL_STATICLIB"
    INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "C;RC"
    INTERFACE_LINK_LIBRARIES "${ZLIB_LIBRARY};Ws2_32.lib;Wldap32.lib;Crypt32.lib;Normaliz.lib"

    IMPORTED_LOCATION_RELEASE "${CURL_LIBRARY}"
    IMPORTED_LOCATION_DEBUG "${CURL_LIBRARY_DEBUG}"
)
