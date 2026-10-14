#pragma once

#include <goldsrc/fwd.h>

// One derivation of the current fade alpha and one definition of a blinding flashbang,
// so the radar wash and the enemy line-of-sight gating agree on what counts as a flash.

// Mirrors the engine V_FadeAlpha: ramped back down to zero the same way each frame.
int RadarFade_ComputeAlpha(const goldsrc::screenfade_t& sf, double time);

// A flashbang fades the screen toward white; tinted map and death fades do not.
bool RadarFade_IsNearWhite(const goldsrc::screenfade_t& sf);
