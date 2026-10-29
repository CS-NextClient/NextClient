set(TARGET_NAME "engine_mini-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(renderer-lifecycle-tests
        RendererLifecycleTests.cpp
        ../../client_mini/tests/RadarGlContext.cpp)
target_include_directories(renderer-lifecycle-tests PRIVATE ../src ../include)
target_include_directories(renderer-lifecycle-tests PRIVATE ../../client_mini/tests)
target_link_libraries(renderer-lifecycle-tests PRIVATE GTest::gtest_main engine_mini)
if (WIN32)
    target_link_libraries(renderer-lifecycle-tests PRIVATE opengl32 gdi32 user32)
else ()
    target_link_libraries(renderer-lifecycle-tests PRIVATE ${CMAKE_DL_LIBS})
endif ()
add_custom_command(TARGET renderer-lifecycle-tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEXTCLIENT_ROOT}/test_support/runtime $<TARGET_FILE_DIR:renderer-lifecycle-tests>)
gtest_discover_tests(renderer-lifecycle-tests)

add_executable(${TARGET_NAME}
        #CmdCheckerBindTests.cpp

        ResourceDescriptorTests.cpp
        PreviewModelBridgeTests.cpp
        StudioModelCacheTests.cpp
        StudioModelTestFiles.cpp
        StudioPreviewContextTests.cpp
        CmdCheckerTests.cpp
        DiscordSessionTests.cpp
        DiscordValidationTests.cpp
        DiscordWorkerTests.cpp
        EngineMiniTestEnvironment.cpp
        GameModeRulesTests.cpp
        GeoIpCountryDatabaseTests.cpp
        HttpMasterClientTests.cpp
        HttpMasterResponseTests.cpp
        MasterDetailsByAddressTests.cpp
        MasterListCacheTests.cpp
        ServerCountryTests.cpp
        TextureProcessingTests.cpp
        WavefrontParserTests.cpp
        PerformanceMetricsTests.cpp
        PerformanceMonitorTests.cpp
        PerformanceSnapshotPublisherTests.cpp
        PerformanceBatchWindowTests.cpp
        PerformanceDumpCaptureTests.cpp
        PerformanceLifecycleTests.cpp
        PerformanceRuntimeTests.cpp
        GpuQueryCollectorTests.cpp
        RenderPerformanceTests.cpp
        StudioProgramContractTests.cpp
        ShaderIncludesTests.cpp
        RenderCaptureSessionTests.cpp
        StudioRendererOwnerTests.cpp
        StudioQueueTests.cpp
        StudioInterceptTests.cpp
        PerformanceMapperTests.cpp
        PerformanceNetworkTests.cpp
        PerformancePanelTests.cpp
        mocks/CmdCheckerConfigProviderMock.h
        mocks/LoggerMock.h
)

set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

if (WIN32)
    target_sources(${TARGET_NAME} PRIVATE TextSurfaceTests.cpp TextRenderTests.cpp)
endif ()

add_executable(performance-gl-tests
        StudioMaterialProviderTests.cpp
        RendererResourcesTestRoot.cpp
        ../src/graphics/render/RenderDevice.cpp
        ../src/graphics/render/ShaderManager.cpp
        ../src/graphics/render/material/MaterialManifestCatalog.cpp
        ../src/graphics/render/material/MaterialTextureCache.cpp
        ../src/graphics/render/material/StudioProgramContract.cpp
        StudioSubmissionGlTests.cpp
        StudioOptimizationsGlTests.cpp
        ../src/common/memory/g_scratch_allocator.cpp
        ../src/graphics/render/GpuBuffer.cpp
        ../src/graphics/render/StudioQueue.cpp
        ../src/graphics/render/StudioSubmit.cpp
        ../src/graphics/render/StudioMaterialRuntime.cpp
        PerformanceGpuGlTests.cpp
        PerformanceGlStateTests.cpp
        ../../client_mini/tests/RadarGlContext.cpp
        ../src/performance/GpuQueryBackend.cpp
        ../src/performance/GpuQueryCollector.cpp
        ../src/performance/PerformanceFrameHistory.cpp
        ../src/performance/PerformanceMonitor.cpp
        ../src/performance/PerformanceSnapshotPublisher.cpp
        ../src/performance/PerformanceBatchWindow.cpp
        ../src/performance/PerformanceDumpCapture.cpp
        ../src/performance/PerformanceNetwork.cpp
        ../src/performance/PerformancePanel.cpp
        ../src/performance/PerformancePanelLayout.cpp
        ../src/performance/PerformanceMapper.cpp
        ../src/performance/performance_scopes.cpp
        ../src/performance/performance_stages.cpp)
target_sources(performance-gl-tests PRIVATE ../src/graphics/render/render_device.cpp)
target_sources(performance-gl-tests PRIVATE ../src/performance/TracyGpuAdapter.cpp)
target_include_directories(performance-gl-tests PRIVATE
        ../src ../include ../../client_mini/tests)
target_link_libraries(performance-gl-tests PRIVATE
        GTest::gtest_main NextClient::profiling ncl_perf::ncl_perf glad::glad
        nitro_api::nitro_api hl1_source_sdk::hl1_source_sdk
        NextClient::filesystem_proxy NextClient::gameui
        TaskCoro::TaskCoro cpr::cpr ncl_gl::ncl_gl ncl_material::ncl_material ncl_math::ncl_math)
if (WIN32)
    target_compile_definitions(performance-gl-tests PRIVATE NOMINMAX)
    target_link_libraries(performance-gl-tests PRIVATE opengl32 gdi32 user32)
else ()
    target_link_libraries(performance-gl-tests PRIVATE ${CMAKE_DL_LIBS})
endif ()
if (UNIX)
    gtest_discover_tests(performance-gl-tests PROPERTIES LABELS gl ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
else ()
    gtest_discover_tests(performance-gl-tests PROPERTIES LABELS gl)
endif ()

target_compile_definitions(${TARGET_NAME} PRIVATE
        NEXTCLIENT_ASSETS_DIR="${NEXTCLIENT_ROOT}/assets"
        NOMINMAX
)

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        GTest::gmock_main
        engine_mini
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../nitro_api/src
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEXTCLIENT_ROOT}/test_support/runtime $<TARGET_FILE_DIR:${TARGET_NAME}>
)

gtest_discover_tests(${TARGET_NAME})

add_executable(studio-capture-gl-tests
        StudioCaptureGlTests.cpp
        StudioModelTestFiles.cpp
        StudioMaterialRuntimeGlTests.cpp
        MaterialCoverageGlTests.cpp
        StudioEffectsGlTests.cpp
        EngineMiniTestEnvironment.cpp
        ../../client_mini/tests/RadarGlContext.cpp)
target_include_directories(studio-capture-gl-tests PRIVATE ../src ../include ../../client_mini/tests ../../nitro_api/src)
target_link_libraries(studio-capture-gl-tests PRIVATE GTest::gtest_main engine_mini)
if (WIN32)
    target_compile_definitions(studio-capture-gl-tests PRIVATE NOMINMAX)
    target_link_libraries(studio-capture-gl-tests PRIVATE opengl32 gdi32 user32)
else ()
    target_link_libraries(studio-capture-gl-tests PRIVATE ${CMAKE_DL_LIBS})
endif ()
gtest_discover_tests(studio-capture-gl-tests PROPERTIES LABELS gl)

if (USE_PROFILER)
    foreach (probe IN ITEMS performance-probe-a performance-probe-b)
        add_library(${probe} SHARED profiling/RuntimeProbe.cpp)
        target_link_libraries(${probe} PRIVATE NextClient::profiling)
    endforeach ()
    add_executable(diagnostic-profiling-tests
            DiagnosticProfilingTests.cpp
            DiagnosticStreamingTests.cpp
            ../src/performance/TracyGpuAdapter.cpp
            ../src/performance/performance_stages.cpp)
    target_include_directories(diagnostic-profiling-tests PRIVATE ../src ../include)
    target_link_libraries(diagnostic-profiling-tests PRIVATE GTest::gtest_main NextClient::profiling nitro_utils::nitro_utils glad::glad ncl_perf::ncl_perf)
    add_dependencies(diagnostic-profiling-tests performance-probe-a performance-probe-b)
    target_compile_definitions(diagnostic-profiling-tests PRIVATE
            PERF_PROBE_A_PATH="$<TARGET_FILE:performance-probe-a>"
            PERF_PROBE_B_PATH="$<TARGET_FILE:performance-probe-b>")
    if (WIN32)
        target_link_libraries(diagnostic-profiling-tests PRIVATE ws2_32)
    endif ()
    gtest_discover_tests(diagnostic-profiling-tests PROPERTIES TIMEOUT 30)
    add_executable(profiling-connection-tests
            ProfilingConnectionTests.cpp
            ../src/performance/performance_stages.cpp)
    target_include_directories(profiling-connection-tests PRIVATE ../src ../include)
    target_link_libraries(profiling-connection-tests PRIVATE
            GTest::gtest_main NextClient::profiling glad::glad ncl_perf::ncl_perf)
    gtest_discover_tests(profiling-connection-tests)
endif ()

add_executable(material-lifecycle-tests
        MaterialLifecycleTests.cpp
        MaterialDependencyTests.cpp
        ClientStringRegistryIntegrationTests.cpp
        ../src/client/cl_string_registry.cpp
        ../src/graphics/render/material/material_edit.cpp
        ../src/graphics/render/material/MaterialEditor.cpp
        ../src/client/ClientStringRegistry.cpp
        ../src/graphics/render/material/MaterialDocumentStore.cpp
        ../src/graphics/render/material/MaterialGameFiles.cpp
        ../src/graphics/render/render_context.cpp
        ../src/graphics/render/RenderDevice.cpp
        ../src/graphics/render/ShaderManager.cpp
        ../src/graphics/render/material/MaterialManifestCatalog.cpp
        ../src/graphics/render/material/MaterialTextureCache.cpp
        ../src/graphics/render/model/StudioModelCache.cpp
        ../src/graphics/render/model/StudioModelMaterials.cpp
        ../src/graphics/render/model/StudioGeometry.cpp
        ../src/graphics/render/GpuBuffer.cpp
        ../src/graphics/render/material/server_materials_integration.cpp
        ../src/client/ncl_entity/MaterialSyncSystem.cpp
        ../src/graphics/render/material/ServerMaterials.cpp
        ../src/graphics/render/material/StudioProgramContract.cpp
        ../src/graphics/render/RenderCaptureSession.cpp
        ../src/graphics/render/render_file_name.cpp)
target_sources(material-lifecycle-tests PRIVATE
        ../src/performance/PerformanceMonitor.cpp
        ../src/performance/PerformanceMapper.cpp
        ../src/performance/PerformanceFrameHistory.cpp
        ../src/performance/PerformanceSnapshotPublisher.cpp
        ../src/performance/PerformanceBatchWindow.cpp
        ../src/performance/PerformanceDumpCapture.cpp
        ../src/performance/GpuQueryCollector.cpp
        ../src/performance/performance_stages.cpp)
target_include_directories(material-lifecycle-tests PRIVATE ../src ../include ../../../dep/nextclientapi_amxx/include)
get_target_property(ENGINE_MINI_TEST_LIBRARIES engine_mini LINK_LIBRARIES)
target_link_libraries(material-lifecycle-tests PRIVATE GTest::gtest_main ${ENGINE_MINI_TEST_LIBRARIES})
target_compile_definitions(material-lifecycle-tests PRIVATE NOMINMAX
        NEXTCLIENT_STUDIO_MANIFEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/studio.json")
gtest_discover_tests(material-lifecycle-tests)

add_executable(performance-history-tests
        FrameHistoryTests.cpp
        ../src/performance/PerformanceFrameHistory.cpp)
target_include_directories(performance-history-tests PRIVATE ../src ../include)
target_link_libraries(performance-history-tests PRIVATE GTest::gtest_main ncl_perf::ncl_perf)
gtest_discover_tests(performance-history-tests)

add_executable(preview-model-tests
        PreviewModelRepositoryTests.cpp
        StudioModelTestFiles.cpp
        EngineMiniTestEnvironment.cpp)
target_include_directories(preview-model-tests PRIVATE ../src ../include)
target_link_libraries(preview-model-tests PRIVATE GTest::gtest_main engine_mini)
gtest_discover_tests(preview-model-tests)
add_executable(scene-preview-gl-tests
        ScenePreviewGlTests.cpp
        PreviewInteractionGlTests.cpp
        SceneAuthoringCloseGlTests.cpp
        StudioModelTestFiles.cpp
        EngineMiniTestEnvironment.cpp
        ../../client_mini/tests/RadarGlContext.cpp)
target_include_directories(scene-preview-gl-tests PRIVATE ../src ../include ../../client_mini/tests)
target_link_libraries(scene-preview-gl-tests PRIVATE GTest::gtest_main engine_mini)
if (WIN32)
    target_compile_definitions(scene-preview-gl-tests PRIVATE NOMINMAX)
    target_link_libraries(scene-preview-gl-tests PRIVATE opengl32 gdi32 user32)
else ()
    target_link_libraries(scene-preview-gl-tests PRIVATE ${CMAKE_DL_LIBS})
endif ()
gtest_discover_tests(scene-preview-gl-tests PROPERTIES LABELS gl)
add_executable(ncl-entity-message-tests
        NclEntityMessageTests.cpp
        ../src/client/ncl_entity/ncl_entity_message.cpp)
target_include_directories(ncl-entity-message-tests PRIVATE ../src ../../../dep/nextclientapi_amxx/include)
target_link_libraries(ncl-entity-message-tests PRIVATE GTest::gtest_main data_types::data_types unordered_dense::unordered_dense)
gtest_discover_tests(ncl-entity-message-tests)
