# Refuses to install a tree in which an LFS-tracked asset is still its pointer file.
#
# Nothing in the build reads these images, so a checkout without LFS produces a green build
# and a release in which every mask is 130 bytes of text. Run with -DDIRECTORY=<path>.

if (NOT DEFINED DIRECTORY)
    message(FATAL_ERROR "CheckLfsPointers: DIRECTORY is not set")
endif ()

file(GLOB_RECURSE _files LIST_DIRECTORIES FALSE "${DIRECTORY}/*")

set(_pointers "")

foreach (_file IN LISTS _files)
    file(READ "${_file}" _head LIMIT 40)

    if (_head MATCHES "^version https://git-lfs")
        list(APPEND _pointers "${_file}")
    endif ()
endforeach ()

if (_pointers)
    string(REPLACE ";" "\n  " _list "${_pointers}")
    message(FATAL_ERROR
            "These files are Git LFS pointers rather than their content:\n  ${_list}\n"
            "Run \"git lfs install\" and \"git lfs pull\" before building.")
endif ()
