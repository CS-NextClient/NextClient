#pragma once

#include <cstring>
#include <type_traits>

#include "ncl_math/vec3.h"

namespace ncl_math
{
    // Components are stored in x, y, z, w order.
    struct Quaternion
    {
        float values[4]{};

        float* data()
        {
            return values;
        }

        const float* data() const
        {
            return values;
        }

        float& operator[](int index)
        {
            return values[index];
        }

        float operator[](int index) const
        {
            return values[index];
        }

        void Store(float* destination) const
        {
            std::memcpy(destination, values, sizeof(values));
        }

        static Quaternion Load(const float* source)
        {
            Quaternion result;
            std::memcpy(result.values, source, sizeof(result.values));

            return result;
        }
    };

    static_assert(sizeof(Quaternion) == sizeof(float[4]));
    static_assert(std::is_trivially_copyable_v<Quaternion>);
    static_assert(std::is_standard_layout_v<Quaternion>);

    // Angles are radians about X, Y, Z, composed as Rz * Ry * Rx.
    void AngleQuaternion(const Vector3& angles, Quaternion& quaternion);

    // Selects the shortest path by changing the second quaternion's sign.
    void QuaternionSlerp(const Quaternion& first, Quaternion& second, float blend, Quaternion& output);

} // namespace ncl_math
