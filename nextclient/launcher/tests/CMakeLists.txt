add_compile_definitions(
        # windows
        _WIN32
        WIN32
        _WIN32_WINNT=0x0600
        WINVER=0x0600
        NOMINMAX
        _UNICODE
        #easyloggingpp options
        ELPP_THREAD_SAFE
        ELPP_DISABLE_DEFAULT_CRASH_HANDLING
        ELPP_DEFAULT_LOGGER="updater-tests"
        ELPP_DEFAULT_LOG_FILE="updater-tests.log"
)

add_compile_options(
        /GR- # disable rtti to reduce the library size
        /WX  # treat all warnings as error
)

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(
            /O2 # enables maximize speed optimizations
    )

    add_link_options(
            /LTCG            # enables whole program optimization on link-time
            /INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
            /OPT:REF         # eliminates functions and data that are never referenced
            /OPT:ICF         # enables COMDAT folding
    )
endif ()

if (NEXT_LAUNCHER_ENABLE_UPDATER)
    add_subdirectory(updater)
endif ()
