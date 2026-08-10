#pragma once
#ifndef VELOX_RHI_MATH_HPP
#define VELOX_RHI_MATH_HPP
#include <cstddef>
#include <cmath>
#include <cassert>
#include <limits>
#include <utility>

// needed early to prefix the swizzle accessors
#if defined(_MSC_VER)
#define VX_MATH_FORCEINLINE __forceinline
#elif defined(__clang__)
#define VX_MATH_FORCEINLINE __attribute__((always_inline)) inline
#elif defined(__GNUC__)
#define VX_MATH_FORCEINLINE __attribute__((always_inline)) inline
#else
#define VX_MATH_FORCEINLINE inline
#endif


// Autodetects Emscripten and selects the WASM SIMD128 backend; otherwise falls
// back to DirectXMath. Override with VX_MATH_FORCE_BACKEND_WASM or
// VX_MATH_FORCE_BACKEND_DIRECTX to force a specific backend
#if defined(VX_MATH_FORCE_BACKEND_WASM) && defined(VX_MATH_FORCE_BACKEND_DIRECTX)
    #error "Define at most one of VX_MATH_FORCE_BACKEND_WASM / VX_MATH_FORCE_BACKEND_DIRECTX"
#elif defined(VX_MATH_FORCE_BACKEND_WASM)
    #define VX_MATH_BACKEND_WASM 1
#elif defined(VX_MATH_FORCE_BACKEND_DIRECTX)
    #define VX_MATH_BACKEND_WASM 0
#elif defined(__EMSCRIPTEN__)
    #define VX_MATH_BACKEND_WASM 1
#else
    #define VX_MATH_BACKEND_WASM 0
#endif

#if VX_MATH_BACKEND_WASM
    #include <wasm_simd128.h>
#else
    #include <DirectXMath.h>
#endif

// Define VX_MATH_RELAXED_FMA to use relaxed-simd fused multiply-add
// (wasm_f32x4_relaxed_madd / relaxed_nmadd) on the WASM backend.
// this is mostly fully supported across browsers, but not 100% yet
// todo-ship: consider adding a runtime check for relaxed-simd support, and
// using that to select the backend at runtime. For now, just leave it disabled
#define VX_MATH_RELAXED_FMA

namespace velox::math
{
// Swizzle generation macros - these create combinations from storage members
#define SWIZZLE_2(a, b) \
    constexpr VX_MATH_FORCEINLINE Float2 a##b() const noexcept { return Float2(a, b); }

#define SWIZZLE_3(a, b, c) \
    constexpr VX_MATH_FORCEINLINE Float3 a##b##c() const noexcept { return Float3(a, b, c); }

#define SWIZZLE_4(a, b, c, d) \
    constexpr VX_MATH_FORCEINLINE Float4 a##b##c##d() const noexcept { return Float4(a, b, c, d); }

// Forward declarations
struct Float3;
struct Float4;
struct Vector;
struct Float3x3;
struct Float4x3;
struct Float4x4;
struct Matrix;

/**
 * This file defines unoptimized vector/matrix storage types (Float2/3/4,
 * Float3x3/4x3/4x4) alongside optimized SIMD types (Vector, Matrix). The
 * storage types are plain, backend-agnostic, and constexpr-friendly; they
 * can be kept around, stored, or transferred as needed. Vector/Matrix are
 * backed by either WASM SIMD128 (Emscripten) or DirectXMath, selected
 * above. If you need to do a lot of repeated math in a hot loop, convert
 * to the SIMD type via ToVector/ToMatrix, and back via FromVector/FromMatrix
 * when you're done.
 *
 * -----------------------------------------------------------------------
 * Removed / deferred during the WASM port (add back if/when needed):
 * -----------------------------------------------------------------------
 * - Integer vector types trimmed prior to this port
 * - Vector::ReciprocalEst / SqrtEst / ReciprocalSqrtEst, and the
 *   NormalizeEst<N>/LengthEst<N> that depended on them - WASM SIMD128 has no
 *   approximate sqrt/rsqrt intrinsic; would need a hand-rolled fast-inverse-sqrt.
 * - Vector::Refract<N> - no native intrinsic on the WASM side and
 *   comparatively rarely used
 * - Vector::Epsilon(), Vector::AlmostEqual(Vector) removed
 * - Vector::AlmostEqual(Vector, float) Vector::AlmostZero() also removed
 * -----------------------------------------------------------------------
 * Also worth flagging (pre-existing in the source, preserved as-is):
 * -----------------------------------------------------------------------
 * - Matrix::Perspective/Orthographic default to LH, but Matrix::LookAt/
 *   LookTo default to RH. That LH-projection / RH-view mismatch was
 *   already present in the original file; it's preserved here rather
 *   than silently "fixed", but it's worth deciding intentionally for a
 *   new WebGPU target rather than inheriting it by accident.
 */

struct Float2
{
public:
    // Constructors
    constexpr Float2() noexcept : x(0.0f), y(0.0f) {}
    constexpr Float2(float x, float y) noexcept : x(x), y(y) {}
    constexpr explicit Float2(float scalar) noexcept : x(scalar), y(scalar) {}

    constexpr Float2(const Float2& other) noexcept = default;
    constexpr Float2(Float2&& other) noexcept = default;
    constexpr Float2& operator=(const Float2& other) noexcept = default;
    constexpr Float2& operator=(Float2&& other) noexcept = default;

    // Array-style accessor (const + mutable collapsed via deducing this)
    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t index) noexcept
    {
        return (&self.x)[index];
    }

    // Arithmetic operators
    constexpr Float2 operator+(const Float2& rhs) const noexcept;
    constexpr Float2 operator-(const Float2& rhs) const noexcept;
    constexpr Float2 operator*(const Float2& rhs) const noexcept;
    constexpr Float2 operator/(const Float2& rhs) const noexcept;

    // Scalar operators
    constexpr Float2 operator*(float scalar) const noexcept;
    constexpr Float2 operator/(float scalar) const noexcept;

    // Unary operators
    constexpr Float2 operator-() const noexcept;

    // Compound assignment operators
    constexpr Float2& operator+=(const Float2& rhs) noexcept;
    constexpr Float2& operator-=(const Float2& rhs) noexcept;
    constexpr Float2& operator*=(const Float2& rhs) noexcept;
    constexpr Float2& operator/=(const Float2& rhs) noexcept;
    constexpr Float2& operator*=(float scalar) noexcept;
    constexpr Float2& operator/=(float scalar) noexcept;

    // Comparison operators
    constexpr bool operator==(const Float2& rhs) const noexcept;
    constexpr bool operator!=(const Float2& rhs) const noexcept;

    union
    {
        struct { float x, y; };
        struct { float u, v; };
    };
};

struct Float3
{
public:
    // Constructors
    constexpr Float3() noexcept : x(0.0f), y(0.0f), z(0.0f) {}
    constexpr Float3(float x, float y, float z) noexcept : x(x), y(y), z(z) {}
    constexpr explicit Float3(float scalar) noexcept : x(scalar), y(scalar), z(scalar) {}
    constexpr Float3(const Float2& xy, float z) noexcept : x(xy.x), y(xy.y), z(z) {}
    constexpr Float3(float x, const Float2& yz) noexcept : x(x), y(yz.x), z(yz.y) {}

    constexpr Float3(const Float3& other) noexcept = default;
    constexpr Float3(Float3&& other) noexcept = default;
    constexpr Float3& operator=(const Float3& other) noexcept = default;
    constexpr Float3& operator=(Float3&& other) noexcept = default;

    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t index) noexcept
    {
        return (&self.x)[index];
    }

    // Arithmetic operators
    constexpr Float3 operator+(const Float3& rhs) const noexcept;
    constexpr Float3 operator-(const Float3& rhs) const noexcept;
    constexpr Float3 operator*(const Float3& rhs) const noexcept;
    constexpr Float3 operator/(const Float3& rhs) const noexcept;

    // Scalar operators
    constexpr Float3 operator*(float scalar) const noexcept;
    constexpr Float3 operator/(float scalar) const noexcept;

    // Unary operators
    constexpr Float3 operator-() const noexcept;

    // Compound assignment operators
    constexpr Float3& operator+=(const Float3& rhs) noexcept;
    constexpr Float3& operator-=(const Float3& rhs) noexcept;
    constexpr Float3& operator*=(const Float3& rhs) noexcept;
    constexpr Float3& operator/=(const Float3& rhs) noexcept;
    constexpr Float3& operator*=(float scalar) noexcept;
    constexpr Float3& operator/=(float scalar) noexcept;

    // Comparison operators
    constexpr bool operator==(const Float3& rhs) const noexcept;
    constexpr bool operator!=(const Float3& rhs) const noexcept;

    SWIZZLE_3(x, y, z)
    SWIZZLE_3(x, z, y)
    SWIZZLE_3(y, x, z)
    SWIZZLE_3(y, z, x)
    SWIZZLE_3(z, x, y)
    SWIZZLE_3(z, y, x)
    SWIZZLE_3(x, x, x)
    SWIZZLE_3(y, y, y)
    SWIZZLE_3(z, z, z)
    SWIZZLE_2(x, y)
    SWIZZLE_2(x, z)
    SWIZZLE_2(y, z)

    union
    {
        struct { float x, y, z; };
        struct { float r, g, b; };
        struct { float u, v, w; };
    };
};

struct Float4
{
public:
    // Constructors
    constexpr Float4() noexcept : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr Float4(float x, float y, float z, float w) noexcept : x(x), y(y), z(z), w(w) {}
    constexpr explicit Float4(float scalar) noexcept : x(scalar), y(scalar), z(scalar), w(scalar) {}
    constexpr Float4(const Float3& xyz, float w) noexcept : x(xyz.x), y(xyz.y), z(xyz.z), w(w) {}
    constexpr Float4(float x, const Float3& yzw) noexcept : x(x), y(yzw.x), z(yzw.y), w(yzw.z) {}
    constexpr Float4(const Float2& xy, const Float2& zw) noexcept : x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
    constexpr Float4(const Float2& xy, float z, float w) noexcept : x(xy.x), y(xy.y), z(z), w(w) {}
    constexpr Float4(float x, const Float2& yz, float w) noexcept : x(x), y(yz.x), z(yz.y), w(w) {}
    constexpr Float4(float x, float y, const Float2& zw) noexcept : x(x), y(y), z(zw.x), w(zw.y) {}

    constexpr Float4(const Float4& other) noexcept = default;
    constexpr Float4(Float4&& other) noexcept = default;
    constexpr Float4& operator=(const Float4& other) noexcept = default;
    constexpr Float4& operator=(Float4&& other) noexcept = default;

    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t index) noexcept
    {
        return (&self.x)[index];
    }

    // Arithmetic operators
    constexpr Float4 operator+(const Float4& rhs) const noexcept;
    constexpr Float4 operator-(const Float4& rhs) const noexcept;
    constexpr Float4 operator*(const Float4& rhs) const noexcept;
    constexpr Float4 operator/(const Float4& rhs) const noexcept;

    // Scalar operators
    constexpr Float4 operator*(float scalar) const noexcept;
    constexpr Float4 operator/(float scalar) const noexcept;

    // Unary operators
    constexpr Float4 operator-() const noexcept;

    // Compound assignment operators
    constexpr Float4& operator+=(const Float4& rhs) noexcept;
    constexpr Float4& operator-=(const Float4& rhs) noexcept;
    constexpr Float4& operator*=(const Float4& rhs) noexcept;
    constexpr Float4& operator/=(const Float4& rhs) noexcept;
    constexpr Float4& operator*=(float scalar) noexcept;
    constexpr Float4& operator/=(float scalar) noexcept;

    // Comparison operators
    constexpr bool operator==(const Float4& rhs) const noexcept;
    constexpr bool operator!=(const Float4& rhs) const noexcept;

    // Swizzle accessors - 4D permutations. will expand if missing any later
    SWIZZLE_4(x, y, z, w)
    SWIZZLE_4(x, y, w, z)
    SWIZZLE_4(x, z, y, w)
    SWIZZLE_4(x, z, w, y)
    SWIZZLE_4(x, w, y, z)
    SWIZZLE_4(x, w, z, y)
    SWIZZLE_4(w, x, y, z)
    SWIZZLE_4(x, x, x, x)
    SWIZZLE_4(y, y, y, y)
    SWIZZLE_4(z, z, z, z)
    SWIZZLE_4(w, w, w, w)
    SWIZZLE_3(x, y, z)
    SWIZZLE_3(r, g, b)
    SWIZZLE_2(x, y)
    SWIZZLE_2(z, w)

    union
    {
        struct { float x, y, z, w; };
        struct { float r, g, b, a; };
        struct { float u, v, s, t; };
    };
};

/**
 * SIMD Vector type - optimized for mathematical operations, backed by
 * either WASM SIMD128 (Emscripten builds) or DirectXMath (native builds).
 * This type should NOT be stored or persisted. Convert to storage types
 * (Float2/3/4) at the end of mathematical operations, and from storage
 * types at the beginning, via ToVector/FromVector.
 *
 * All operations pass and return by value for optimal SIMD performance.
 * 
 * Operations like Normalize() and Length() are templated on number of
 * input components: make sure to get this right and be explicit about
 * your vector widths
 */
struct alignas(16) Vector
{
public:
    // Constructors
    Vector() noexcept;
    Vector(float x, float y, float z, float w) noexcept;
    Vector(float x, float y, float z) noexcept;
    Vector(float x, float y) noexcept;
    explicit Vector(float scalar) noexcept;
#if VX_MATH_BACKEND_WASM
    Vector(v128_t vec) noexcept : data{vec} {}
#else
    Vector(DirectX::XMVECTOR vec) noexcept : data{vec} {}
#endif

    Vector(const Vector& other) noexcept = default;
    Vector(Vector&& other) noexcept = default;
    Vector& operator=(const Vector& other) noexcept = default;
    Vector& operator=(Vector&& other) noexcept = default;

    // Native SIMD interop
#if VX_MATH_BACKEND_WASM
    v128_t Data() const noexcept { return data; }
#else
    DirectX::XMVECTOR Data() const noexcept { return data; }
#endif

    /** @note Accessing a single scalar value in this vector type is comparatively slow */
    float x() const noexcept;
    /** @note Accessing a single scalar value in this vector type is comparatively slow */
    float y() const noexcept;
    /** @note Accessing a single scalar value in this vector type is comparatively slow */
    float z() const noexcept;
    /** @note Accessing a single scalar value in this vector type is comparatively slow */
    float w() const noexcept;

    // Arithmetic operators
    Vector operator+(Vector rhs) const noexcept;
    Vector operator-(Vector rhs) const noexcept;
    Vector operator*(Vector rhs) const noexcept;
    Vector operator/(Vector rhs) const noexcept;
    Vector operator*(float scalar) const noexcept;
    Vector operator/(float scalar) const noexcept;
    Vector operator-() const noexcept;

    // Gated on VX_MATH_RELAXED_FMA (see top of file); always available, the
    // define only changes which instruction it lowers to on the WASM backend.
    Vector MultiplyAdd(Vector factor, Vector addend) const noexcept;

    // Compound assignment operators
    Vector& operator+=(Vector rhs) noexcept;
    Vector& operator-=(Vector rhs) noexcept;
    Vector& operator*=(Vector rhs) noexcept;
    Vector& operator/=(Vector rhs) noexcept;
    Vector& operator*=(float scalar) noexcept;
    Vector& operator/=(float scalar) noexcept;

    Vector Reciprocal() const noexcept;
    Vector Sqrt() const noexcept;
    Vector ReciprocalSqrt() const noexcept;

    template<int N>
    Vector Normalize() const noexcept;

    template<int N>
    float Length() const noexcept;

    template<int N>
    float LengthSq() const noexcept;

    // Cross product only makes sense for 3D vectors
    Vector Cross(Vector other) const noexcept;

    template<int N>
    float Dot(Vector other) const noexcept;

    Vector Lerp(Vector target, float t) const noexcept;

    template<int N>
    Vector Reflect(Vector normal) const noexcept;

    Vector Clamp(Vector min, Vector max) const noexcept;
    Vector Saturate() const noexcept;
    Vector Abs() const noexcept;
    Vector Min(Vector other) const noexcept;
    Vector Max(Vector other) const noexcept;
    Vector Pow(float exponent) const noexcept;
    Vector Pow(Vector exponent) const noexcept;

    static Vector Replicate(float scalar) noexcept;
    static Vector Zero() noexcept;
    static Vector Identity() noexcept;
    static Vector Abs(Vector vec) noexcept;
    static Vector Pow(Vector base, float exponent) noexcept;
    static Vector Pow(Vector base, Vector exponent) noexcept;

private:
#if VX_MATH_BACKEND_WASM
    v128_t data;
#else
    DirectX::XMVECTOR data;
#endif
};

/**
 * Matrix3x3 storage type for persistence and interop.
 * @note You cannot perform mathematical operations directly on these types, you
 * must convert them to the SIMD Matrix type first via ToMatrix(). This is an
 * intentional design choice!
 */
struct Float3x3
{
public:
    // Constructors
    constexpr Float3x3() noexcept : m{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    } {}

    constexpr Float3x3(
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22
    ) noexcept : m{
        {m00, m01, m02},
        {m10, m11, m12},
        {m20, m21, m22}
    } {}

    constexpr Float3x3(const Float3& row0, const Float3& row1, const Float3& row2) noexcept
        : m{
            {row0.x, row0.y, row0.z},
            {row1.x, row1.y, row1.z},
            {row2.x, row2.y, row2.z}
        } {}

    // Matrix conversion constructors
    // Extracts upper-left 3x3 portion from 4x4 matrix (useful for removing translation from view matrices for skybox rendering)
    explicit constexpr Float3x3(const Float4x4& mat4x4) noexcept;

    constexpr Float3x3(const Float3x3& other) noexcept = default;
    constexpr Float3x3(Float3x3&& other) noexcept = default;
    constexpr Float3x3& operator=(const Float3x3& other) noexcept = default;
    constexpr Float3x3& operator=(Float3x3&& other) noexcept = default;

    // C++23 multidimensional subscript operator; const/non-const collapsed via deducing this.
    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t row, size_t col) noexcept
    {
        assert(row < 3 && col < 3);
        return self.m[row][col];
    }

    constexpr Float3 Row(size_t index) const noexcept;
    constexpr void SetRow(size_t index, const Float3& row) noexcept;

    constexpr Float3 Column(size_t index) const noexcept;
    constexpr void SetColumn(size_t index, const Float3& column) noexcept;

    template <class Self>
    constexpr auto&& Data(this Self&& self) noexcept
    {
        return self.m;
    }

    constexpr bool operator==(const Float3x3& rhs) const noexcept;
    constexpr bool operator!=(const Float3x3& rhs) const noexcept;

    // Static factory methods
    static constexpr Float3x3 Identity() noexcept;
    static constexpr Float3x3 Zero() noexcept;

private:
    float m[3][3];
};

/**
 * Matrix4x3 storage type for persistence and interop.
 * 4x3 matrices are commonly used for affine transformations, those without perspective.
 * Row 3 is the translation row; rows 0-2 are the linear (rotation/scale) part.
 * @note You cannot perform mathematical operations directly on these types, you
 * must convert them to the SIMD Matrix type first via ToMatrix(). This is an
 * intentional design choice!
 */
struct Float4x3
{
public:
    constexpr Float4x3() noexcept : m{
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f}
    } {}

    constexpr Float4x3(
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22,
        float m30, float m31, float m32
    ) noexcept : m{
        {m00, m01, m02},
        {m10, m11, m12},
        {m20, m21, m22},
        {m30, m31, m32}
    } {}

    constexpr Float4x3(const Float3& row0, const Float3& row1, const Float3& row2, const Float3& row3) noexcept
        : m{
            {row0.x, row0.y, row0.z},
            {row1.x, row1.y, row1.z},
            {row2.x, row2.y, row2.z},
            {row3.x, row3.y, row3.z}
        } {}

    constexpr Float4x3(const Float4x3& other) noexcept = default;
    constexpr Float4x3(Float4x3&& other) noexcept = default;
    constexpr Float4x3& operator=(const Float4x3& other) noexcept = default;
    constexpr Float4x3& operator=(Float4x3&& other) noexcept = default;

    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t row, size_t col) noexcept
    {
        assert(row < 4 && col < 3);
        return self.m[row][col];
    }

    constexpr Float3 Row(size_t index) const noexcept;
    constexpr void SetRow(size_t index, const Float3& row) noexcept;

    constexpr Float4 Column(size_t index) const noexcept;
    constexpr void SetColumn(size_t index, const Float4& column) noexcept;

    template <class Self>
    constexpr auto&& Data(this Self&& self) noexcept
    {
        return self.m;
    }

    constexpr bool operator==(const Float4x3& rhs) const noexcept;
    constexpr bool operator!=(const Float4x3& rhs) const noexcept;

    static constexpr Float4x3 Identity() noexcept;
    static constexpr Float4x3 Zero() noexcept;

private:
    float m[4][3];
};

/**
 * Matrix4x4 storage type for persistence and interop.
 * @note You cannot perform mathematical operations directly on these types, you
 * must convert them to the SIMD Matrix type first via ToMatrix(). This is an
 * intentional design choice!
 */
struct Float4x4
{
public:
    constexpr Float4x4() noexcept : m{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f}
    } {}

    constexpr Float4x4(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33
    ) noexcept : m{
        {m00, m01, m02, m03},
        {m10, m11, m12, m13},
        {m20, m21, m22, m23},
        {m30, m31, m32, m33}
    } {}

    constexpr Float4x4(const Float4& row0, const Float4& row1, const Float4& row2, const Float4& row3) noexcept
        : m{
            {row0.x, row0.y, row0.z, row0.w},
            {row1.x, row1.y, row1.z, row1.w},
            {row2.x, row2.y, row2.z, row2.w},
            {row3.x, row3.y, row3.z, row3.w}
        } {}

    // Matrix conversion constructors
    // Expands 3x3 matrix to 4x4 with identity translation and w component (useful for converting rotation/scale matrices to full transforms)
    explicit constexpr Float4x4(const Float3x3& mat3x3) noexcept;

    constexpr Float4x4(const Float4x4& other) noexcept = default;
    constexpr Float4x4(Float4x4&& other) noexcept = default;
    constexpr Float4x4& operator=(const Float4x4& other) noexcept = default;
    constexpr Float4x4& operator=(Float4x4&& other) noexcept = default;

    template <class Self>
    constexpr auto&& operator[](this Self&& self, size_t row, size_t col) noexcept
    {
        assert(row < 4 && col < 4);
        return self.m[row][col];
    }

    constexpr Float4 Row(size_t index) const noexcept;
    constexpr void SetRow(size_t index, const Float4& row) noexcept;

    constexpr Float4 Column(size_t index) const noexcept;
    constexpr void SetColumn(size_t index, const Float4& column) noexcept;

    template <class Self>
    constexpr auto&& Data(this Self&& self) noexcept
    {
        return self.m;
    }

    constexpr bool operator==(const Float4x4& rhs) const noexcept;
    constexpr bool operator!=(const Float4x4& rhs) const noexcept;

    static constexpr Float4x4 Identity() noexcept;
    static constexpr Float4x4 Zero() noexcept;

private:
    float m[4][4];
};

/**
 * SIMD Matrix type - optimized for mathematical operations, backed by either
 * WASM SIMD128 (Emscripten builds) or DirectXMath (native builds).
 * This type should NOT be stored or persisted. Convert to storage types
 * (Float3x3/4x3/4x4) at the end of mathematical operations and from storage
 * types at the beginning, via ToMatrix/FromMatrix.
 *
 * All operations pass and return by value for optimal SIMD performance.
 * Row-major storage, row-vector convention (v' = v * M), matching the
 * original DirectXMath-backed source this was ported from.
 */
struct alignas(16) Matrix
{
public:
    // Constructors
    Matrix() noexcept;
#if VX_MATH_BACKEND_WASM
    Matrix(v128_t row0, v128_t row1, v128_t row2, v128_t row3) noexcept : data{row0, row1, row2, row3} {}
#else
    Matrix(DirectX::XMMATRIX mat) noexcept : data{mat} {}
#endif
    // Construct from individual elements (4x4)
    Matrix(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33
    ) noexcept;

    // Construct from row vectors
    Matrix(Vector row0, Vector row1, Vector row2, Vector row3) noexcept;

    Matrix(const Matrix& other) noexcept;
    Matrix(Matrix&& other) noexcept;
    Matrix& operator=(const Matrix& other) noexcept;
    Matrix& operator=(Matrix&& other) noexcept;

    // Native SIMD interop
#if VX_MATH_BACKEND_WASM
    struct NativeType { v128_t r[4]; };
    NativeType Data() const noexcept { return NativeType{data[0], data[1], data[2], data[3]}; }
#else
    DirectX::XMMATRIX Data() const noexcept { return data; }
#endif

    // Row access
    Vector GetRow(size_t index) const noexcept;
    void SetRow(size_t index, Vector row) noexcept;

    // Column access
    Vector GetColumn(size_t index) const noexcept;
    void SetColumn(size_t index, Vector column) noexcept;

    // Element access. Read-only: SIMD lanes aren't addressable as float&,
    // so mutation goes through SetElement (matches the original API shape).
    float operator[](size_t row, size_t col) const noexcept;
    void SetElement(size_t row, size_t col, float value) noexcept;

    // Matrix arithmetic operations
    Matrix operator+(const Matrix& rhs) const noexcept;
    Matrix operator-(const Matrix& rhs) const noexcept;
    Matrix operator*(const Matrix& rhs) const noexcept;
    Matrix operator*(float scalar) const noexcept;
    Matrix operator-() const noexcept;

    // Compound assignment operators
    Matrix& operator+=(const Matrix& rhs) noexcept;
    Matrix& operator-=(const Matrix& rhs) noexcept;
    Matrix& operator*=(const Matrix& rhs) noexcept;
    Matrix& operator*=(float scalar) noexcept;

    // Matrix-vector operations
    Vector operator*(Vector vec) const noexcept;

    // Matrix operations
    Matrix Transpose() const noexcept;
    // Returns a matrix of +infinity in every element if the matrix is singular
    // (mirrors DirectXMath's XMMatrixInverse behavior on a zero determinant).
    Matrix Inverse() const noexcept;
    float Determinant() const noexcept;

    // Transformation matrices (static factory methods)
    static Matrix Translation(Vector translation) noexcept;
    static Matrix Translation(float x, float y, float z) noexcept;
    static Matrix Scale(Vector scale) noexcept;
    static Matrix Scale(float x, float y, float z) noexcept;
    static Matrix Scale(float uniform_scale) noexcept;

    // Rotation matrices
    static Matrix RotationX(float radians) noexcept;
    static Matrix RotationY(float radians) noexcept;
    static Matrix RotationZ(float radians) noexcept;
    static Matrix RotationAxis(Vector axis, float radians) noexcept;
    static Matrix RotationQuaternion(Vector quaternion) noexcept;

    // Combined transformations
    static Matrix TRS(Vector translation, Vector rotation_quaternion, Vector scale) noexcept;
    // NOTE: LookAt/LookTo default to a right-handed view space, while
    // Perspective/Orthographic below default to left-handed projections.
    // That mismatch was already present in the source this was ported
    // from; preserved as-is here rather than silently changed. Prefer
    // the explicit *LH/*RH variants if you want to be sure.
    static Matrix LookAt(Vector eye, Vector target, Vector up) noexcept;
    static Matrix LookTo(Vector eye, Vector direction, Vector up) noexcept;

    // Projection matrices (D3D/WebGPU-style depth range [0, 1])
    static Matrix Perspective(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept;
    static Matrix PerspectiveLH(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept;
    static Matrix PerspectiveRH(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept;
    static Matrix Orthographic(float width, float height, float near_plane, float far_plane) noexcept;
    static Matrix OrthographicLH(float width, float height, float near_plane, float far_plane) noexcept;
    static Matrix OrthographicRH(float width, float height, float near_plane, float far_plane) noexcept;

    // Utility matrices
    static Matrix Identity() noexcept;
    static Matrix Zero() noexcept;

    // Comparison utilities
    bool IsIdentity() const noexcept;
    bool IsNearlyEqual(const Matrix& other, float epsilon = 0.0001f) const noexcept;

private:
#if VX_MATH_BACKEND_WASM
    v128_t data[4];
#else
    DirectX::XMMATRIX data;
#endif
};

// Vector transformation functions that use matrix and vector types together
template<int N>
Vector Transform(Vector vector, Matrix matrix) noexcept;
Vector TransformNormal(Vector vector, Matrix matrix) noexcept;

// Free function scalar multiplication (scalar * vector) for storage types
constexpr Float2 operator*(float scalar, const Float2& vec) noexcept;
constexpr Float3 operator*(float scalar, const Float3& vec) noexcept;
constexpr Float4 operator*(float scalar, const Float4& vec) noexcept;

// Free function scalar multiplication for SIMD Vector and Matrix
Vector operator*(float scalar, Vector vec) noexcept;
Matrix operator*(float scalar, const Matrix& mat) noexcept;

// Matrix-Vector operations (mixing storage and SIMD types). See the
// "worth flagging" note above the storage types regarding the Float4x3
// variants' assumptions.
constexpr Float3 operator*(const Float3x3& mat, const Float3& vec) noexcept;
constexpr Float4 operator*(const Float4x3& mat, const Float3& vec) noexcept;
constexpr Float4 operator*(const Float4x3& mat, const Float4& vec) noexcept;
constexpr Float4 operator*(const Float4x4& mat, const Float4& vec) noexcept;
constexpr Float3 operator*(const Float4x4& mat, const Float3& vec) noexcept;

// Note: Matrix * Vector is handled by Matrix::operator*(Vector) member function

/**
 * @brief Convert Matrix3x3 storage type to SIMD Matrix
 * Use this function at the START of mathematical operations to convert from storage format
 */
Matrix ToMatrix(const Float3x3& storage) noexcept;

/**
 * @brief Convert Matrix4x3 storage type to SIMD Matrix
 * Use this function at the START of mathematical operations to convert from storage format
 */
Matrix ToMatrix(const Float4x3& storage) noexcept;

/**
 * @brief Convert Matrix4x4 storage type to SIMD Matrix
 * Use this function at the START of mathematical operations to convert from storage format
 */
Matrix ToMatrix(const Float4x4& storage) noexcept;

template<typename T>
T FromMatrix(const Matrix& mat) noexcept;

/**
 * @brief Convert SIMD Matrix to Matrix3x3 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float3x3 FromMatrix(const Matrix& mat) noexcept;

/**
 * @brief Convert SIMD Matrix to Matrix4x3 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float4x3 FromMatrix(const Matrix& mat) noexcept;

/**
 * @brief Convert SIMD Matrix to Matrix4x4 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float4x4 FromMatrix(const Matrix& mat) noexcept;

/**
 * @brief Convert Float2 storage type to SIMD Vector
 * Use this function at the START of mathematical operations to convert from storage format
 */
Vector ToVector(const Float2& storage) noexcept;

/**
 * @brief Convert Float3 storage type to SIMD Vector
 * Use this function at the START of mathematical operations to convert from storage format
 */
Vector ToVector(const Float3& storage) noexcept;

/**
 * @brief Convert Float4 storage type to SIMD Vector
 * Use this function at the START of mathematical operations to convert from storage format
 */
Vector ToVector(const Float4& storage) noexcept;

template<typename T>
T FromVector(Vector vec) noexcept;

/**
 * @brief Convert SIMD Vector to Float2 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float2 FromVector<Float2>(Vector vec) noexcept;

/**
 * @brief Convert SIMD Vector to Float3 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float3 FromVector<Float3>(Vector vec) noexcept;

/**
 * @brief Convert SIMD Vector to Float4 storage type
 * Use this function at the END of mathematical operations to convert back to storage format
 */
template<>
Float4 FromVector<Float4>(Vector vec) noexcept;

#undef SWIZZLE_2
#undef SWIZZLE_3
#undef SWIZZLE_4

} // namespace velox::math

#include "math/Math.inl"

#endif // !VELOX_RHI_MATH_HPP
