#pragma once

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "scalar.h"

// Vector / Euler-angle helpers with no dependency on the Valve SDK mathlib.
//
// Two styles coexist on purpose:
//   - Vector2 / Vector3 value types with operators.
//   - Free functions over raw float* (float[3] storage such as vec3_t),
//     operating on it in place without a copy.
// Both value types are standard-layout and size-compatible with float[N], so
// Vector3::Ref aliases such storage directly.
namespace ncl_math
{
    inline constexpr float kPi = 3.14159265358979323846f;
    inline constexpr float kDeg2Rad = kPi / 180.0f;
    inline constexpr float kRad2Deg = 180.0f / kPi;

    struct Vector2
    {
        float x{}, y{};

        float* data() { return &x; }
        const float* data() const { return &x; }
        float& operator[](int i) { return (&x)[i]; }
        float operator[](int i) const { return (&x)[i]; }

        Vector2 operator+(const Vector2& o) const { return {x + o.x, y + o.y}; }
        Vector2 operator-(const Vector2& o) const { return {x - o.x, y - o.y}; }
        Vector2 operator*(float s) const { return {x * s, y * s}; }
        Vector2 operator/(float s) const { return {x / s, y / s}; }
        Vector2 operator-() const { return {-x, -y}; }
        bool operator==(const Vector2& o) const { return x == o.x && y == o.y; }
        bool operator!=(const Vector2& o) const { return !(*this == o); }

        float Dot(const Vector2& o) const { return x * o.x + y * o.y; }
        float LengthSqr() const { return Dot(*this); }
        float Length() const { return std::sqrt(LengthSqr()); }

        // Unit vector in the same direction; a zero vector yields a zero vector.
        Vector2 Normalized() const
        {
            float len = Length();
            return len > 0.0f ? Vector2{x / len, y / len} : Vector2{};
        }

        // Rotated +90 degrees (CCW): (x, y) -> (-y, x).
        Vector2 Perp() const { return {-y, x}; }

        // Rotated by the given angle (radians, CCW).
        Vector2 Rotated(float radians) const
        {
            float c = std::cos(radians);
            float s = std::sin(radians);
            return {x * c - y * s, x * s + y * c};
        }

        static Vector2& Ref(float* p) { return *reinterpret_cast<Vector2*>(p); }
        static const Vector2& Ref(const float* p) { return *reinterpret_cast<const Vector2*>(p); }
    };
    static_assert(sizeof(Vector2) == sizeof(float[2]));
    static_assert(std::is_standard_layout_v<Vector2>);

    struct Vector3
    {
        float x{}, y{}, z{};

        float* data() { return &x; }
        const float* data() const { return &x; }
        float& operator[](int i) { return (&x)[i]; }
        float operator[](int i) const { return (&x)[i]; }

        Vector3 operator+(const Vector3& o) const { return {x + o.x, y + o.y, z + o.z}; }
        Vector3 operator-(const Vector3& o) const { return {x - o.x, y - o.y, z - o.z}; }
        Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
        Vector3 operator/(float s) const { return {x / s, y / s, z / s}; }
        Vector3 operator-() const { return {-x, -y, -z}; }
        bool operator==(const Vector3& o) const { return x == o.x && y == o.y && z == o.z; }
        bool operator!=(const Vector3& o) const { return !(*this == o); }

        float Dot(const Vector3& o) const { return x * o.x + y * o.y + z * o.z; }
        Vector3 Cross(const Vector3& o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
        float LengthSqr() const { return Dot(*this); }
        float Length() const { return std::sqrt(LengthSqr()); }

        // Normalizes in place and returns the previous length; a zero vector is
        // left untouched.
        float Normalize()
        {
            float len = Length();
            if (len > 0.0f)
            {
                float inv = 1.0f / len;
                x *= inv;
                y *= inv;
                z *= inv;
            }
            return len;
        }

        // Unit vector in the same direction; a zero vector yields a zero vector.
        Vector3 Normalized() const
        {
            float len = Length();
            return len > 0.0f ? Vector3{x / len, y / len, z / len} : Vector3{};
        }

        static Vector3& Ref(float* p) { return *reinterpret_cast<Vector3*>(p); }
        static const Vector3& Ref(const float* p) { return *reinterpret_cast<const Vector3*>(p); }
    };
    static_assert(sizeof(Vector3) == sizeof(float[3]));
    static_assert(std::is_standard_layout_v<Vector3>);

    // 2D affine transform: maps a point p to origin + basis_x*p.x + basis_y*p.y.
    // basis_x/basis_y are the images of the x/y axes (the two columns of the 2x2
    // linear part); origin is the translation. ApplyVector omits the translation,
    // for transforming directions rather than points.
    struct Transform2
    {
        Vector2 basis_x{}, basis_y{}, origin{};

        Vector2 Apply(Vector2 p) const { return origin + basis_x * p.x + basis_y * p.y; }
        Vector2 ApplyVector(Vector2 v) const { return basis_x * v.x + basis_y * v.y; }
    };

    // Unit vector at the given angle (radians), CCW from +X.
    inline Vector2 FromAngle(float radians) { return {std::cos(radians), std::sin(radians)}; }

    inline Vector2 Lerp(Vector2 a, Vector2 b, float t) { return a + (b - a) * t; }
    inline Vector3 Lerp(const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; }

    inline float Distance(Vector2 a, Vector2 b) { return (a - b).Length(); }
    inline float DistanceSqr(Vector2 a, Vector2 b) { return (a - b).LengthSqr(); }
    inline float Distance(const Vector3& a, const Vector3& b) { return (a - b).Length(); }
    inline float DistanceSqr(const Vector3& a, const Vector3& b) { return (a - b).LengthSqr(); }

    inline Vector2 Min(Vector2 a, Vector2 b) { return {std::min(a.x, b.x), std::min(a.y, b.y)}; }
    inline Vector2 Max(Vector2 a, Vector2 b) { return {std::max(a.x, b.x), std::max(a.y, b.y)}; }

    inline void VectorCopy(const float* src, float* dst)
    {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }

    inline void VectorClear(float* v)
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 0.0f;
    }

    inline void VectorSubtract(const float* a, const float* b, float* out)
    {
        out[0] = a[0] - b[0];
        out[1] = a[1] - b[1];
        out[2] = a[2] - b[2];
    }

    inline void VectorAdd(const float* a, const float* b, float* out)
    {
        out[0] = a[0] + b[0];
        out[1] = a[1] + b[1];
        out[2] = a[2] + b[2];
    }

    inline void VectorScale(const float* a, float scale, float* out)
    {
        out[0] = a[0] * scale;
        out[1] = a[1] * scale;
        out[2] = a[2] * scale;
    }

    inline void VectorNegate(float* v)
    {
        v[0] = -v[0];
        v[1] = -v[1];
        v[2] = -v[2];
    }

    // out = start + scale * direction
    inline void VectorMA(const float* start, float scale, const float* direction, float* out)
    {
        out[0] = start[0] + direction[0] * scale;
        out[1] = start[1] + direction[1] * scale;
        out[2] = start[2] + direction[2] * scale;
    }

    // dst += scale * direction
    inline void VectorMA(const float* direction, float scale, float* dst)
    {
        dst[0] += direction[0] * scale;
        dst[1] += direction[1] * scale;
        dst[2] += direction[2] * scale;
    }

    // dst.xy += scale * direction.xy, dst.z -= scale * direction.z, mirroring
    // the SDK mathlib's VectorMA_2.
    inline void VectorMA_2(const float* direction, float scale, float* dst)
    {
        dst[0] += direction[0] * scale;
        dst[1] += direction[1] * scale;
        dst[2] -= direction[2] * scale;
    }

    inline float SegPointDistSq(const float* a, const float* b, const float* p)
    {
        ncl_math::Vector3 va = ncl_math::Vector3::Ref(a);
        ncl_math::Vector3 ab = ncl_math::Vector3::Ref(b) - va;
        ncl_math::Vector3 ap = ncl_math::Vector3::Ref(p) - va;
        float ab2 = ab.LengthSqr();
        float t = ab2 > 0.0f ? ap.Dot(ab) / ab2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        ncl_math::Vector3 closest = va + ab * t;
        return (ncl_math::Vector3::Ref(p) - closest).LengthSqr();
    }

    inline float DotProduct(const float* a, const float* b)
    {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    }

    inline float VectorLengthSqr(const float* v)
    {
        return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    }

    inline float VectorLength(const float* v)
    {
        return std::sqrt(VectorLengthSqr(v));
    }

    inline float Vector2DLength(const float* v)
    {
        return std::sqrt(v[0] * v[0] + v[1] * v[1]);
    }

    // Normalizes in place and returns the previous length; leaves a zero vector
    // untouched.
    inline float VectorNormalize(float* v)
    {
        float len = VectorLength(v);
        if (len > 0.0f)
        {
            float inv = 1.0f / len;
            v[0] *= inv;
            v[1] *= inv;
            v[2] *= inv;
        }
        return len;
    }

    // Euler angles (pitch, yaw, roll in degrees) -> basis vectors. Any output may
    // be null. Right-handed forward/right/up, matching the engine convention.
    inline void AngleVectors(const float* angles, float* forward, float* right, float* up)
    {
        float sp = std::sin(angles[0] * kDeg2Rad);
        float cp = std::cos(angles[0] * kDeg2Rad);
        float sy = std::sin(angles[1] * kDeg2Rad);
        float cy = std::cos(angles[1] * kDeg2Rad);
        float sr = std::sin(angles[2] * kDeg2Rad);
        float cr = std::cos(angles[2] * kDeg2Rad);

        if (forward)
        {
            forward[0] = cp * cy;
            forward[1] = cp * sy;
            forward[2] = -sp;
        }
        if (right)
        {
            right[0] = -sr * sp * cy + cr * sy;
            right[1] = -sr * sp * sy - cr * cy;
            right[2] = -sr * cp;
        }
        if (up)
        {
            up[0] = cr * sp * cy + sr * sy;
            up[1] = cr * sp * sy - sr * cy;
            up[2] = cr * cp;
        }
    }

    // Wraps an angle into [-180, 180].
    inline float AngleNormalize(float angle)
    {
        angle = std::fmod(angle, 360.0f);
        if (angle > 180.0f)
        {
            angle -= 360.0f;
        }
        if (angle < -180.0f)
        {
            angle += 360.0f;
        }
        return angle;
    }

    // Per-component shortest-path interpolation between two Euler angles.
    inline void AngleLerp(const float* a, const float* b, float t, float* out)
    {
        for (int i = 0; i < 3; i++)
        {
            float dt = b[i] - a[i];
            if (dt < -180.0f)
            {
                dt += 360.0f;
            }
            else if (dt > 180.0f)
            {
                dt -= 360.0f;
            }
            out[i] = a[i] + t * dt;
        }
    }
} // namespace ncl_math
