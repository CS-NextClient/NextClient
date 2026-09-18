add_library(goldsrc_abi INTERFACE)
add_library(goldsrc::abi ALIAS goldsrc_abi)

target_include_directories(goldsrc_abi INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(goldsrc_abi INTERFACE cxx_std_23)

# Compiled, never linked: the assertions are checked by the compiler.
add_library(goldsrc_abi_golden OBJECT tests/golden_layout.cpp)
target_link_libraries(goldsrc_abi_golden PRIVATE goldsrc::abi)

if (MSVC)
    target_compile_options(goldsrc_abi_golden PRIVATE /W4)
endif ()

# Temporary until stage 6: compares the new headers with the SDK ones under MSVC. It includes the old
# headers, whose own warnings are not ours to fix, so it is built without /W4 and without /WX.
add_library(goldsrc_abi_equivalence OBJECT tests/equivalence.cpp)
target_link_libraries(goldsrc_abi_equivalence PRIVATE goldsrc::abi hl1_source_sdk::hl1_source_sdk)
target_include_directories(goldsrc_abi_equivalence PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)
