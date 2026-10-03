if (MSVC)
    add_compile_options(
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )
endif()


if (WIN32)
    add_compile_definitions(
            _WIN32
            WIN32
            _WIN32_WINNT=0x0600
            WINVER=0x0600
            NOMINMAX
            _UNICODE
            WIN32_LEAN_AND_MEAN
    )
endif()

# GoldSrc is a 32-bit-only engine, so these static libraries must match it even though
# the build host is x86_64 (same convention as client_mini/gameui/steam_api_proxy).
if (UNIX)
    add_compile_options(-m32 -march=pentium4 -Wno-narrowing)
    add_link_options(-m32 -static-libstdc++ -static-libgcc)
    set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)
endif()

add_subdirectory(data_encoding)
add_subdirectory(data_types)
add_subdirectory(ncl_utils)
add_subdirectory(task_coro)
add_subdirectory(ncl_math)
add_subdirectory(gameplay)
add_subdirectory(hwid_collector)
