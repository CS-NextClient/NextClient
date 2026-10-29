#pragma once

#include <cstddef>

#include <ncl_utils/safe_result.h>

namespace ncl_studio
{
    // What a loader keeps of a studio file. When the textures are embedded the loader
    // stops at the pixel data, so the header, the tables and the geometry survive while
    // everything from texturedataindex on does not.
    size_t CachedStudioSize(const void* data, size_t size);

    // Checks a whole .mdl buffer before anything is copied out of it.
    //
    // The invariant it establishes is the one every later reader depends on and none of
    // them can check for itself: every table the cached block will be asked for lies
    // inside the cached block, and every texture the loader will upload lies inside the
    // file. A model arrives over the network, so a count or an offset it declares is an
    // instruction to read somewhere, and the only place that knows how far the buffer
    // actually goes is here.
    ncl_utils::Result<> ValidateStudioFile(const void* data, size_t size);
} // namespace ncl_studio
