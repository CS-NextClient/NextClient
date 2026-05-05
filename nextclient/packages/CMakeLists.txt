add_compile_options(
        /WX  # treat all warnings as error
        /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
)


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

add_subdirectory(data_encoding)
add_subdirectory(data_types)
add_subdirectory(ncl_utils)
add_subdirectory(task_coro)
