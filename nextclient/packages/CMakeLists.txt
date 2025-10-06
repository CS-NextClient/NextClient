add_compile_options(
        /WX  # treat all warnings as error
        /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
)

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(
            /Zi # enables PDB generation
            /O2 # enables maximize speed optimizations
            #/GL # enables whole program optimization (enables optimizations not within a single module (file), but across all modules)
    )

    add_link_options(
            #/LTCG            # enables whole program optimization on link-time
            #/INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
            /DEBUG           # enables PDB generation
            /OPT:REF         # eliminates functions and data that are never referenced
            /OPT:ICF         # enables COMDAT folding
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

add_subdirectory(data_encoding)
add_subdirectory(data_types)
add_subdirectory(ncl_utils)
add_subdirectory(task_coro)
