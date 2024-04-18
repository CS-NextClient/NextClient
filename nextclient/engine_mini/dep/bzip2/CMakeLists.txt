project(bzip2)

add_library(${PROJECT_NAME} STATIC
        blocksort.c
        bzlib.c
        bzlib.h
        bzlib_private.h
        compress.c
        crctable.c
        decompress.c
        huffman.c
        randtable.c)
