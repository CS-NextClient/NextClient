find_program(NEXTCLIENT_CARGO_EXECUTABLE cargo HINTS "$ENV{USERPROFILE}/.cargo/bin" REQUIRED)
get_filename_component(NEXTCLIENT_CARGO_DIR "${NEXTCLIENT_CARGO_EXECUTABLE}" DIRECTORY)
find_program(NEXTCLIENT_RUSTC_EXECUTABLE rustc HINTS "${NEXTCLIENT_CARGO_DIR}" REQUIRED)

set(NEXTCLIENT_LIFE_STATS_DLL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/life_stats.dll" PARENT_SCOPE)
set(NEXTCLIENT_LIFE_STATS_PROFILE "$<IF:$<CONFIG:Debug>,debug,release>")
set(NEXTCLIENT_LIFE_STATS_OUTPUT "${CMAKE_BINARY_DIR}/rust-life-stats/i686-pc-windows-msvc/${NEXTCLIENT_LIFE_STATS_PROFILE}")

# Cargo owns the Rust dependency graph; invoking it also picks up edits in the SDK.
add_custom_target(plugin_life_stats
        COMMAND ${CMAKE_COMMAND} -E env
            "RUSTC=${NEXTCLIENT_RUSTC_EXECUTABLE}"
            "NEXTCLIENT_WIN7_THUNKS=${NEXTCLIENT_ROOT}/dep/yy-thunks/x86/YY_Thunks_for_Win7.obj"
            "${NEXTCLIENT_CARGO_EXECUTABLE}" build --locked --target i686-pc-windows-msvc
            --target-dir "${CMAKE_BINARY_DIR}/rust-life-stats"
            --profile "$<IF:$<CONFIG:Debug>,dev,release>"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>" "${CMAKE_PDB_OUTPUT_DIRECTORY}/$<CONFIG>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NEXTCLIENT_LIFE_STATS_OUTPUT}/life_stats.dll" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/life_stats.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NEXTCLIENT_LIFE_STATS_OUTPUT}/life_stats.pdb" "${CMAKE_PDB_OUTPUT_DIRECTORY}/$<CONFIG>/life_stats.pdb"
        BYPRODUCTS "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/life_stats.dll" "${CMAKE_PDB_OUTPUT_DIRECTORY}/$<CONFIG>/life_stats.pdb"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)

