configure_file(${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher/version.h.in ${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher/version.h)

add_compile_definitions(
        _HAS_STATIC_RTTI=0
        # windows
        _WIN32
        WIN32
        _WIN32_WINNT=0x0600
        WINVER=0x0600
        NOMINMAX
        _UNICODE
        WIN32_LEAN_AND_MEAN
        #easyloggingpp options
        ELPP_WINSOCK2
        ELPP_THREAD_SAFE
        ELPP_DISABLE_DEFAULT_CRASH_HANDLING
        ELPP_DEFAULT_LOGGER="launcher"
        ELPP_DEFAULT_LOG_FILE="launcher.log"
)

add_compile_options(
        /GR- # disable rtti to reduce the library size
        /WX  # treat all warnings as error
        /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
)

if (CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(
            /Zi # enables PDB generation
            /O2 # enables maximize speed optimizations
    )

    add_link_options(
            /LTCG            # enables whole program optimization on link-time
            /INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
            /DEBUG           # enables PDB generation
            /OPT:REF         # eliminates functions and data that are never referenced
            /OPT:ICF         # enables COMDAT folding
    )
endif ()

add_subdirectory(utils)
add_subdirectory(gui_app_core)
add_subdirectory(updater)
add_subdirectory(next_launcher)
