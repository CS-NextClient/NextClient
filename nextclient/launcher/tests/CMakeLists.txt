add_compile_definitions(
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
        ELPP_DEFAULT_LOGGER="updater-tests"
        ELPP_DEFAULT_LOG_FILE="updater-tests.log"
)

add_compile_options(
        /GR- # disable rtti to reduce the library size
        /WX  # treat all warnings as error
)

if (NEXT_LAUNCHER_ENABLE_UPDATER)
    add_subdirectory(updater)
endif ()
