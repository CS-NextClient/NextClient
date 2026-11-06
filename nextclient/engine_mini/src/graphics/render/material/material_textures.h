#pragma once

namespace render
{
    // GL texture name for a material's texture parameter, loaded on first use and kept
    // for the session. Returns 0 when the file is missing or cannot be decoded; a failure
    // is remembered so a missing file is reported once rather than retried every frame.
    unsigned int MaterialTexture_Get(const char* path);

    // A 1x1 opaque white texture, for a sampler a program declares and a material does not
    // fill. Such a sampler otherwise keeps its default of unit 0, which is the model's own
    // skin, so a missing image reads as the skin standing in for the layer instead of the
    // layer being absent. White is the neutral value for the multiply and threshold uses
    // the shipped program puts its optional maps to.
    unsigned int MaterialTexture_Neutral();

    // Bound to map load, like the geometry cache: the material artwork of the previous
    // map has no reason to occupy address space on the next one.
    void MaterialTextures_Clear();
    void MaterialTextures_Forget();
} // namespace render
