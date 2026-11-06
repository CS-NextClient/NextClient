add_library(nextclient_profiling INTERFACE)
add_library(NextClient::profiling ALIAS nextclient_profiling)
target_include_directories(nextclient_profiling INTERFACE
        "${NEXTCLIENT_ROOT}/nextclient/packages/ncl_perf/include")
target_compile_definitions(nextclient_profiling INTERFACE USE_PROFILER=$<BOOL:${USE_PROFILER}>)

if (USE_PROFILER)
    find_package(Tracy 0.14.1 EXACT CONFIG REQUIRED)
    target_link_libraries(nextclient_profiling INTERFACE Tracy::TracyClient)
    target_compile_definitions(nextclient_profiling INTERFACE
            TRACY_ENABLE TRACY_ON_DEMAND TRACY_NO_CRASH_HANDLER TRACY_MANUAL_LIFETIME)
endif ()
