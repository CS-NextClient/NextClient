set(TARGET_NAME "gameui-tests")

find_package(GTest CONFIG REQUIRED)

include(GoogleTest)

add_executable(${TARGET_NAME}
        MaterialImageTests.cpp
        MaterialCatalogStateTests.cpp
        MaterialResourceCatalogTests.cpp
        MaterialSaveWorkflowTests.cpp
        MaterialTransitionControllerTests.cpp
        MaterialModelThumbnailTests.cpp
        ../src/GameUi/MaterialEditor/MaterialModelThumbnail.cpp
        ../src/GameUi/MaterialEditor/MaterialCatalogState.cpp
        ../src/GameUi/MaterialEditor/MaterialResourceCatalog.cpp
        ../src/GameUi/MaterialEditor/MaterialSaveWorkflow.cpp
        ../src/GameUi/MaterialEditor/MaterialTransitionController.cpp
        MaterialEditDialogStateTests.cpp
        MaterialPreviewStateTests.cpp
        MaterialPreviewPointerTests.cpp
        VideoSettingsTests.cpp
        ../../launcher/src/next_launcher/CommandLine.cpp
        ../src/GameUi/MaterialEditor/MaterialPreviewState.cpp
        ImageFileCacheTests.cpp
        ../src/GameUi/Controls/ImageFileCache.cpp
        ImageFileLayoutTests.cpp
        ../src/GameUi/Controls/ImageFileLayout.cpp
        ServerBrowserTextTests.cpp
        ServerFilterCountsTests.cpp
        ServerGameModeNamesTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ServerBrowser/ServerBrowserText.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/ServerBrowser/ServerFilterCounts.cpp
)

set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

target_compile_definitions(${TARGET_NAME} PRIVATE
        NEXTCLIENT_ASSETS_DIR="${NEXTCLIENT_ROOT}/assets"
)

if (WIN32)
        target_compile_definitions(${TARGET_NAME} PRIVATE
                _WIN32
                WIN32
                _WIN32_WINNT=0x0600
                WINVER=0x0600
                NOMINMAX
                _UNICODE
                WIN32_LEAN_AND_MEAN
        )
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        GTest::gtest
        GTest::gtest_main
        hl1_source_sdk::hl1_source_sdk
        nitro_utils::nitro_utils
        NextClient::engine_mini
        NextClient::next_launcher
        ncl_material::ncl_material
        ncl_math::ncl_math
        ncl_studio::ncl_studio
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${NEXTCLIENT_ROOT}/nextclient/launcher/src
)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${NEXTCLIENT_ROOT}/test_support/runtime $<TARGET_FILE_DIR:${TARGET_NAME}>
)

gtest_discover_tests(${TARGET_NAME})

add_executable(drag-value-label-tests
        DragValueLabelTests.cpp
        ../src/GameUi/Controls/DragValueLabel.cpp
)

set_target_properties(drag-value-label-tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

target_include_directories(drag-value-label-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/drag_value_label_fakes
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(drag-value-label-tests PRIVATE GTest::gtest GTest::gtest_main)

gtest_discover_tests(drag-value-label-tests)

add_executable(material-preview-panel-tests
        MaterialPreviewPanelTests.cpp
        ../src/GameUi/MaterialEditor/MaterialPreviewPanel.cpp
        ../src/GameUi/MaterialEditor/MaterialPreviewState.cpp
)

set_target_properties(material-preview-panel-tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
)

target_include_directories(material-preview-panel-tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/drag_value_label_fakes
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${NEXTCLIENT_ROOT}/nextclient/engine_mini/include
)

target_link_libraries(material-preview-panel-tests PRIVATE
        GTest::gtest GTest::gtest_main
        hl1_source_sdk::hl1_source_sdk
        ncl_math::ncl_math
)

gtest_discover_tests(material-preview-panel-tests)
