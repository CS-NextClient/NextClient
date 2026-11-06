find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(nitro_api-tests
        StudioAnimationBindingsTests.cpp
        StudioDirectCallTests.cpp
        StudioDispatchTests.cpp
        ../../client_mini/src/studio/StudioAnimation.cpp
        ../../client_mini/src/studio/studio_pose.cpp
        ../src/modules/client/studio_animation.cpp
        ../src/modules/client/ClientModule.cpp
        FrameLifecycleTests.cpp
        EngineFrameLifecycleTests.cpp
        ../src/modules/engine/EngineModule.cpp
        ../src/modules/engine/enginemsg_utils.cpp
        HostAbortAdapterTests.cpp
        ../src/modules/engine/HostAbortAdapter.cpp
        PresentationBoundaryTests.cpp
        ../src/modules/sdl2/loaded_sdl_module.cpp
        ../src/FrameLifecycle.cpp
        ../src/ChainExtensions.cpp
        ../src/AddressProviderBase.cpp
        ../src/SearchConfig.cpp
        ../src/modules/ModuleBase.cpp
        ../src/modules/sdl2/SDL2AddressProvider.cpp
        ../src/modules/sdl2/SDL2Module.cpp
        ../dep/easyloggingpp/src/easylogging++.cc
)
target_include_directories(nitro_api-tests PRIVATE
        ../include
        ../src
        ../dep/easyloggingpp/src
        ../../client_mini/src
        ../../client_mini/tests
)
target_link_libraries(nitro_api-tests PRIVATE
        GTest::gtest_main
        nitro_utils::nitro_utils
        magic_enum::magic_enum
        funchook-static
        xbyak::xbyak
        hl1_source_sdk::hl1_source_sdk
        NextClient::profiling
        ncl_math::ncl_math
        ncl_studio::ncl_studio
)
target_compile_options(nitro_api-tests PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/fp:precise>"
        "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-fno-fast-math;-ffp-contract=off>"
)
target_compile_definitions(nitro_api-tests PRIVATE ELPP_THREAD_SAFE ELPP_DISABLE_DEFAULT_CRASH_HANDLING NOMINMAX)

if (WIN32)
    add_custom_command(TARGET nitro_api-tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${NEXTCLIENT_ROOT}/test_support/runtime/tier0.dll" $<TARGET_FILE_DIR:nitro_api-tests>)
endif ()

gtest_discover_tests(nitro_api-tests)

add_library(performance-test-sdl SHARED FakeSdl.cpp)
add_library(performance-test-legacy-sdl SHARED FakeSdl.cpp)
target_compile_definitions(performance-test-legacy-sdl PRIVATE PERF_TEST_LEGACY_SDL)
add_dependencies(nitro_api-tests performance-test-legacy-sdl)
target_compile_definitions(nitro_api-tests PRIVATE PERF_TEST_LEGACY_SDL_PATH="$<TARGET_FILE:performance-test-legacy-sdl>")
set_target_properties(performance-test-sdl PROPERTIES OUTPUT_NAME sdl2)
add_library(performance-test-engine SHARED FakeSdlEngine.cpp)
target_link_libraries(performance-test-engine PRIVATE performance-test-sdl)
add_dependencies(nitro_api-tests performance-test-engine)
target_compile_definitions(nitro_api-tests PRIVATE PERF_TEST_ENGINE_PATH="$<TARGET_FILE:performance-test-engine>")

set_target_properties(nitro_api-tests performance-test-sdl performance-test-legacy-sdl performance-test-engine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)
