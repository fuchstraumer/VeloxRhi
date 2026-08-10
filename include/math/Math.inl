#pragma once
// Implementation file for Math.hpp. This file holds the *portable* bits -
// storage-type (Float2/3/4, Float3x3/4x3/4x4) implementations
// Reference implementation, including DirectXMath SIMD inline: 
// https://github.com/fuchstraumer/DiamondDogs/blob/master/foundation/include/math/Math.inl
// other SIMD backends are in MathBackendWASM.inl and MathBackendDX.inl

namespace math
{
    // ================================
    // Float2 implementations
    // ================================
    constexpr inline Float2 Float2::operator+(const Float2& rhs) const noexcept
    {
        return Float2(x + rhs.x, y + rhs.y);
    }

    constexpr inline Float2 Float2::operator-(const Float2& rhs) const noexcept
    {
        return Float2(x - rhs.x, y - rhs.y);
    }

    constexpr inline Float2 Float2::operator*(const Float2& rhs) const noexcept
    {
        return Float2(x * rhs.x, y * rhs.y);
    }

    constexpr inline Float2 Float2::operator/(const Float2& rhs) const noexcept
    {
        return Float2(x / rhs.x, y / rhs.y);
    }

    constexpr inline Float2 Float2::operator*(float scalar) const noexcept
    {
        return Float2(x * scalar, y * scalar);
    }

    constexpr inline Float2 Float2::operator/(float scalar) const noexcept
    {
        return Float2(x / scalar, y / scalar);
    }

    constexpr inline Float2 Float2::operator-() const noexcept
    {
        return Float2(-x, -y);
    }

    constexpr inline Float2& Float2::operator+=(const Float2& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr inline Float2& Float2::operator-=(const Float2& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr inline Float2& Float2::operator*=(const Float2& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }

    constexpr inline Float2& Float2::operator/=(const Float2& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }

    constexpr inline Float2& Float2::operator*=(float scalar) noexcept
    {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr inline Float2& Float2::operator/=(float scalar) noexcept
    {
        x /= scalar;
        y /= scalar;
        return *this;
    }

    constexpr inline bool Float2::operator==(const Float2& rhs) const noexcept
    {
        return x == rhs.x && y == rhs.y;
    }

    constexpr inline bool Float2::operator!=(const Float2& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    // ================================
    // Float3 implementations
    // ================================
    constexpr inline Float3 Float3::operator+(const Float3& rhs) const noexcept
    {
        return Float3(x + rhs.x, y + rhs.y, z + rhs.z);
    }

    constexpr inline Float3 Float3::operator-(const Float3& rhs) const noexcept
    {
        return Float3(x - rhs.x, y - rhs.y, z - rhs.z);
    }

    constexpr inline Float3 Float3::operator*(const Float3& rhs) const noexcept
    {
        return Float3(x * rhs.x, y * rhs.y, z * rhs.z);
    }

    constexpr inline Float3 Float3::operator/(const Float3& rhs) const noexcept
    {
        return Float3(x / rhs.x, y / rhs.y, z / rhs.z);
    }

    constexpr inline Float3 Float3::operator*(float scalar) const noexcept
    {
        return Float3(x * scalar, y * scalar, z * scalar);
    }

    constexpr inline Float3 Float3::operator/(float scalar) const noexcept
    {
        return Float3(x / scalar, y / scalar, z / scalar);
    }

    constexpr inline Float3 Float3::operator-() const noexcept
    {
        return Float3(-x, -y, -z);
    }

    constexpr Float3& Float3::operator+=(const Float3& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Float3& Float3::operator-=(const Float3& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Float3& Float3::operator*=(const Float3& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        return *this;
    }

    constexpr Float3& Float3::operator/=(const Float3& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        return *this;
    }

    constexpr Float3& Float3::operator*=(float scalar) noexcept
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    constexpr Float3& Float3::operator/=(float scalar) noexcept
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        return *this;
    }

    constexpr bool Float3::operator==(const Float3& rhs) const noexcept
    {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    constexpr bool Float3::operator!=(const Float3& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    // ================================
    // Float4 implementations
    // ================================
    constexpr inline Float4 Float4::operator+(const Float4& rhs) const noexcept
    {
        return Float4(x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w);
    }

    constexpr inline Float4 Float4::operator-(const Float4& rhs) const noexcept
    {
        return Float4(x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w);
    }

    constexpr inline Float4 Float4::operator*(const Float4& rhs) const noexcept
    {
        return Float4(x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w);
    }

    constexpr inline Float4 Float4::operator/(const Float4& rhs) const noexcept
    {
        return Float4(x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w);
    }

    constexpr inline Float4 Float4::operator*(float scalar) const noexcept
    {
        return Float4(x * scalar, y * scalar, z * scalar, w * scalar);
    }

    constexpr inline Float4 Float4::operator/(float scalar) const noexcept
    {
        return Float4(x / scalar, y / scalar, z / scalar, w / scalar);
    }

    constexpr inline Float4 Float4::operator-() const noexcept
    {
        return Float4(-x, -y, -z, -w);
    }

    constexpr Float4& Float4::operator+=(const Float4& rhs) noexcept
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    constexpr Float4& Float4::operator-=(const Float4& rhs) noexcept
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    constexpr Float4& Float4::operator*=(const Float4& rhs) noexcept
    {
        x *= rhs.x;
        y *= rhs.y;
        z *= rhs.z;
        w *= rhs.w;
        return *this;
    }

    constexpr Float4& Float4::operator/=(const Float4& rhs) noexcept
    {
        x /= rhs.x;
        y /= rhs.y;
        z /= rhs.z;
        w /= rhs.w;
        return *this;
    }

    constexpr Float4& Float4::operator*=(float scalar) noexcept
    {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    constexpr Float4& Float4::operator/=(float scalar) noexcept
    {
        x /= scalar;
        y /= scalar;
        z /= scalar;
        w /= scalar;
        return *this;
    }

    constexpr bool Float4::operator==(const Float4& rhs) const noexcept
    {
        return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
    }

    constexpr bool Float4::operator!=(const Float4& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    // ================================
    // Free Function Implementations (storage types - portable)
    // ================================

    constexpr Float2 operator*(float scalar, const Float2& vec) noexcept
    {
        return vec * scalar;
    }

    constexpr Float3 operator*(float scalar, const Float3& vec) noexcept
    {
        return vec * scalar;
    }

    constexpr Float4 operator*(float scalar, const Float4& vec) noexcept
    {
        return vec * scalar;
    }

    // ================================
    // Matrix3x3 Storage Implementation
    // ================================

    constexpr Float3x3::Float3x3(const Float4x4& mat4x4) noexcept
        : m{
            {mat4x4[0, 0], mat4x4[0, 1], mat4x4[0, 2]},
            {mat4x4[1, 0], mat4x4[1, 1], mat4x4[1, 2]},
            {mat4x4[2, 0], mat4x4[2, 1], mat4x4[2, 2]}
        }
    {
    }

    constexpr Float3 Float3x3::Row(size_t index) const noexcept
    {
        return Float3(m[index][0], m[index][1], m[index][2]);
    }

    constexpr void Float3x3::SetRow(size_t index, const Float3& row) noexcept
    {
        m[index][0] = row.x;
        m[index][1] = row.y;
        m[index][2] = row.z;
    }

    constexpr Float3 Float3x3::Column(size_t index) const noexcept
    {
        return Float3(m[0][index], m[1][index], m[2][index]);
    }

    constexpr void Float3x3::SetColumn(size_t index, const Float3& column) noexcept
    {
        m[0][index] = column.x;
        m[1][index] = column.y;
        m[2][index] = column.z;
    }

    constexpr bool Float3x3::operator==(const Float3x3& rhs) const noexcept
    {
        return m[0][0] == rhs.m[0][0] && m[0][1] == rhs.m[0][1] && m[0][2] == rhs.m[0][2] &&
               m[1][0] == rhs.m[1][0] && m[1][1] == rhs.m[1][1] && m[1][2] == rhs.m[1][2] &&
               m[2][0] == rhs.m[2][0] && m[2][1] == rhs.m[2][1] && m[2][2] == rhs.m[2][2];
    }

    constexpr bool Float3x3::operator!=(const Float3x3& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    constexpr Float3x3 Float3x3::Identity() noexcept
    {
        return Float3x3();
    }

    constexpr Float3x3 Float3x3::Zero() noexcept
    {
        return Float3x3(
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f
        );
    }

    // ================================
    // Matrix4x3 Storage Implementation
    // ================================

    constexpr Float3 Float4x3::Row(size_t index) const noexcept
    {
        return Float3(m[index][0], m[index][1], m[index][2]);
    }

    constexpr void Float4x3::SetRow(size_t index, const Float3& row) noexcept
    {
        m[index][0] = row.x;
        m[index][1] = row.y;
        m[index][2] = row.z;
    }

    constexpr Float4 Float4x3::Column(size_t index) const noexcept
    {
        return Float4(m[0][index], m[1][index], m[2][index], m[3][index]);
    }

    constexpr void Float4x3::SetColumn(size_t index, const Float4& column) noexcept
    {
        m[0][index] = column.x;
        m[1][index] = column.y;
        m[2][index] = column.z;
        m[3][index] = column.w;
    }

    constexpr bool Float4x3::operator==(const Float4x3& rhs) const noexcept
    {
        return m[0][0] == rhs.m[0][0] && m[0][1] == rhs.m[0][1] && m[0][2] == rhs.m[0][2] &&
               m[1][0] == rhs.m[1][0] && m[1][1] == rhs.m[1][1] && m[1][2] == rhs.m[1][2] &&
               m[2][0] == rhs.m[2][0] && m[2][1] == rhs.m[2][1] && m[2][2] == rhs.m[2][2] &&
               m[3][0] == rhs.m[3][0] && m[3][1] == rhs.m[3][1] && m[3][2] == rhs.m[3][2];
    }

    constexpr bool Float4x3::operator!=(const Float4x3& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    constexpr Float4x3 Float4x3::Identity() noexcept
    {
        return Float4x3();
    }

    constexpr Float4x3 Float4x3::Zero() noexcept
    {
        return Float4x3(
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f
        );
    }

    // ================================
    // Matrix4x4 Storage Implementation
    // ================================

    constexpr Float4x4::Float4x4(const Float3x3& mat3x3) noexcept
        : m{
            {mat3x3[0, 0], mat3x3[0, 1], mat3x3[0, 2], 0.0f},
            {mat3x3[1, 0], mat3x3[1, 1], mat3x3[1, 2], 0.0f},
            {mat3x3[2, 0], mat3x3[2, 1], mat3x3[2, 2], 0.0f},
            {0.0f,         0.0f,         0.0f,         1.0f}
        }
    {
    }

    constexpr Float4 Float4x4::Row(size_t index) const noexcept
    {
        return Float4(m[index][0], m[index][1], m[index][2], m[index][3]);
    }

    constexpr void Float4x4::SetRow(size_t index, const Float4& row) noexcept
    {
        m[index][0] = row.x;
        m[index][1] = row.y;
        m[index][2] = row.z;
        m[index][3] = row.w;
    }

    constexpr Float4 Float4x4::Column(size_t index) const noexcept
    {
        return Float4(m[0][index], m[1][index], m[2][index], m[3][index]);
    }

    constexpr void Float4x4::SetColumn(size_t index, const Float4& column) noexcept
    {
        m[0][index] = column.x;
        m[1][index] = column.y;
        m[2][index] = column.z;
        m[3][index] = column.w;
    }

    constexpr bool Float4x4::operator==(const Float4x4& rhs) const noexcept
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (m[i][j] != rhs.m[i][j])
                {
                    return false;
                }
            }
        }
        return true;
    }

    constexpr bool Float4x4::operator!=(const Float4x4& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    constexpr Float4x4 Float4x4::Identity() noexcept
    {
        return Float4x4();
    }

    constexpr Float4x4 Float4x4::Zero() noexcept
    {
        return Float4x4(
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f
        );
    }

    // ================================
    // Storage-Matrix * Storage-Vector free functions
    // ================================
    // Row-vector convention throughout (result = vec * mat), consistent with
    // the SIMD Matrix type. NONE of these five free functions had an
    // implementation in the original .inl (declared in the header only) -
    // implemented here under the documented assumptions below; please
    // double-check them against your intended usage before relying on them.

    // Pure linear transform (rotation/scale, no translation): Float3 in, Float3 out.
    constexpr Float3 operator*(const Float3x3& mat, const Float3& vec) noexcept
    {
        return Float3(
            vec.x * mat[0, 0] + vec.y * mat[1, 0] + vec.z * mat[2, 0],
            vec.x * mat[0, 1] + vec.y * mat[1, 1] + vec.z * mat[2, 1],
            vec.x * mat[0, 2] + vec.y * mat[1, 2] + vec.z * mat[2, 2]
        );
    }

    // Assumption: `vec` is a point (implicit w=1); rows 0-2 are the linear part,
    // row 3 is translation. Returns the transformed point with w=1 so it can be
    // chained into further homogeneous math.
    constexpr Float4 operator*(const Float4x3& mat, const Float3& vec) noexcept
    {
        return Float4(
            vec.x * mat[0, 0] + vec.y * mat[1, 0] + vec.z * mat[2, 0] + mat[3, 0],
            vec.x * mat[0, 1] + vec.y * mat[1, 1] + vec.z * mat[2, 1] + mat[3, 1],
            vec.x * mat[0, 2] + vec.y * mat[1, 2] + vec.z * mat[2, 2] + mat[3, 2],
            1.0f
        );
    }

    // Assumption: Float4x3 is treated as a 4x4 matrix with an implicit last
    // column of (0, 0, 0, 1) - i.e. rows 0-2 are the linear part scaled by
    // vec.xyz, row 3 (translation) is scaled by vec.w, and vec.w passes
    // through unchanged into the result's w (since the implicit column only
    // contributes a "1" against w).
    constexpr Float4 operator*(const Float4x3& mat, const Float4& vec) noexcept
    {
        return Float4(
            vec.x * mat[0, 0] + vec.y * mat[1, 0] + vec.z * mat[2, 0] + vec.w * mat[3, 0],
            vec.x * mat[0, 1] + vec.y * mat[1, 1] + vec.z * mat[2, 1] + vec.w * mat[3, 1],
            vec.x * mat[0, 2] + vec.y * mat[1, 2] + vec.z * mat[2, 2] + vec.w * mat[3, 2],
            vec.w
        );
    }

    // Standard full homogeneous transform.
    constexpr Float4 operator*(const Float4x4& mat, const Float4& vec) noexcept
    {
        return Float4(
            vec.x * mat[0, 0] + vec.y * mat[1, 0] + vec.z * mat[2, 0] + vec.w * mat[3, 0],
            vec.x * mat[0, 1] + vec.y * mat[1, 1] + vec.z * mat[2, 1] + vec.w * mat[3, 1],
            vec.x * mat[0, 2] + vec.y * mat[1, 2] + vec.z * mat[2, 2] + vec.w * mat[3, 2],
            vec.x * mat[0, 3] + vec.y * mat[1, 3] + vec.z * mat[2, 3] + vec.w * mat[3, 3]
        );
    }

    // Assumption: `vec` is a point (implicit w=1); result drops w with NO
    // perspective divide (i.e. this assumes an affine matrix - use the SIMD
    // Matrix/Vector path's Transform<3>() if you need true perspective-correct
    // TransformCoord semantics).
    constexpr Float3 operator*(const Float4x4& mat, const Float3& vec) noexcept
    {
        return Float3(
            vec.x * mat[0, 0] + vec.y * mat[1, 0] + vec.z * mat[2, 0] + mat[3, 0],
            vec.x * mat[0, 1] + vec.y * mat[1, 1] + vec.z * mat[2, 1] + mat[3, 1],
            vec.x * mat[0, 2] + vec.y * mat[1, 2] + vec.z * mat[2, 2] + mat[3, 2]
        );
    }

} // namespace math

// ============================================================================
// Backend dispatch - Vector/Matrix implementations
// ============================================================================
#if DD_MATH_BACKEND_WASM
    #include "math/MathBackendWASM.inl"
#else
    #include "math/MathBackendDX.inl"
#endif
