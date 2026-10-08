configure_file(${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher/version.h.in ${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher/version.h)

add_compile_definitions(
        _HAS_STATIC_RTTI=0
        #easyloggingpp options
        ELPP_THREAD_SAFE
        ELPP_DISABLE_DEFAULT_CRASH_HANDLING
        ELPP_DEFAULT_LOG_FILE="launcher.log"
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
            ELPP_WINSOCK2
    )
endif ()

if (MSVC)
    add_compile_options(
            /GR- # disable rtti to reduce the library size
            /WX  # treat all warnings as error
            /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
    )
endif ()

add_subdirectory(utils)
add_subdirectory(next_launcher)

# gui_app_core/updater_gui_app are the self-updater's GUI (GLFW-based) - not
# ported to Linux, and only needed when NEXT_LAUNCHER_ENABLE_UPDATER is on
# (off by default), so they stay Windows-only for now.
if (WIN32)
    add_subdirectory(gui_app_core)
    add_subdirectory(updater_gui_app)
endif ()
