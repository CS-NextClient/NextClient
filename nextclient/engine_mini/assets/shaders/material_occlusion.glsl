uniform float uDepthTolerance;
uniform sampler2D uObjectDepth;
uniform sampler2D uSceneDepth;
uniform sampler2DMS uObjectDepthMs;
uniform sampler2DMS uSceneDepthMs;
uniform int uSamples;

bool MaterialOcclusion_IsHidden(float object_depth, float scene_depth, float depth_sign)
{
    return depth_sign * (object_depth - scene_depth) > uDepthTolerance;
}

bool MaterialOcclusion_IsHidden(ivec2 object_pixel, ivec2 scene_pixel, float depth_sign)
{
    if (uSamples == 0)
    {
        return MaterialOcclusion_IsHidden(
            texelFetch(uObjectDepth, object_pixel, 0).r,
            texelFetch(uSceneDepth, scene_pixel, 0).r,
            depth_sign
        );
    }

    bool covered = false;
    float far_depth = depth_sign > 0.0 ? 1.0 : 0.0;
    for (int sample_index = 0; sample_index < uSamples; sample_index++)
    {
        float object_depth = texelFetch(uObjectDepthMs, object_pixel, sample_index).r;
        if (object_depth == far_depth)
        {
            continue;
        }
        covered = true;
        if (!MaterialOcclusion_IsHidden(object_depth, texelFetch(uSceneDepthMs, scene_pixel, sample_index).r, depth_sign))
        {
            return false;
        }
    }
    return covered;
}

bool MaterialOcclusion_HasSampleCoverage(ivec2 object_pixel, float depth_sign)
{
    float far_depth = depth_sign > 0.0 ? 1.0 : 0.0;
    for (int sample_index = 0; sample_index < uSamples; sample_index++)
    {
        if (texelFetch(uObjectDepthMs, object_pixel, sample_index).r != far_depth)
        {
            return true;
        }
    }
    return false;
}
