#pragma once

#include <cstdint>

#include <goldsrc/format/studio.h>
#include <ncl_math/Quaternion.h>

namespace ncl_studio
{
    void CalcBoneAdj(
        const goldsrc::studiohdr_t& header,
        float interpolant,
        float* adjustment,
        const uint8_t* current,
        const uint8_t* previous,
        uint8_t mouth
    );
    void CalcBoneQuaternion(
        int frame,
        float fraction,
        const goldsrc::mstudiobone_t& bone,
        const goldsrc::mstudioanim_t& animation,
        const float* adjustment,
        ncl_math::Quaternion& quaternion
    );
    void CalcBonePosition(
        int frame,
        float fraction,
        const goldsrc::mstudiobone_t& bone,
        const goldsrc::mstudioanim_t& animation,
        const float* adjustment,
        ncl_math::Vector3& position
    );
} // namespace ncl_studio
