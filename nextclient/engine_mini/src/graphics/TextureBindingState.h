#pragma once

#include <array>

struct TextureBindingCacheState
{
    unsigned int target{};
    int current{};
    std::array<int, 2> bindings{};
    bool initialized{};
};

TextureBindingCacheState GL_CaptureTextureCache();
void GL_RestoreTextureCache(const TextureBindingCacheState& state);
