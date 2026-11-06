#include "ncl_math/Matrix3x4.h"

#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace ncl_math
{
    void QuaternionMatrix(const Quaternion& quaternion, Matrix3x4& output)
    {
        output[0][0] = 1.0f - 2.0f * quaternion[1] * quaternion[1] - 2.0f * quaternion[2] * quaternion[2];
        output[1][0] = 2.0f * (quaternion[0] * quaternion[1] + quaternion[3] * quaternion[2]);
        output[2][0] = 2.0f * quaternion[0] * quaternion[2] - 2.0f * quaternion[3] * quaternion[1];

        output[0][1] = 2.0f * quaternion[0] * quaternion[1] - 2.0f * quaternion[3] * quaternion[2];
        output[1][1] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[2] * quaternion[2];
        output[2][1] = 2.0f * (quaternion[1] * quaternion[2] + quaternion[3] * quaternion[0]);

        output[0][2] = 2.0f * (quaternion[0] * quaternion[2] + quaternion[3] * quaternion[1]);
        output[1][2] = 2.0f * quaternion[1] * quaternion[2] - 2.0f * quaternion[3] * quaternion[0];
        output[2][2] = 1.0f - 2.0f * quaternion[0] * quaternion[0] - 2.0f * quaternion[1] * quaternion[1];
    }

    void ConcatTransforms(const Matrix3x4& left, const Matrix3x4& right, Matrix3x4& output)
    {
#if defined(_M_IX86) || defined(_M_X64) || defined(__SSE__)
        // In-place composition preserves the scalar load/store order.
        if (&output != &left && &output != &right)
        {
            const __m128 right0 = _mm_loadu_ps(right[0]);
            const __m128 right1 = _mm_loadu_ps(right[1]);
            const __m128 right2 = _mm_loadu_ps(right[2]);

            for (int row = 0; row < 3; ++row)
            {
                const __m128 first = _mm_mul_ps(_mm_set1_ps(left[row][0]), right0);
                const __m128 second = _mm_mul_ps(_mm_set1_ps(left[row][1]), right1);
                const __m128 third = _mm_mul_ps(_mm_set1_ps(left[row][2]), right2);
                const __m128 value = _mm_add_ps(_mm_add_ps(first, second), third);
                const __m128 translation = _mm_add_ss(_mm_shuffle_ps(value, value, _MM_SHUFFLE(3, 3, 3, 3)), _mm_load_ss(&left[row][3]));

                _mm_storeu_ps(output[row], value);
                _mm_store_ss(&output[row][3], translation);
            }

            return;
        }
#endif

        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 4; ++column)
            {
                const float first = left[row][0] * right[0][column];
                const float second = left[row][1] * right[1][column];
                const float third = left[row][2] * right[2][column];
                float value = (first + second) + third;

                if (column == 3)
                {
                    value += left[row][3];
                }

                output[row][column] = value;
            }
        }
    }

} // namespace ncl_math
