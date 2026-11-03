#include "ncl_math/Quaternion.h"

#include <cmath>
#include <numbers>

#include "ncl_math/trigonometry.h"

namespace ncl_math
{
    namespace
    {
        constexpr double kSlerpEpsilon = 1e-6;
    } // namespace

    void AngleQuaternion(const Vector3& angles, Quaternion& quaternion)
    {
        float sx, cx;
        SinCos(angles.x * 0.5f, sx, cx);
        float sy, cy;
        SinCos(angles.y * 0.5f, sy, cy);
        float sz, cz;
        SinCos(angles.z * 0.5f, sz, cz);

        quaternion[0] = sx * cy * cz - cx * sy * sz;
        quaternion[1] = cx * sy * cz + sx * cy * sz;
        quaternion[2] = cx * cy * sz - sx * sy * cz;
        quaternion[3] = cx * cy * cz + sx * sy * sz;
    }

    void QuaternionSlerp(const Quaternion& first, Quaternion& second, float blend, Quaternion& output)
    {
        // A float distance sum can round a shortest-path decision to a tie.
        double difference_squared = 0;
        double sum_squared = 0;

        for (int coordinate = 0; coordinate < 4; ++coordinate)
        {
            const double difference = double(first[coordinate]) - second[coordinate];
            const double sum = double(first[coordinate]) + second[coordinate];
            difference_squared += difference * difference;
            sum_squared += sum * sum;
        }

        if (difference_squared > sum_squared)
        {
            for (int coordinate = 0; coordinate < 4; ++coordinate)
            {
                second[coordinate] = -second[coordinate];
            }
        }

        double dot = 0;

        for (int coordinate = 3; coordinate >= 0; --coordinate)
        {
            dot += double(first[coordinate]) * second[coordinate];
        }

        const float inverse = 1.0f - blend;

        if (1.0 + dot > kSlerpEpsilon)
        {
            float first_weight = inverse;
            float second_weight = blend;
            const float stored_dot = float(dot);

            if (1.0 - stored_dot > kSlerpEpsilon)
            {
                const float angle = std::acos(stored_dot);
                const float denominator = std::sin(angle);

                first_weight = std::sin(inverse * angle) / denominator;
                second_weight = std::sin(blend * angle) / denominator;
            }

            for (int coordinate = 0; coordinate < 4; ++coordinate)
            {
                output[coordinate] = first_weight * first[coordinate] + second_weight * second[coordinate];
            }

            return;
        }

        output[0] = -second[1];
        output[1] = second[0];
        output[2] = -second[3];
        output[3] = second[2];

        const double first_weight = std::sin(inverse * (std::numbers::pi / 2.0));
        const double second_weight = std::sin(double(blend) * (std::numbers::pi / 2.0));

        for (int coordinate = 0; coordinate < 3; ++coordinate)
        {
            output[coordinate] = float(first_weight * first[coordinate] + second_weight * output[coordinate]);
        }
    }

} // namespace ncl_math
