#pragma once
// WASM SIMD128 backend for math::Vector / math::Matrix
// Requires <wasm_simd128.h>, <cmath>, and <limits>, already included by Math.hpp.
namespace velox::math
{
namespace detail
{
    // General 4x4 inverse via Gauss-Jordan elimination with partial
    // pivoting. Not vectorized - matrix inversion is rarely a hot-path
    // operation (once or twice a frame, typically), so a clear scalar
    // implementation is worth more here than a bespoke SIMD one.
    inline bool InvertMatrix4x4(const float in[4][4], float out[4][4]) noexcept
    {
        float a[4][8];
        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                a[r][c] = in[r][c];
                a[r][c + 4] = (r == c) ? 1.0f : 0.0f;
            }
        }

        for (int col = 0; col < 4; ++col)
        {
            int pivotRow = col;
            float pivotVal = std::fabs(a[col][col]);
            for (int r = col + 1; r < 4; ++r)
            {
                float v = std::fabs(a[r][col]);
                if (v > pivotVal)
                {
                    pivotVal = v;
                    pivotRow = r;
                }
            }
            if (pivotVal < 1e-8f)
            {
                return false; // singular
            }
            if (pivotRow != col)
            {
                for (int c = 0; c < 8; ++c)
                {
                    std::swap(a[col][c], a[pivotRow][c]);
                }
            }
            float inv_pivot = 1.0f / a[col][col];
            for (int c = 0; c < 8; ++c)
            {
                a[col][c] *= inv_pivot;
            }
            for (int r = 0; r < 4; ++r)
            {
                if (r == col)
                {
                    continue;
                }
                float factor = a[r][col];
                for (int c = 0; c < 8; ++c)
                {
                    a[r][c] -= factor * a[col][c];
                }
            }
        }

        for (int r = 0; r < 4; ++r)
        {
            for (int c = 0; c < 4; ++c)
            {
                out[r][c] = a[r][c + 4];
            }
        }
        return true;
    }

    inline float Det3x3(
        float a1, float a2, float a3, float b1, float b2, float b3, float c1, float c2, float c3) noexcept
    {
        return a1 * (b2 * c3 - b3 * c2) - a2 * (b1 * c3 - b3 * c1) + a3 * (b1 * c2 - b2 * c1);
    }

    inline float Determinant4x4(const float m[4][4]) noexcept
    {
        return m[0][0] *
                   Det3x3(m[1][1], m[1][2], m[1][3], m[2][1], m[2][2], m[2][3], m[3][1], m[3][2], m[3][3]) -
               m[0][1] *
                   Det3x3(m[1][0], m[1][2], m[1][3], m[2][0], m[2][2], m[2][3], m[3][0], m[3][2], m[3][3]) +
               m[0][2] *
                   Det3x3(m[1][0], m[1][1], m[1][3], m[2][0], m[2][1], m[2][3], m[3][0], m[3][1], m[3][3]) -
               m[0][3] *
                   Det3x3(m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2], m[3][0], m[3][1], m[3][2]);
    }
} // namespace detail

// ================================
// Vector SIMD Implementation (WASM SIMD128)
// ================================

VX_MATH_FORCEINLINE Vector::Vector() noexcept
    : data{ wasm_f32x4_splat(0.0f) }
{
}

VX_MATH_FORCEINLINE Vector::Vector(float x, float y, float z, float w) noexcept
    : data{ wasm_f32x4_make(x, y, z, w) }
{
}

VX_MATH_FORCEINLINE Vector::Vector(float x, float y, float z) noexcept
    : data{ wasm_f32x4_make(x, y, z, 0.0f) }
{
}

VX_MATH_FORCEINLINE Vector::Vector(float x, float y) noexcept
    : data{ wasm_f32x4_make(x, y, 0.0f, 0.0f) }
{
}

VX_MATH_FORCEINLINE Vector::Vector(float scalar) noexcept
    : data{ wasm_f32x4_splat(scalar) }
{
}

// Component accessors
VX_MATH_FORCEINLINE float Vector::x() const noexcept
{
    return wasm_f32x4_extract_lane(data, 0);
}
VX_MATH_FORCEINLINE float Vector::y() const noexcept
{
    return wasm_f32x4_extract_lane(data, 1);
}
VX_MATH_FORCEINLINE float Vector::z() const noexcept
{
    return wasm_f32x4_extract_lane(data, 2);
}
VX_MATH_FORCEINLINE float Vector::w() const noexcept
{
    return wasm_f32x4_extract_lane(data, 3);
}

// Static factory methods
VX_MATH_FORCEINLINE Vector Vector::Zero() noexcept
{
    return Vector{ wasm_f32x4_splat(0.0f) };
}
VX_MATH_FORCEINLINE Vector Vector::Replicate(float scalar) noexcept
{
    return Vector{ wasm_f32x4_splat(scalar) };
}
VX_MATH_FORCEINLINE Vector Vector::Identity() noexcept
{
    return Vector{ wasm_f32x4_splat(1.0f) };
}

// Arithmetic operators
VX_MATH_FORCEINLINE Vector Vector::operator+(Vector rhs) const noexcept
{
    return Vector{ wasm_f32x4_add(data, rhs.data) };
}
VX_MATH_FORCEINLINE Vector Vector::operator-(Vector rhs) const noexcept
{
    return Vector{ wasm_f32x4_sub(data, rhs.data) };
}
VX_MATH_FORCEINLINE Vector Vector::operator*(Vector rhs) const noexcept
{
    return Vector{ wasm_f32x4_mul(data, rhs.data) };
}
VX_MATH_FORCEINLINE Vector Vector::operator/(Vector rhs) const noexcept
{
    return Vector{ wasm_f32x4_div(data, rhs.data) };
}
VX_MATH_FORCEINLINE Vector Vector::operator*(float scalar) const noexcept
{
    return Vector{ wasm_f32x4_mul(data, wasm_f32x4_splat(scalar)) };
}
VX_MATH_FORCEINLINE Vector Vector::operator/(float scalar) const noexcept
{
    return Vector{ wasm_f32x4_div(data, wasm_f32x4_splat(scalar)) };
}
VX_MATH_FORCEINLINE Vector Vector::operator-() const noexcept
{
    return Vector{ wasm_f32x4_neg(data) };
}

VX_MATH_FORCEINLINE Vector Vector::MultiplyAdd(Vector factor, Vector addend) const noexcept
{
#if defined(VX_MATH_RELAXED_FMA)
    return Vector{ wasm_f32x4_relaxed_madd(data, factor.data, addend.data) };
#else
    return Vector{ wasm_f32x4_add(wasm_f32x4_mul(data, factor.data), addend.data) };
#endif
}

// Compound assignment operators
VX_MATH_FORCEINLINE Vector& Vector::operator+=(Vector rhs) noexcept
{
    data = wasm_f32x4_add(data, rhs.data);
    return *this;
}
VX_MATH_FORCEINLINE Vector& Vector::operator-=(Vector rhs) noexcept
{
    data = wasm_f32x4_sub(data, rhs.data);
    return *this;
}
VX_MATH_FORCEINLINE Vector& Vector::operator*=(Vector rhs) noexcept
{
    data = wasm_f32x4_mul(data, rhs.data);
    return *this;
}
VX_MATH_FORCEINLINE Vector& Vector::operator/=(Vector rhs) noexcept
{
    data = wasm_f32x4_div(data, rhs.data);
    return *this;
}
VX_MATH_FORCEINLINE Vector& Vector::operator*=(float scalar) noexcept
{
    data = wasm_f32x4_mul(data, wasm_f32x4_splat(scalar));
    return *this;
}
VX_MATH_FORCEINLINE Vector& Vector::operator/=(float scalar) noexcept
{
    data = wasm_f32x4_div(data, wasm_f32x4_splat(scalar));
    return *this;
}

VX_MATH_FORCEINLINE Vector Vector::Reciprocal() const noexcept
{
    return Vector{ wasm_f32x4_div(wasm_f32x4_splat(1.0f), data) };
}
VX_MATH_FORCEINLINE Vector Vector::Sqrt() const noexcept
{
    return Vector{ wasm_f32x4_sqrt(data) };
}
VX_MATH_FORCEINLINE Vector Vector::ReciprocalSqrt() const noexcept
{
    return Vector{ wasm_f32x4_div(wasm_f32x4_splat(1.0f), wasm_f32x4_sqrt(data)) };
}

// Dot<N> extracts only the lanes relevant to N, matching the semantics of
// DirectXMath's XMVector{2,3,4}Dot (which ignore unused lanes regardless
// of what garbage they may hold) rather than assuming unused lanes are
// zeroed.
template<int N>
VX_MATH_FORCEINLINE float Vector::Dot(Vector other) const noexcept
{
    static_assert(N >= 2 && N <= 4, "Dot dimensionality must be 2, 3, or 4");
    v128_t prod = wasm_f32x4_mul(data, other.data);
    float px = wasm_f32x4_extract_lane(prod, 0);
    float py = wasm_f32x4_extract_lane(prod, 1);
    if constexpr (N == 2)
    {
        return px + py;
    }
    else
    {
        float pz = wasm_f32x4_extract_lane(prod, 2);
        if constexpr (N == 3)
        {
            return px + py + pz;
        }
        else
        {
            float pw = wasm_f32x4_extract_lane(prod, 3);
            return px + py + pz + pw;
        }
    }
}

template<int N>
VX_MATH_FORCEINLINE float Vector::LengthSq() const noexcept
{
    static_assert(N >= 2 && N <= 4, "LengthSq dimensionality must be 2, 3, or 4");
    return Dot<N>(*this);
}

template<int N>
VX_MATH_FORCEINLINE float Vector::Length() const noexcept
{
    static_assert(N >= 2 && N <= 4, "Length dimensionality must be 2, 3, or 4");
    return std::sqrt(Dot<N>(*this));
}

template<int N>
VX_MATH_FORCEINLINE Vector Vector::Normalize() const noexcept
{
    static_assert(N >= 2 && N <= 4, "Normalize dimensionality must be 2, 3, or 4");
    float invLen = 1.0f / std::sqrt(Dot<N>(*this));
    return Vector{ wasm_f32x4_mul(data, wasm_f32x4_splat(invLen)) };
}

// Cross product only makes sense for 3D vectors. Standard SIMD cross
// product via two "rotated" shuffles: (a.yzx*b.zxy) - (a.zxy*b.yzx).
VX_MATH_FORCEINLINE Vector Vector::Cross(Vector other) const noexcept
{
    v128_t a_yzx = wasm_i32x4_shuffle(data, data, 1, 2, 0, 3);
    v128_t a_zxy = wasm_i32x4_shuffle(data, data, 2, 0, 1, 3);
    v128_t b_yzx = wasm_i32x4_shuffle(other.data, other.data, 1, 2, 0, 3);
    v128_t b_zxy = wasm_i32x4_shuffle(other.data, other.data, 2, 0, 1, 3);
    return Vector{ wasm_f32x4_sub(wasm_f32x4_mul(a_yzx, b_zxy), wasm_f32x4_mul(a_zxy, b_yzx)) };
}

VX_MATH_FORCEINLINE Vector Vector::Lerp(Vector target, float t) const noexcept
{
    v128_t diff = wasm_f32x4_sub(target.data, data);
    return Vector{ wasm_f32x4_add(data, wasm_f32x4_mul(diff, wasm_f32x4_splat(t))) };
}

template<int N>
VX_MATH_FORCEINLINE Vector Vector::Reflect(Vector normal) const noexcept
{
    static_assert(N >= 2 && N <= 4, "Reflect dimensionality must be 2, 3, or 4");
    float d = Dot<N>(normal);
    return *this - normal * (2.0f * d);
}

VX_MATH_FORCEINLINE Vector Vector::Clamp(Vector min, Vector max) const noexcept
{
    return Vector{ wasm_f32x4_min(wasm_f32x4_max(data, min.data), max.data) };
}

VX_MATH_FORCEINLINE Vector Vector::Saturate() const noexcept
{
    return Vector{ wasm_f32x4_min(wasm_f32x4_max(data, wasm_f32x4_splat(0.0f)), wasm_f32x4_splat(1.0f)) };
}

VX_MATH_FORCEINLINE Vector Vector::Abs() const noexcept
{
    return Vector{ wasm_f32x4_abs(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Min(Vector other) const noexcept
{
    return Vector{ wasm_f32x4_min(data, other.data) };
}

VX_MATH_FORCEINLINE Vector Vector::Max(Vector other) const noexcept
{
    return Vector{ wasm_f32x4_max(data, other.data) };
}

// No native SIMD pow in WASM SIMD128 - per-lane std::pow. Not remotely as
// fast as the rest of this file, but Pow() wasn't a hot-path SIMD win in
// the original DirectXMath backend either (XMVectorPow is itself a
// per-lane scalar loop under the hood).
VX_MATH_FORCEINLINE Vector Vector::Pow(float exponent) const noexcept
{
    return Vector{ wasm_f32x4_make(std::pow(wasm_f32x4_extract_lane(data, 0), exponent),
                                   std::pow(wasm_f32x4_extract_lane(data, 1), exponent),
                                   std::pow(wasm_f32x4_extract_lane(data, 2), exponent),
                                   std::pow(wasm_f32x4_extract_lane(data, 3), exponent)) };
}

VX_MATH_FORCEINLINE Vector Vector::Pow(Vector exponent) const noexcept
{
    return Vector{ wasm_f32x4_make(
        std::pow(wasm_f32x4_extract_lane(data, 0), wasm_f32x4_extract_lane(exponent.data, 0)),
        std::pow(wasm_f32x4_extract_lane(data, 1), wasm_f32x4_extract_lane(exponent.data, 1)),
        std::pow(wasm_f32x4_extract_lane(data, 2), wasm_f32x4_extract_lane(exponent.data, 2)),
        std::pow(wasm_f32x4_extract_lane(data, 3), wasm_f32x4_extract_lane(exponent.data, 3))) };
}

VX_MATH_FORCEINLINE Vector Vector::Abs(Vector vec) noexcept
{
    return Vector{ wasm_f32x4_abs(vec.data) };
}

VX_MATH_FORCEINLINE Vector Vector::Pow(Vector base, float exponent) noexcept
{
    return base.Pow(exponent);
}

VX_MATH_FORCEINLINE Vector Vector::Pow(Vector base, Vector exponent) noexcept
{
    return base.Pow(exponent);
}

// ================================
// Free Function Implementations (Vector)
// ================================

VX_MATH_FORCEINLINE Vector operator*(float scalar, Vector vec) noexcept
{
    return vec * scalar;
}

VX_MATH_FORCEINLINE Vector ToVector(const Float2& in) noexcept
{
    return Vector{ wasm_f32x4_make(in.x, in.y, 0.0f, 0.0f) };
}

VX_MATH_FORCEINLINE Vector ToVector(const Float3& in) noexcept
{
    return Vector{ wasm_f32x4_make(in.x, in.y, in.z, 0.0f) };
}

VX_MATH_FORCEINLINE Vector ToVector(const Float4& in) noexcept
{
    return Vector{ wasm_f32x4_make(in.x, in.y, in.z, in.w) };
}

template<>
VX_MATH_FORCEINLINE Float2 FromVector<Float2>(Vector vec) noexcept
{
    return Float2(wasm_f32x4_extract_lane(vec.Data(), 0), wasm_f32x4_extract_lane(vec.Data(), 1));
}

template<>
VX_MATH_FORCEINLINE Float3 FromVector<Float3>(Vector vec) noexcept
{
    return Float3(wasm_f32x4_extract_lane(vec.Data(), 0),
                  wasm_f32x4_extract_lane(vec.Data(), 1),
                  wasm_f32x4_extract_lane(vec.Data(), 2));
}

template<>
VX_MATH_FORCEINLINE Float4 FromVector<Float4>(Vector vec) noexcept
{
    return Float4(wasm_f32x4_extract_lane(vec.Data(), 0),
                  wasm_f32x4_extract_lane(vec.Data(), 1),
                  wasm_f32x4_extract_lane(vec.Data(), 2),
                  wasm_f32x4_extract_lane(vec.Data(), 3));
}

// ================================
// SIMD Matrix Implementation (WASM SIMD128)
// ================================

VX_MATH_FORCEINLINE Matrix::Matrix() noexcept
    : data{ wasm_f32x4_make(1.0f, 0.0f, 0.0f, 0.0f),
            wasm_f32x4_make(0.0f, 1.0f, 0.0f, 0.0f),
            wasm_f32x4_make(0.0f, 0.0f, 1.0f, 0.0f),
            wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) }
{
}

VX_MATH_FORCEINLINE Matrix::Matrix(float m00,
                                   float m01,
                                   float m02,
                                   float m03,
                                   float m10,
                                   float m11,
                                   float m12,
                                   float m13,
                                   float m20,
                                   float m21,
                                   float m22,
                                   float m23,
                                   float m30,
                                   float m31,
                                   float m32,
                                   float m33) noexcept
    : data{ wasm_f32x4_make(m00, m01, m02, m03),
            wasm_f32x4_make(m10, m11, m12, m13),
            wasm_f32x4_make(m20, m21, m22, m23),
            wasm_f32x4_make(m30, m31, m32, m33) }
{
}

VX_MATH_FORCEINLINE Matrix::Matrix(Vector row0, Vector row1, Vector row2, Vector row3) noexcept
    : data{ row0.Data(), row1.Data(), row2.Data(), row3.Data() }
{
}

VX_MATH_FORCEINLINE Matrix::Matrix(const Matrix& other) noexcept
    : data{ other.data[0], other.data[1], other.data[2], other.data[3] }
{
}

VX_MATH_FORCEINLINE Matrix::Matrix(Matrix&& other) noexcept
    : data{ other.data[0], other.data[1], other.data[2], other.data[3] }
{
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator=(const Matrix& other) noexcept
{
    if (this != &other)
    {
        data[0] = other.data[0];
        data[1] = other.data[1];
        data[2] = other.data[2];
        data[3] = other.data[3];
    }
    return *this;
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator=(Matrix&& other) noexcept
{
    return (*this = other); // trivially-copyable payload, plain copy is the move
}

VX_MATH_FORCEINLINE Vector Matrix::GetRow(size_t index) const noexcept
{
    return Vector{ data[index] };
}

VX_MATH_FORCEINLINE void Matrix::SetRow(size_t index, Vector row) noexcept
{
    data[index] = row.Data();
}

// NOTE: wasm_f32x4_extract_lane/replace_lane require a *compile-time
// constant* lane index (it's encoded as an immediate in the instruction),
// so runtime row/col indices below go through a local array (store/load)
// instead - the same trick the DirectX backend uses via reinterpret_cast.
VX_MATH_FORCEINLINE Vector Matrix::GetColumn(size_t index) const noexcept
{
    alignas(16) float r0[4], r1[4], r2[4], r3[4];
    wasm_v128_store(r0, data[0]);
    wasm_v128_store(r1, data[1]);
    wasm_v128_store(r2, data[2]);
    wasm_v128_store(r3, data[3]);
    return Vector{ r0[index], r1[index], r2[index], r3[index] };
}

VX_MATH_FORCEINLINE void Matrix::SetColumn(size_t index, Vector column) noexcept
{
    alignas(16) float col[4];
    wasm_v128_store(col, column.Data());
    for (int r = 0; r < 4; ++r)
    {
        alignas(16) float lanes[4];
        wasm_v128_store(lanes, data[r]);
        lanes[index] = col[r];
        data[r] = wasm_v128_load(lanes);
    }
}

VX_MATH_FORCEINLINE float Matrix::operator[](size_t row, size_t col) const noexcept
{
    alignas(16) float lanes[4];
    wasm_v128_store(lanes, data[row]);
    return lanes[col];
}

VX_MATH_FORCEINLINE void Matrix::SetElement(size_t row, size_t col, float value) noexcept
{
    alignas(16) float lanes[4];
    wasm_v128_store(lanes, data[row]);
    lanes[col] = value;
    data[row] = wasm_v128_load(lanes);
}

VX_MATH_FORCEINLINE Matrix Matrix::operator+(const Matrix& rhs) const noexcept
{
    return Matrix{ wasm_f32x4_add(data[0], rhs.data[0]),
                   wasm_f32x4_add(data[1], rhs.data[1]),
                   wasm_f32x4_add(data[2], rhs.data[2]),
                   wasm_f32x4_add(data[3], rhs.data[3]) };
}

VX_MATH_FORCEINLINE Matrix Matrix::operator-(const Matrix& rhs) const noexcept
{
    return Matrix{ wasm_f32x4_sub(data[0], rhs.data[0]),
                   wasm_f32x4_sub(data[1], rhs.data[1]),
                   wasm_f32x4_sub(data[2], rhs.data[2]),
                   wasm_f32x4_sub(data[3], rhs.data[3]) };
}

namespace detail
{
    // Row-vector * row-major matrix multiply for a single row:
    // result = sum_i row[i] * mat.row(i)
    VX_MATH_FORCEINLINE v128_t MulRowByMatrix(v128_t row, const v128_t m[4]) noexcept
    {
#if defined(VX_MATH_RELAXED_FMA)
        v128_t r = wasm_f32x4_mul(wasm_i32x4_shuffle(row, row, 0, 0, 0, 0), m[0]);
        r = wasm_f32x4_relaxed_madd(wasm_i32x4_shuffle(row, row, 1, 1, 1, 1), m[1], r);
        r = wasm_f32x4_relaxed_madd(wasm_i32x4_shuffle(row, row, 2, 2, 2, 2), m[2], r);
        r = wasm_f32x4_relaxed_madd(wasm_i32x4_shuffle(row, row, 3, 3, 3, 3), m[3], r);
        return r;
#else
        v128_t r = wasm_f32x4_mul(wasm_i32x4_shuffle(row, row, 0, 0, 0, 0), m[0]);
        r = wasm_f32x4_add(r, wasm_f32x4_mul(wasm_i32x4_shuffle(row, row, 1, 1, 1, 1), m[1]));
        r = wasm_f32x4_add(r, wasm_f32x4_mul(wasm_i32x4_shuffle(row, row, 2, 2, 2, 2), m[2]));
        r = wasm_f32x4_add(r, wasm_f32x4_mul(wasm_i32x4_shuffle(row, row, 3, 3, 3, 3), m[3]));
        return r;
#endif
    }
} // namespace detail

VX_MATH_FORCEINLINE Matrix Matrix::operator*(const Matrix& rhs) const noexcept
{
    return Matrix{ detail::MulRowByMatrix(data[0], rhs.data),
                   detail::MulRowByMatrix(data[1], rhs.data),
                   detail::MulRowByMatrix(data[2], rhs.data),
                   detail::MulRowByMatrix(data[3], rhs.data) };
}

VX_MATH_FORCEINLINE Matrix Matrix::operator*(float scalar) const noexcept
{
    v128_t s = wasm_f32x4_splat(scalar);
    return Matrix{ wasm_f32x4_mul(data[0], s),
                   wasm_f32x4_mul(data[1], s),
                   wasm_f32x4_mul(data[2], s),
                   wasm_f32x4_mul(data[3], s) };
}

VX_MATH_FORCEINLINE Matrix Matrix::operator-() const noexcept
{
    return Matrix{
        wasm_f32x4_neg(data[0]), wasm_f32x4_neg(data[1]), wasm_f32x4_neg(data[2]), wasm_f32x4_neg(data[3])
    };
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator+=(const Matrix& rhs) noexcept
{
    data[0] = wasm_f32x4_add(data[0], rhs.data[0]);
    data[1] = wasm_f32x4_add(data[1], rhs.data[1]);
    data[2] = wasm_f32x4_add(data[2], rhs.data[2]);
    data[3] = wasm_f32x4_add(data[3], rhs.data[3]);
    return *this;
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator-=(const Matrix& rhs) noexcept
{
    data[0] = wasm_f32x4_sub(data[0], rhs.data[0]);
    data[1] = wasm_f32x4_sub(data[1], rhs.data[1]);
    data[2] = wasm_f32x4_sub(data[2], rhs.data[2]);
    data[3] = wasm_f32x4_sub(data[3], rhs.data[3]);
    return *this;
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator*=(const Matrix& rhs) noexcept
{
    *this = *this * rhs;
    return *this;
}

VX_MATH_FORCEINLINE Matrix& Matrix::operator*=(float scalar) noexcept
{
    v128_t s = wasm_f32x4_splat(scalar);
    data[0] = wasm_f32x4_mul(data[0], s);
    data[1] = wasm_f32x4_mul(data[1], s);
    data[2] = wasm_f32x4_mul(data[2], s);
    data[3] = wasm_f32x4_mul(data[3], s);
    return *this;
}

VX_MATH_FORCEINLINE Vector Matrix::operator*(Vector vec) const noexcept
{
    return Vector{ detail::MulRowByMatrix(vec.Data(), data) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Transpose() const noexcept
{
    // Standard 4x4 SIMD transpose (the _MM_TRANSPOSE4_PS pattern).
    v128_t tmp0 = wasm_i32x4_shuffle(data[0], data[1], 0, 4, 1, 5);
    v128_t tmp1 = wasm_i32x4_shuffle(data[2], data[3], 0, 4, 1, 5);
    v128_t tmp2 = wasm_i32x4_shuffle(data[0], data[1], 2, 6, 3, 7);
    v128_t tmp3 = wasm_i32x4_shuffle(data[2], data[3], 2, 6, 3, 7);
    return Matrix{ wasm_i32x4_shuffle(tmp0, tmp1, 0, 1, 4, 5),
                   wasm_i32x4_shuffle(tmp0, tmp1, 2, 3, 6, 7),
                   wasm_i32x4_shuffle(tmp2, tmp3, 0, 1, 4, 5),
                   wasm_i32x4_shuffle(tmp2, tmp3, 2, 3, 6, 7) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Inverse() const noexcept
{
    alignas(16) float m[4][4];
    wasm_v128_store(m[0], data[0]);
    wasm_v128_store(m[1], data[1]);
    wasm_v128_store(m[2], data[2]);
    wasm_v128_store(m[3], data[3]);

    float inv[4][4];
    if (!detail::InvertMatrix4x4(m, inv))
    {
        // Mirrors DirectXMath's XMMatrixInverse behavior on a singular
        // (zero-determinant) matrix: all elements become +infinity.
        v128_t inf = wasm_f32x4_splat(std::numeric_limits<float>::infinity());
        return Matrix{ inf, inf, inf, inf };
    }

    return Matrix{
        wasm_v128_load(inv[0]), wasm_v128_load(inv[1]), wasm_v128_load(inv[2]), wasm_v128_load(inv[3])
    };
}

VX_MATH_FORCEINLINE float Matrix::Determinant() const noexcept
{
    alignas(16) float m[4][4];
    wasm_v128_store(m[0], data[0]);
    wasm_v128_store(m[1], data[1]);
    wasm_v128_store(m[2], data[2]);
    wasm_v128_store(m[3], data[3]);
    return detail::Determinant4x4(m);
}

// Static factory methods for transformations
VX_MATH_FORCEINLINE Matrix Matrix::Translation(Vector translation) noexcept
{
    return Translation(translation.x(), translation.y(), translation.z());
}

VX_MATH_FORCEINLINE Matrix Matrix::Translation(float x, float y, float z) noexcept
{
    return Matrix{ wasm_f32x4_make(1.0f, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 1.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 1.0f, 0.0f),
                   wasm_f32x4_make(x, y, z, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Scale(Vector scale) noexcept
{
    return Scale(scale.x(), scale.y(), scale.z());
}

VX_MATH_FORCEINLINE Matrix Matrix::Scale(float x, float y, float z) noexcept
{
    return Matrix{ wasm_f32x4_make(x, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, y, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, z, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Scale(float uniform_scale) noexcept
{
    return Scale(uniform_scale, uniform_scale, uniform_scale);
}

// Row-vector, row-major rotation matrices (matching DirectXMath's
// XMMatrixRotationX/Y/Z convention).
VX_MATH_FORCEINLINE Matrix Matrix::RotationX(float radians) noexcept
{
    float s = std::sin(radians), c = std::cos(radians);
    return Matrix{ wasm_f32x4_make(1.0f, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, c, s, 0.0f),
                   wasm_f32x4_make(0.0f, -s, c, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::RotationY(float radians) noexcept
{
    float s = std::sin(radians), c = std::cos(radians);
    return Matrix{ wasm_f32x4_make(c, 0.0f, -s, 0.0f),
                   wasm_f32x4_make(0.0f, 1.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(s, 0.0f, c, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::RotationZ(float radians) noexcept
{
    float s = std::sin(radians), c = std::cos(radians);
    return Matrix{ wasm_f32x4_make(c, s, 0.0f, 0.0f),
                   wasm_f32x4_make(-s, c, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 1.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

// Rodrigues' rotation formula, row-vector convention.
VX_MATH_FORCEINLINE Matrix Matrix::RotationAxis(Vector axis, float radians) noexcept
{
    Vector n = axis.Normalize<3>();
    float x = n.x(), y = n.y(), z = n.z();
    float c = std::cos(radians), s = std::sin(radians), t = 1.0f - c;

    return Matrix{ wasm_f32x4_make(t * x * x + c, t * x * y + s * z, t * x * z - s * y, 0.0f),
                   wasm_f32x4_make(t * x * y - s * z, t * y * y + c, t * y * z + s * x, 0.0f),
                   wasm_f32x4_make(t * x * z + s * y, t * y * z - s * x, t * z * z + c, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

// Standard quaternion-to-matrix, row-vector convention (matches
// DirectXMath's XMMatrixRotationQuaternion).
VX_MATH_FORCEINLINE Matrix Matrix::RotationQuaternion(Vector quaternion) noexcept
{
    float x = quaternion.x(), y = quaternion.y(), z = quaternion.z(), w = quaternion.w();

    return Matrix{
        wasm_f32x4_make(1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + z * w), 2.0f * (x * z - y * w), 0.0f),
        wasm_f32x4_make(2.0f * (x * y - z * w), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + x * w), 0.0f),
        wasm_f32x4_make(2.0f * (x * z + y * w), 2.0f * (y * z - x * w), 1.0f - 2.0f * (x * x + y * y), 0.0f),
        wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f)
    };
}

VX_MATH_FORCEINLINE Matrix Matrix::TRS(Vector translation, Vector rotation_quaternion, Vector scale) noexcept
{
    Matrix scale_matrix = Matrix::Scale(scale);
    Matrix rotation_matrix = Matrix::RotationQuaternion(rotation_quaternion);
    Matrix translation_matrix = Matrix::Translation(translation);
    return (scale_matrix * rotation_matrix) * translation_matrix;
}

VX_MATH_FORCEINLINE Matrix Matrix::LookAt(Vector eye, Vector target, Vector up) noexcept
{
    Vector zaxis = (eye - target).Normalize<3>();
    Vector xaxis = up.Cross(zaxis).Normalize<3>();
    Vector yaxis = zaxis.Cross(xaxis);

    return Matrix{ wasm_f32x4_make(xaxis.x(), yaxis.x(), zaxis.x(), 0.0f),
                   wasm_f32x4_make(xaxis.y(), yaxis.y(), zaxis.y(), 0.0f),
                   wasm_f32x4_make(xaxis.z(), yaxis.z(), zaxis.z(), 0.0f),
                   wasm_f32x4_make(-xaxis.Dot<3>(eye), -yaxis.Dot<3>(eye), -zaxis.Dot<3>(eye), 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::LookTo(Vector eye, Vector direction, Vector up) noexcept
{
    Vector zaxis = (-direction).Normalize<3>();
    Vector xaxis = up.Cross(zaxis).Normalize<3>();
    Vector yaxis = zaxis.Cross(xaxis);

    return Matrix{ wasm_f32x4_make(xaxis.x(), yaxis.x(), zaxis.x(), 0.0f),
                   wasm_f32x4_make(xaxis.y(), yaxis.y(), zaxis.y(), 0.0f),
                   wasm_f32x4_make(xaxis.z(), yaxis.z(), zaxis.z(), 0.0f),
                   wasm_f32x4_make(-xaxis.Dot<3>(eye), -yaxis.Dot<3>(eye), -zaxis.Dot<3>(eye), 1.0f) };
}

// D3D/WebGPU-style depth range [0, 1] throughout, matching the original
// DirectXMath-backed source.
namespace detail
{
    VX_MATH_FORCEINLINE Matrix PerspectiveFovLH(float fov_y_radians,
                                                float aspect_ratio,
                                                float near_plane,
                                                float far_plane) noexcept
    {
        float height = 1.0f / std::tan(fov_y_radians * 0.5f);
        float width = height / aspect_ratio;
        float range = far_plane / (far_plane - near_plane);

        return Matrix{ wasm_f32x4_make(width, 0.0f, 0.0f, 0.0f),
                       wasm_f32x4_make(0.0f, height, 0.0f, 0.0f),
                       wasm_f32x4_make(0.0f, 0.0f, range, 1.0f),
                       wasm_f32x4_make(0.0f, 0.0f, -range * near_plane, 0.0f) };
    }
} // namespace detail

VX_MATH_FORCEINLINE Matrix Matrix::Perspective(float fov_y_radians,
                                               float aspect_ratio,
                                               float near_plane,
                                               float far_plane) noexcept
{
    return detail::PerspectiveFovLH(fov_y_radians, aspect_ratio, near_plane, far_plane);
}

VX_MATH_FORCEINLINE Matrix Matrix::PerspectiveLH(float fov_y_radians,
                                                 float aspect_ratio,
                                                 float near_plane,
                                                 float far_plane) noexcept
{
    return detail::PerspectiveFovLH(fov_y_radians, aspect_ratio, near_plane, far_plane);
}

VX_MATH_FORCEINLINE Matrix Matrix::PerspectiveRH(float fov_y_radians,
                                                 float aspect_ratio,
                                                 float near_plane,
                                                 float far_plane) noexcept
{
    float height = 1.0f / std::tan(fov_y_radians * 0.5f);
    float width = height / aspect_ratio;
    float range = far_plane / (near_plane - far_plane);

    return Matrix{ wasm_f32x4_make(width, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, height, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, range, -1.0f),
                   wasm_f32x4_make(0.0f, 0.0f, range * near_plane, 0.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Orthographic(float width,
                                                float height,
                                                float near_plane,
                                                float far_plane) noexcept
{
    return OrthographicLH(width, height, near_plane, far_plane);
}

VX_MATH_FORCEINLINE Matrix Matrix::OrthographicLH(float width,
                                                  float height,
                                                  float near_plane,
                                                  float far_plane) noexcept
{
    float range = 1.0f / (far_plane - near_plane);
    return Matrix{ wasm_f32x4_make(2.0f / width, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 2.0f / height, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, range, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, -range * near_plane, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::OrthographicRH(float width,
                                                  float height,
                                                  float near_plane,
                                                  float far_plane) noexcept
{
    float range = 1.0f / (near_plane - far_plane);
    return Matrix{ wasm_f32x4_make(2.0f / width, 0.0f, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 2.0f / height, 0.0f, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, range, 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, range * near_plane, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix Matrix::Identity() noexcept
{
    return Matrix{};
}

VX_MATH_FORCEINLINE Matrix Matrix::Zero() noexcept
{
    v128_t z = wasm_f32x4_splat(0.0f);
    return Matrix{ z, z, z, z };
}

VX_MATH_FORCEINLINE bool Matrix::IsIdentity() const noexcept
{
    Matrix identity{};
    return IsNearlyEqual(identity, 0.0f);
}

VX_MATH_FORCEINLINE bool Matrix::IsNearlyEqual(const Matrix& other, float epsilon) const noexcept
{
    v128_t eps = wasm_f32x4_splat(epsilon);
    for (int i = 0; i < 4; ++i)
    {
        v128_t diff = wasm_f32x4_abs(wasm_f32x4_sub(data[i], other.data[i]));
        v128_t cmp = wasm_f32x4_le(diff, eps);
        // 4-bit mask, one bit per lane; 0xF means all four lanes matched.
        if (wasm_i32x4_bitmask(cmp) != 0xF)
        {
            return false;
        }
    }
    return true;
}

// ================================
// Matrix Free Functions
// ================================

template<int N>
VX_MATH_FORCEINLINE Vector Transform(Vector vector, Matrix matrix) noexcept
{
    static_assert(N >= 2 && N <= 4, "Transform dimensionality must be 2, 3, or 4");
    if constexpr (N == 4)
    {
        return matrix * vector;
    }
    else
    {
        // TransformCoord semantics: treat vector as a point (w=1 going in),
        // then perspective-divide by the resulting w.
        alignas(16) float lanes[4];
        wasm_v128_store(lanes, vector.Data());
        lanes[3] = 1.0f;
        if constexpr (N == 2)
        {
            lanes[2] = 0.0f;
        }
        Vector point{ wasm_v128_load(lanes) };
        Vector result = matrix * point;
        float invW = 1.0f / result.w();
        return result * invW;
    }
}

VX_MATH_FORCEINLINE Vector TransformNormal(Vector normal, Matrix matrix) noexcept
{
    // Directions transform by the upper-left 3x3 only (w forced to 0 so
    // translation doesn't leak in).
    alignas(16) float lanes[4];
    wasm_v128_store(lanes, normal.Data());
    lanes[3] = 0.0f;
    return matrix * Vector{ wasm_v128_load(lanes) };
}

VX_MATH_FORCEINLINE Matrix operator*(float scalar, const Matrix& mat) noexcept
{
    return mat * scalar;
}

// Matrix conversion functions
VX_MATH_FORCEINLINE Matrix ToMatrix(const Float3x3& storage) noexcept
{
    const auto& m = storage.Data();
    return Matrix{ wasm_f32x4_make(m[0][0], m[0][1], m[0][2], 0.0f),
                   wasm_f32x4_make(m[1][0], m[1][1], m[1][2], 0.0f),
                   wasm_f32x4_make(m[2][0], m[2][1], m[2][2], 0.0f),
                   wasm_f32x4_make(0.0f, 0.0f, 0.0f, 1.0f) };
}

VX_MATH_FORCEINLINE Matrix ToMatrix(const Float4x3& storage) noexcept
{
    const auto& m = storage.Data();
    return Matrix{ wasm_f32x4_make(m[0][0], m[0][1], m[0][2], 0.0f),
                   wasm_f32x4_make(m[1][0], m[1][1], m[1][2], 0.0f),
                   wasm_f32x4_make(m[2][0], m[2][1], m[2][2], 0.0f),
                   wasm_f32x4_make(m[3][0], m[3][1], m[3][2], 1.0f) };
}

VX_MATH_FORCEINLINE Matrix ToMatrix(const Float4x4& storage) noexcept
{
    const auto& m = storage.Data();
    return Matrix{ wasm_f32x4_make(m[0][0], m[0][1], m[0][2], m[0][3]),
                   wasm_f32x4_make(m[1][0], m[1][1], m[1][2], m[1][3]),
                   wasm_f32x4_make(m[2][0], m[2][1], m[2][2], m[2][3]),
                   wasm_f32x4_make(m[3][0], m[3][1], m[3][2], m[3][3]) };
}

template<>
VX_MATH_FORCEINLINE Float3x3 FromMatrix(const Matrix& mat) noexcept
{
    return Float3x3(
        mat[0, 0], mat[0, 1], mat[0, 2], mat[1, 0], mat[1, 1], mat[1, 2], mat[2, 0], mat[2, 1], mat[2, 2]);
}

template<>
VX_MATH_FORCEINLINE Float4x3 FromMatrix(const Matrix& mat) noexcept
{
    return Float4x3(mat[0, 0],
                    mat[0, 1],
                    mat[0, 2],
                    mat[1, 0],
                    mat[1, 1],
                    mat[1, 2],
                    mat[2, 0],
                    mat[2, 1],
                    mat[2, 2],
                    mat[3, 0],
                    mat[3, 1],
                    mat[3, 2]);
}

template<>
VX_MATH_FORCEINLINE Float4x4 FromMatrix(const Matrix& mat) noexcept
{
    return Float4x4(mat[0, 0],
                    mat[0, 1],
                    mat[0, 2],
                    mat[0, 3],
                    mat[1, 0],
                    mat[1, 1],
                    mat[1, 2],
                    mat[1, 3],
                    mat[2, 0],
                    mat[2, 1],
                    mat[2, 2],
                    mat[2, 3],
                    mat[3, 0],
                    mat[3, 1],
                    mat[3, 2],
                    mat[3, 3]);
}

} // namespace velox::math
