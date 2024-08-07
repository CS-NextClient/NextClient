CMAKE_MINIMUM_REQUIRED (VERSION 3.0)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")

PROJECT (GameAnalytics)

# useful Cmake macro for recursive search of files
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../jenkins/CMakeIncludes")
include("create_source_groups_macro")
include("eval_condition_macro")

message(STATUS ${CMAKE_MODULE_PATH})

set(GA_SOURCE_DIR "${PROJECT_SOURCE_DIR}/../../../source/gameanalytics")
set(DEPENDENCIES_DIR "${PROJECT_SOURCE_DIR}/../../../source/dependencies")

message(STATUS "********************** platform is ${PLATFORM}")

# osx platforms
EVAL_CONDITION(OSX_STATIC_SQLITE_SRC "${PLATFORM}" STREQUAL "osx-static")
EVAL_CONDITION(OSX_STATIC_NO_SQLITE_SRC "${PLATFORM}" STREQUAL "osx-static-no-sqlite-src")
EVAL_CONDITION(OSX_STATIC "${OSX_STATIC_SQLITE_SRC}" OR "${OSX_STATIC_NO_SQLITE_SRC}")
EVAL_CONDITION(OSX_SHARED "${PLATFORM}" STREQUAL "osx-shared")
EVAL_CONDITION(OSX_ALL "${OSX_STATIC}" OR "${OSX_SHARED}")

# windows platforms
# vc120
EVAL_CONDITION(WIN32_VC120_STATIC "${PLATFORM}" STREQUAL "win32-vc120-static")
EVAL_CONDITION(WIN32_VC120_MT_STATIC "${PLATFORM}" STREQUAL "win32-vc120-mt-static")
EVAL_CONDITION(WIN32_VC120_SHARED "${PLATFORM}" STREQUAL "win32-vc120-shared")
EVAL_CONDITION(WIN64_VC120_STATIC "${PLATFORM}" STREQUAL "win64-vc120-static")
EVAL_CONDITION(WIN64_VC120_MT_STATIC "${PLATFORM}" STREQUAL "win64-vc120-mt-static")
EVAL_CONDITION(WIN64_VC120_SHARED "${PLATFORM}" STREQUAL "win64-vc120-shared")

# vc140
EVAL_CONDITION(WIN32_VC140_STATIC "${PLATFORM}" STREQUAL "win32-vc140-static")
EVAL_CONDITION(WIN32_VC140_STATIC_NO_SQLITE_SRC "${PLATFORM}" STREQUAL "win32-vc140-static-no-sqlite-src")
EVAL_CONDITION(WIN32_VC140_MT_STATIC "${PLATFORM}" STREQUAL "win32-vc140-mt-static")
EVAL_CONDITION(WIN32_VC140_SHARED "${PLATFORM}" STREQUAL "win32-vc140-shared")
EVAL_CONDITION(WIN32_VC140_SHARED_NO_WMI "${PLATFORM}" STREQUAL "win32-vc140-shared-nowmi")
EVAL_CONDITION(WIN64_VC140_STATIC "${PLATFORM}" STREQUAL "win64-vc140-static")
EVAL_CONDITION(WIN64_VC140_STATIC_NO_SQLITE_SRC "${PLATFORM}" STREQUAL "win64-vc140-static-no-sqlite-src")
EVAL_CONDITION(WIN64_VC140_MT_STATIC "${PLATFORM}" STREQUAL "win64-vc140-mt-static")
EVAL_CONDITION(WIN64_VC140_SHARED "${PLATFORM}" STREQUAL "win64-vc140-shared")
EVAL_CONDITION(UWP_X86_VC140_STATIC "${PLATFORM}" STREQUAL "uwp-x86-vc140-static")
EVAL_CONDITION(UWP_X86_VC140_SHARED "${PLATFORM}" STREQUAL "uwp-x86-vc140-shared")
EVAL_CONDITION(UWP_X64_VC140_STATIC "${PLATFORM}" STREQUAL "uwp-x64-vc140-static")
EVAL_CONDITION(UWP_X64_VC140_SHARED "${PLATFORM}" STREQUAL "uwp-x64-vc140-shared")
EVAL_CONDITION(UWP_ARM_VC140_STATIC "${PLATFORM}" STREQUAL "uwp-arm-vc140-static")
EVAL_CONDITION(UWP_ARM_VC140_SHARED "${PLATFORM}" STREQUAL "uwp-arm-vc140-shared")

# vc141
EVAL_CONDITION(WIN32_VC141_STATIC "${PLATFORM}" STREQUAL "win32-vc141-static")
EVAL_CONDITION(WIN32_VC141_MT_STATIC "${PLATFORM}" STREQUAL "win32-vc141-mt-static")
EVAL_CONDITION(WIN32_VC141_SHARED "${PLATFORM}" STREQUAL "win32-vc141-shared")
EVAL_CONDITION(WIN64_VC141_STATIC "${PLATFORM}" STREQUAL "win64-vc141-static")
EVAL_CONDITION(WIN64_VC141_MT_STATIC "${PLATFORM}" STREQUAL "win64-vc141-mt-static")
EVAL_CONDITION(WIN64_VC141_SHARED "${PLATFORM}" STREQUAL "win64-vc141-shared")

EVAL_CONDITION(WIN_STATIC "${WIN32_VC120_MT_STATIC}" OR "${WIN64_VC120_MT_STATIC}" OR "${WIN32_VC120_STATIC}" OR "${WIN64_VC120_STATIC}" OR "${WIN32_VC140_STATIC}" OR "${WIN32_VC140_STATIC_NO_SQLITE_SRC}" OR "${WIN32_VC140_MT_STATIC}" OR "${WIN64_VC140_STATIC}" OR "${WIN64_VC140_STATIC_NO_SQLITE_SRC}" OR "${WIN64_VC140_MT_STATIC}" OR "${WIN32_VC141_STATIC}" OR "${WIN32_VC141_MT_STATIC}" OR "${WIN64_VC141_STATIC}" OR "${WIN64_VC141_MT_STATIC}")
EVAL_CONDITION(WIN_MT "${WIN32_VC120_MT_STATIC}" OR "${WIN64_VC120_MT_STATIC}" OR "${WIN32_VC140_MT_STATIC}" OR "${WIN64_VC140_MT_STATIC}" OR "${WIN32_VC141_MT_STATIC}" OR "${WIN64_VC141_MT_STATIC}")
EVAL_CONDITION(UWP_STATIC "${UWP_X86_VC140_STATIC}" OR "${UWP_X64_VC140_STATIC}" OR "${UWP_ARM_VC140_STATIC}")
EVAL_CONDITION(WIN_UWP_STATIC "${WIN_STATIC}" OR "${UWP_STATIC}")
EVAL_CONDITION(WIN_SHARED "${WIN32_VC120_SHARED}" OR "${WIN64_VC120_SHARED}" OR "${WIN32_VC140_SHARED}" OR "${WIN32_VC140_SHARED_NO_WMI}" OR "${WIN64_VC140_SHARED}" OR "${WIN32_VC141_SHARED}" OR "${WIN64_VC141_SHARED}")
EVAL_CONDITION(UWP_SHARED "${UWP_X86_VC140_SHARED}" OR "${UWP_X64_VC140_SHARED}" OR "${UWP_ARM_VC140_SHARED}")
EVAL_CONDITION(WIN_UWP_SHARED "${WIN_SHARED}" OR "${UWP_SHARED}")
EVAL_CONDITION(UWP_ALL "${UWP_STATIC}" OR "${UWP_SHARED}")
EVAL_CONDITION(WIN_ALL "${WIN_STATIC}" OR "${WIN_SHARED}")

# linux platforms
EVAL_CONDITION(LINUX_X86_CLANG_STATIC "${PLATFORM}" STREQUAL "linux-x86-clang-static")
EVAL_CONDITION(LINUX_X86_GCC_STATIC "${PLATFORM}" STREQUAL "linux-x86-gcc-static")
EVAL_CONDITION(LINUX_X86_CLANG_SHARED "${PLATFORM}" STREQUAL "linux-x86-clang-shared")
EVAL_CONDITION(LINUX_X86_GCC_SHARED "${PLATFORM}" STREQUAL "linux-x86-gcc-shared")
EVAL_CONDITION(LINUX_X64_CLANG_STATIC "${PLATFORM}" STREQUAL "linux-x64-clang-static")
EVAL_CONDITION(LINUX_X64_CLANG_STATIC_NO_SQLITE_SRC "${PLATFORM}" STREQUAL "linux-x64-clang-static-no-sqlite-src")
EVAL_CONDITION(LINUX_X64_GCC_STATIC "${PLATFORM}" STREQUAL "linux-x64-gcc-static")
EVAL_CONDITION(LINUX_X64_GCC5_STATIC "${PLATFORM}" STREQUAL "linux-x64-gcc5-static")
EVAL_CONDITION(LINUX_X64_CLANG_SHARED "${PLATFORM}" STREQUAL "linux-x64-clang-shared")
EVAL_CONDITION(LINUX_X64_GCC_SHARED "${PLATFORM}" STREQUAL "linux-x64-gcc-shared")
EVAL_CONDITION(LINUX_X64_GCC5_SHARED "${PLATFORM}" STREQUAL "linux-x64-gcc5-shared")

EVAL_CONDITION(LINUX_STATIC "${LINUX_X86_CLANG_STATIC}" OR "${LINUX_X86_GCC_STATIC}" OR "${LINUX_X64_CLANG_STATIC}" OR "${LINUX_X64_CLANG_STATIC_NO_SQLITE_SRC}" OR "${LINUX_X64_GCC_STATIC}" OR "${LINUX_X64_GCC5_STATIC}")
EVAL_CONDITION(LINUX_SHARED "${LINUX_X86_CLANG_SHARED}" OR "${LINUX_X86_GCC_SHARED}" OR "${LINUX_X64_CLANG_SHARED}" OR "${LINUX_X64_GCC_SHARED}" OR "${LINUX_X64_GCC5_SHARED}")
EVAL_CONDITION(LINUX_CLANG "${LINUX_X86_CLANG_STATIC}" OR "${LINUX_X64_CLANG_STATIC}" OR "${LINUX_X64_CLANG_STATIC_NO_SQLITE_SRC}" OR "${LINUX_X86_CLANG_SHARED}" OR "${LINUX_X64_CLANG_SHARED}")
EVAL_CONDITION(LINUX_ALL "${LINUX_STATIC}" OR "${LINUX_SHARED}")

# no sqlite src
EVAL_CONDITION(NO_SQLITE "${NO_SQLITE_SRC}" STREQUAL "YES")

message(STATUS "No sqlite src: ${NO_SQLITE}")

if("${WIN_MT}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
endif("${WIN_MT}")

if("${WIN_UWP_STATIC}")
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /MP" ) # Multiprocessor compilation
    add_definitions("-D_CRT_SECURE_NO_WARNINGS") #  warning C4996
    set(LIB_EXTENSION "lib")
    SET (LIB_TYPE STATIC)
elseif("${WIN_UWP_SHARED}")
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /MP" ) # Multiprocessor compilation
    include(GenerateExportHeader)
    add_definitions("-D_CRT_SECURE_NO_WARNINGS -DELPP_NO_DEFAULT_LOG_FILE") #  warning C4996
    SET (LIB_TYPE SHARED)
    add_definitions("-DGA_SHARED_LIB")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
elseif("${OSX_STATIC}")
    set(LIB_EXTENSION "a")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(EXTRA_LIBS z)
    SET (LIB_TYPE STATIC)
elseif("${OSX_SHARED}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -read_only_relocs suppress")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    add_definitions("-DELPP_NO_DEFAULT_LOG_FILE -DOSX")
    set(EXTRA_LIBS z)
    SET (LIB_TYPE SHARED)
    add_definitions("-DGA_SHARED_LIB")
elseif("${LINUX_STATIC}")
    set(LIB_EXTENSION "a")
    SET (LIB_TYPE STATIC)
    if("${LINUX_CLANG}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
    #add_definitions("-D_GLIBCXX_USE_CXX11_ABI=0")
elseif("${LINUX_SHARED}")
    set(LIB_EXTENSION "so")
    SET (LIB_TYPE SHARED)
    if("${LINUX_CLANG}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release")
    add_definitions("-DGA_SHARED_LIB")
    #add_definitions("-DGA_SHARED_LIB -D_GLIBCXX_USE_CXX11_ABI=0")
endif("${WIN_UWP_STATIC}")

if("${UWP_ALL}")
    add_definitions("-DUSE_UWP -ZW")
endif("${UWP_ALL}")

if("${WIN_ALL}")
    add_definitions("-DGUID_WINDOWS")
endif("${WIN_ALL}")

if("${OSX_ALL}")
    add_definitions("-DGUID_CFUUID")
endif("${OSX_ALL}")

if("${LINUX_ALL}")
    add_definitions("-DGUID_LIBUUID -DUSE_LINUX")
endif("${LINUX_ALL}")

message(STATUS "********************** DEPENDENCIES_DIR is ${DEPENDENCIES_DIR}")
add_definitions("-DUSE_OPENSSL -DCURL_STATICLIB")

if("${UWP_ALL}")
    add_definitions("-DSQLITE_OS_WINRT")

    FILE(GLOB_RECURSE CPP_SOURCES
        # Add GameAnalytics Sources
        "${GA_SOURCE_DIR}/*.cpp"
        "${GA_SOURCE_DIR}/*.h"

        # Add dependencies
        "${DEPENDENCIES_DIR}/crossguid/*.cpp"
        "${DEPENDENCIES_DIR}/sqlite/*.c"
        "${DEPENDENCIES_DIR}/sqlite/*.h"
        "${DEPENDENCIES_DIR}/miniz/*.c"
    )

    include_directories(
        "${DEPENDENCIES_DIR}/crossguid"
        "${DEPENDENCIES_DIR}/rapidjson"
        "${DEPENDENCIES_DIR}/sqlite"
        "${DEPENDENCIES_DIR}/miniz"
    )
elseif("${OSX_ALL}")
    # traverse all the subdirectories of the matched directory

    if("${NO_SQLITE}")
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.mm"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    else()
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.mm"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/sqlite/*.c"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    endif("${NO_SQLITE}")


    add_definitions("-DCRYPTOPP_DISABLE_ASM")

    include_directories(
        "${DEPENDENCIES_DIR}/crossguid"
        "${DEPENDENCIES_DIR}/rapidjson"
        "${DEPENDENCIES_DIR}/stacktrace"
        "${DEPENDENCIES_DIR}/zf_log"
        "${DEPENDENCIES_DIR}/sqlite"
        "${DEPENDENCIES_DIR}/crypto"
        "${DEPENDENCIES_DIR}/miniz"
        "${DEPENDENCIES_DIR}/curl/include"
    )
elseif("${LINUX_ALL}")
    # traverse all the subdirectories of the matched directory

    if("${NO_SQLITE}")
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    else()
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/sqlite/*.c"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    endif("${NO_SQLITE}")

    add_definitions("-DCRYPTOPP_DISABLE_ASM")

    include_directories(
        "${DEPENDENCIES_DIR}/crossguid"
        "${DEPENDENCIES_DIR}/rapidjson"
        "${DEPENDENCIES_DIR}/stacktrace"
        "${DEPENDENCIES_DIR}/zf_log"
        "${DEPENDENCIES_DIR}/sqlite"
        "${DEPENDENCIES_DIR}/crypto"
        "${DEPENDENCIES_DIR}/miniz"
        "${DEPENDENCIES_DIR}/curl/include"
    )
else()
    # traverse all the subdirectories of the matched directory

    if("${NO_SQLITE}")
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/stackwalker/*.cpp"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    else()
        FILE(GLOB_RECURSE CPP_SOURCES
            # Add GameAnalytics Sources
            "${GA_SOURCE_DIR}/*.cpp"
            "${GA_SOURCE_DIR}/*.h"

            # Add dependencies
            "${DEPENDENCIES_DIR}/crossguid/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.cpp"
            "${DEPENDENCIES_DIR}/stacktrace/*.hpp"
            "${DEPENDENCIES_DIR}/stackwalker/*.cpp"
            "${DEPENDENCIES_DIR}/sqlite/*.c"
            "${DEPENDENCIES_DIR}/sqlite/*.h"
            "${DEPENDENCIES_DIR}/crypto/*.c"
            "${DEPENDENCIES_DIR}/crypto/*.cpp"
            "${DEPENDENCIES_DIR}/zf_log/*.c"
            "${DEPENDENCIES_DIR}/zf_log/*.h"
            "${DEPENDENCIES_DIR}/miniz/*.c"
        )
    endif("${NO_SQLITE}")

    add_definitions("-DCRYPTOPP_DISABLE_ASM")

    include_directories(
        "${DEPENDENCIES_DIR}/crossguid"
        "${DEPENDENCIES_DIR}/rapidjson"
        "${DEPENDENCIES_DIR}/stacktrace"
        "${DEPENDENCIES_DIR}/stackwalker"
        "${DEPENDENCIES_DIR}/zf_log"
        "${DEPENDENCIES_DIR}/sqlite"
        "${DEPENDENCIES_DIR}/crypto"
        "${DEPENDENCIES_DIR}/miniz"
        "${DEPENDENCIES_DIR}/curl/include"
    )
endif("${UWP_ALL}")

create_source_groups( CPP_SOURCES )
if("${LINUX_ALL}")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif("${LINUX_ALL}")
ADD_LIBRARY (GameAnalytics ${LIB_TYPE} ${CPP_SOURCES})
#target_compile_features(GameAnalytics PRIVATE cxx_inline_namespaces)

#if("${WIN_UWP_STATIC}")
#    target_compile_options(GameAnalytics PRIVATE /Zi)
#    set_target_properties(GameAnalytics PROPERTIES
#        LINK_FLAGS "/INCREMENTAL:NO /DEBUG /OPT:REF /OPT:ICF"
#    )
#
#    set_target_properties(GameAnalytics PROPERTIES
#        COMPILE_PDB_NAME GameAnalytics
#        COMPILE_PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
#    )
#endif("${WIN_UWP_STATIC}")

if("${OSX_STATIC}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )
endif("${OSX_STATIC}")

if("${LINUX_ALL}")
    include_directories(
        "${DEPENDENCIES_DIR}/ooid"
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )
    set_target_properties(GameAnalytics PROPERTIES COMPILE_FLAGS "${TARGET_ARCH}" LINK_FLAGS "${TARGET_ARCH}")
endif("${LINUX_ALL}")

if("${OSX_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/osx/libcrypto.a
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/osx/libssl.a
        ${DEPENDENCIES_DIR}/curl/lib/osx/libcurl.a
        "-framework CoreFoundation"
        "-framework Foundation"
        "-framework CoreServices"
    )
endif("${OSX_SHARED}")

if("${LINUX_X64_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/linux_x64/libcrypto.a
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/linux_x64/libssl.a
        ${DEPENDENCIES_DIR}/curl/lib/linux_x64/libcurl.a
    )
endif("${LINUX_X64_SHARED}")

if("${LINUX_X86_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/linux_x86/libcrypto.a
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/linux_x86/libssl.a
        ${DEPENDENCIES_DIR}/curl/lib/linux_x86/libcurl.a
    )
endif("${LINUX_X86_SHARED}")

if("${WIN_STATIC}" OR "${UWP_ALL}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )
endif("${WIN_STATIC}" OR "${UWP_ALL}")

if("${WIN32_VC140_SHARED}" OR "${WIN32_VC140_SHARED_NO_WMI}" OR "${WIN32_VC141_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win32/vc140_x86_release/libeay32.lib
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win32/vc140_x86_release/ssleay32.lib
        ${DEPENDENCIES_DIR}/curl/lib/win32/vc140_x86_release/libcurl.lib
    )
endif("${WIN32_VC140_SHARED}" OR "${WIN32_VC140_SHARED_NO_WMI}" OR "${WIN32_VC141_SHARED}")

if("${WIN32_VC120_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win32/vc120_x86_release/libeay32.lib
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win32/vc120_x86_release/ssleay32.lib
        ${DEPENDENCIES_DIR}/curl/lib/win32/vc120_x86_release/libcurl.lib
    )
endif("${WIN32_VC120_SHARED}")

if("${UWP_ALL}")
    set_target_properties(GameAnalytics PROPERTIES VS_DESKTOP_EXTENSIONS_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
endif("${UWP_ALL}")

if("${WIN64_VC140_SHARED}" OR "${WIN64_VC141_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win64/vc140_x64_release/libeay32.lib
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win64/vc140_x64_release/ssleay32.lib
        ${DEPENDENCIES_DIR}/curl/lib/win64/vc140_x64_release/libcurl.lib
    )
endif("${WIN64_VC140_SHARED}" OR "${WIN64_VC141_SHARED}")

if("${WIN64_VC120_SHARED}")
    include_directories(
        "${DEPENDENCIES_DIR}/openssl/1.1.1d/include"
    )

    target_link_libraries(GameAnalytics
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win64/vc120_x64_release/libeay32.lib
        ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/win64/vc120_x64_release/ssleay32.lib
        ${DEPENDENCIES_DIR}/curl/lib/win64/vc120_x64_release/libcurl.lib
    )
endif("${WIN64_VC120_SHARED}")

target_include_directories(
    GameAnalytics
    PUBLIC
    "${GA_SOURCE_DIR}"
)

# enable c++11 via -std=c++11 when compiler is clang or gcc
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_features(GameAnalytics PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_features(GameAnalytics PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_features(GameAnalytics PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  # using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  # using Visual Studio C++
endif()
