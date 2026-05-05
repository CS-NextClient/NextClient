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
        ELPP_DEFAULT_LOG_FILE="launcher.log"
)

add_compile_options(
        /GR- # disable rtti to reduce the library size
        /WX  # treat all warnings as error
        /EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
)

add_subdirectory(utils)
add_subdirectory(gui_app_core)
add_subdirectory(updater_gui_app)
add_subdirectory(next_launcher)
