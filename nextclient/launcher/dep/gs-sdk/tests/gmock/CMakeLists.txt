file(GLOB_RECURSE GTEST_SRCS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

message(${GTEST_SRCS})

add_library (gmock ${GTEST_SRCS})

target_include_directories (gmock PRIVATE .)
target_include_directories (gmock PUBLIC .)
