#pragma once

typedef struct screenfade_s screenfade_t;

// Shared screen-fade helpers for the advanced radar: one derivation of the
// current fade alpha (mirroring the engine V_FadeAlpha) and one definition of a
// blinding flashbang (a near-white fade), so the radar wash and the enemy
// line-of-sight gating agree on what counts as a flash.
namespace RadarFade
{
    // Current fade alpha derived from the raw ScreenFade message fields, ramped
    // back down to zero the same way the engine does each frame.
    int Alpha(const screenfade_t& sf, double time);

    // A flashbang fades the screen toward white; tinted map/death fades do not.
    bool NearWhite(const screenfade_t& sf);
} // namespace RadarFade
