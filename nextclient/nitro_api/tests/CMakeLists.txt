find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(nitro_api-tests
        FrameLifecycleTests.cpp
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
target_include_directories(nitro_api-tests PRIVATE ../include ../src ../dep/easyloggingpp/src)
target_link_libraries(nitro_api-tests PRIVATE GTest::gtest_main nitro_utils::nitro_utils magic_enum::magic_enum
        funchook-static xbyak::xbyak hl1_source_sdk::hl1_source_sdk NextClient::profiling)
target_compile_definitions(nitro_api-tests PRIVATE ELPP_THREAD_SAFE ELPP_DISABLE_DEFAULT_CRASH_HANDLING NOMINMAX)
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
