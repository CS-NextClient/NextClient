#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include <glad/glad.h>

namespace render
{
    // Linked programs kept between runs as the blobs the driver itself produced, so a
    // start-up that builds every program costs a few milliseconds instead of a GLSL
    // compile per program. A blob is only ever valid for the driver and the GPU that
    // wrote it, so the whole file is discarded whenever either changes, and an individual
    // blob the driver rejects falls back to a compile from source.

    uint64_t Cache_Hash(std::string_view text, uint64_t seed);

    uint64_t Cache_HashSeed();

    // The blob for the hash, or null on a miss. Points into the cache's own storage,
    // valid until the next Cache_Store.
    const std::vector<uint8_t>* Cache_Lookup(uint64_t hash, GLenum& format);

    // Held in memory until Cache_Save, so a start-up that builds several programs writes
    // the file once rather than once per program.
    void Cache_Store(uint64_t hash, GLenum format, std::vector<uint8_t> blob);

    void Cache_Save();

    // Drops the entries and the file. Bound to r_reload_shaders, which exists to build
    // programs from edited sources: a cache that answered first would defeat it.
    void Cache_Clear();
}
