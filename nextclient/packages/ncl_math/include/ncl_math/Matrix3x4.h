#pragma once

#include <cstring>
#include <type_traits>

#include "ncl_math/Quaternion.h"

namespace ncl_math
{
    // Row-major affine transform with translation in the last column.
    struct Matrix3x4
    {
        using row_t = float[4];

        row_t rows[3]{};

        row_t& operator[](int row)
        {
            return rows[row];
        }

        const row_t& operator[](int row) const
        {
            return rows[row];
        }

        void Store(row_t* destination) const
        {
            std::memcpy(destination, rows, sizeof(rows));
        }

        static Matrix3x4 Load(const row_t* source)
        {
            Matrix3x4 result;
            std::memcpy(result.rows, source, sizeof(result.rows));

            return result;
        }
    };

    static_assert(sizeof(Matrix3x4) == sizeof(float[3][4]));
    static_assert(std::is_trivially_copyable_v<Matrix3x4>);
    static_assert(std::is_standard_layout_v<Matrix3x4>);

    // Replaces only the rotation columns, preserving translation.
    void QuaternionMatrix(const Quaternion& quaternion, Matrix3x4& output);

    // Aliased output observes each preceding row/column store.
    void ConcatTransforms(const Matrix3x4& left, const Matrix3x4& right, Matrix3x4& output);
} // namespace ncl_math
