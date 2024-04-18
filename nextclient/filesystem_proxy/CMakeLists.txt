cmake_minimum_required(VERSION 3.21)
project(filesystem_proxy)
set(OUTPUT_NAME "FileSystem_Proxy")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 23)

# common options
set(BUILD_SHARED_LIBS OFF)

add_library(${PROJECT_NAME} SHARED
		src/FileSystem_Proxy.cpp
		src/FileSystem_Proxy.h
		src/IFileSystemNext.h
)

target_include_directories(${PROJECT_NAME} PRIVATE
		include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
		nitro_utils::nitro_utils
		hl1_source_sdk::hl1_source_sdk
)

if (MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE
			/GR- # disable rtti to reduce the library size
			/EHsc # NEVER catch SEH exceptions; c functions cannot throw exceptions
	)

	if (CMAKE_BUILD_TYPE MATCHES Release)
		target_compile_options(${PROJECT_NAME} PRIVATE
				/Zi # enables PDB generation
				/O2 # enables maximize speed optimizations
				/GL # enables whole program optimization (enables optimizations not within a single module (file), but across all modules)
		)

		target_link_options(${PROJECT_NAME} PRIVATE
				/LTCG            # enables whole program optimization on link-time
				/INCREMENTAL:NO  # /INCREMENTAL is not compatible with /LTCG
				/DEBUG           # enables PDB generation
				/OPT:REF         # eliminates functions and data that are never referenced
				/OPT:ICF         # enables COMDAT folding
		)
	endif()
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
		#SOURCE_SDK_NO_STEAM
		ERROR_TITLE="FileSystem_Proxy")

if (WIN32)
	target_compile_definitions(${PROJECT_NAME} PRIVATE
			_WIN32
			WIN32
			_WIN32_WINNT=0x0600
			WINVER=0x0600
			NOMINMAX
			_UNICODE
			WIN32_LEAN_AND_MEAN
	)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
		PREFIX ""
		OUTPUT_NAME ${OUTPUT_NAME}
		SUFFIX ".dll"
		PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(filesystem_proxy_export INTERFACE)
add_library(NextClient::filesystem_proxy ALIAS filesystem_proxy_export)

target_include_directories(filesystem_proxy_export INTERFACE
		include
)
