CMAKE_MINIMUM_REQUIRED (VERSION 3.2)

# set name of project
PROJECT (GameAnalyticsTests)

set(DEPENDENCIES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../source/dependencies")

# add google mock and GameAnalytics CMakeLists.txt
add_subdirectory (gmock ${CMAKE_BINARY_DIR}/bin/gmock)
add_subdirectory (../build/cmake/gameanalytics/ ${CMAKE_BINARY_DIR}/bin/gameanalytics)

# building an executable file from the test source files
file(GLOB_RECURSE TEST_SOURCES RELATIVE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(
    ${PROJECT_NAME}
    ${TEST_SOURCES}
)

add_definitions("-DGUID_CFUUID")
set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++")

target_link_libraries(${PROJECT_NAME}
    ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/osx/libcrypto.a
    ${DEPENDENCIES_DIR}/openssl/1.1.1d/libs/osx/libssl.a
    ${DEPENDENCIES_DIR}/curl/lib/osx/libcurl.a
    "-framework CoreFoundation"
    "-framework Foundation"
    "-framework CoreServices"
)

# enable c++11 via -std=c++11 when compiler is clang or gcc
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_nonstatic_member_init)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
  target_link_libraries(${PROJECT_NAME} Rpcrt4.lib)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  target_link_libraries(${PROJECT_NAME} Rpcrt4.lib)
endif()

# include external dependency
target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../source/gameanalytics/"
    "${DEPENDENCIES_DIR}/rapidjson/"
    "${DEPENDENCIES_DIR}/zf_log"
    "${DEPENDENCIES_DIR}/openssl/1.1.1d/include/"
)

# include gmock and GameAnalytics library
# these 2 are defined (using add_library) in their CMakeLists.txt file
target_link_libraries(${PROJECT_NAME} gmock GameAnalytics)
