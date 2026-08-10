#pragma once
// DirectXMath backend for math::Vector / math::Matrix. Included by Math.inl
// when DD_MATH_BACKEND_WASM is 0 (native builds, or a forced override).
// Requires <DirectXMath.h>, already included by Math.hpp.

namespace math
{
    // ================================
    // Vector SIMD Implementation (DirectXMath)
    // ================================

    DD_MATH_FORCEINLINE Vector::Vector() noexcept : data{DirectX::XMVectorZero()} {}
    DD_MATH_FORCEINLINE Vector::Vector(float x, float y, float z, float w) noexcept : data{DirectX::XMVectorSet(x, y, z, w)} {}
    DD_MATH_FORCEINLINE Vector::Vector(float x, float y, float z) noexcept : data{DirectX::XMVectorSet(x, y, z, 0.0f)} {}
    DD_MATH_FORCEINLINE Vector::Vector(float x, float y) noexcept : data{DirectX::XMVectorSet(x, y, 0.0f, 0.0f)} {}
    DD_MATH_FORCEINLINE Vector::Vector(float scalar) noexcept : data{DirectX::XMVectorReplicate(scalar)} {}

    // Component accessors
    DD_MATH_FORCEINLINE float Vector::x() const noexcept { return DirectX::XMVectorGetX(data); }
    DD_MATH_FORCEINLINE float Vector::y() const noexcept { return DirectX::XMVectorGetY(data); }
    DD_MATH_FORCEINLINE float Vector::z() const noexcept { return DirectX::XMVectorGetZ(data); }
    DD_MATH_FORCEINLINE float Vector::w() const noexcept { return DirectX::XMVectorGetW(data); }

    // Static factory methods
    DD_MATH_FORCEINLINE Vector Vector::Zero() noexcept { return Vector{DirectX::XMVectorZero()}; }
    DD_MATH_FORCEINLINE Vector Vector::Replicate(float scalar) noexcept { return Vector{DirectX::XMVectorReplicate(scalar)}; }
    DD_MATH_FORCEINLINE Vector Vector::Identity() noexcept { return Vector{DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f)}; }

    // Arithmetic operators
    DD_MATH_FORCEINLINE Vector Vector::operator+(Vector rhs) const noexcept { return Vector{DirectX::XMVectorAdd(data, rhs.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator-(Vector rhs) const noexcept { return Vector{DirectX::XMVectorSubtract(data, rhs.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator*(Vector rhs) const noexcept { return Vector{DirectX::XMVectorMultiply(data, rhs.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator/(Vector rhs) const noexcept { return Vector{DirectX::XMVectorDivide(data, rhs.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator*(float scalar) const noexcept { return Vector{DirectX::XMVectorScale(data, scalar)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator/(float scalar) const noexcept { return Vector{DirectX::XMVectorScale(data, 1.0f / scalar)}; }
    DD_MATH_FORCEINLINE Vector Vector::operator-() const noexcept { return Vector{DirectX::XMVectorNegate(data)}; }

    // DD_MATH_RELAXED_FMA has no effect on this backend: XMVectorMultiplyAdd
    // already lowers to a hardware FMA instruction when the target/compiler
    // flags support it, with no separate "relaxed" mode to opt into.
    DD_MATH_FORCEINLINE Vector Vector::MultiplyAdd(Vector factor, Vector addend) const noexcept
    {
        return Vector{DirectX::XMVectorMultiplyAdd(data, factor.data, addend.data)};
    }

    // Compound assignment operators
    DD_MATH_FORCEINLINE Vector& Vector::operator+=(Vector rhs) noexcept { data = DirectX::XMVectorAdd(data, rhs.data); return *this; }
    DD_MATH_FORCEINLINE Vector& Vector::operator-=(Vector rhs) noexcept { data = DirectX::XMVectorSubtract(data, rhs.data); return *this; }
    DD_MATH_FORCEINLINE Vector& Vector::operator*=(Vector rhs) noexcept { data = DirectX::XMVectorMultiply(data, rhs.data); return *this; }
    DD_MATH_FORCEINLINE Vector& Vector::operator/=(Vector rhs) noexcept { data = DirectX::XMVectorDivide(data, rhs.data); return *this; }
    DD_MATH_FORCEINLINE Vector& Vector::operator*=(float scalar) noexcept { data = DirectX::XMVectorScale(data, scalar); return *this; }
    DD_MATH_FORCEINLINE Vector& Vector::operator/=(float scalar) noexcept { data = DirectX::XMVectorScale(data, 1.0f / scalar); return *this; }

    DD_MATH_FORCEINLINE Vector Vector::Reciprocal() const noexcept { return Vector{DirectX::XMVectorReciprocal(data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Sqrt() const noexcept { return Vector{DirectX::XMVectorSqrt(data)}; }
    DD_MATH_FORCEINLINE Vector Vector::ReciprocalSqrt() const noexcept { return Vector{DirectX::XMVectorReciprocalSqrt(data)}; }

    template<int N>
    DD_MATH_FORCEINLINE Vector Vector::Normalize() const noexcept
    {
        static_assert(N >= 2 && N <= 4, "Normalize dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return Vector{DirectX::XMVector2Normalize(data)}; }
        else if constexpr (N == 3) { return Vector{DirectX::XMVector3Normalize(data)}; }
        else { return Vector{DirectX::XMVector4Normalize(data)}; }
    }

    template<int N>
    DD_MATH_FORCEINLINE float Vector::Length() const noexcept
    {
        static_assert(N >= 2 && N <= 4, "Length dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return DirectX::XMVectorGetX(DirectX::XMVector2Length(data)); }
        else if constexpr (N == 3) { return DirectX::XMVectorGetX(DirectX::XMVector3Length(data)); }
        else { return DirectX::XMVectorGetX(DirectX::XMVector4Length(data)); }
    }

    template<int N>
    DD_MATH_FORCEINLINE float Vector::LengthSq() const noexcept
    {
        static_assert(N >= 2 && N <= 4, "LengthSq dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return DirectX::XMVectorGetX(DirectX::XMVector2LengthSq(data)); }
        else if constexpr (N == 3) { return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(data)); }
        else { return DirectX::XMVectorGetX(DirectX::XMVector4LengthSq(data)); }
    }

    // Cross product only makes sense for 3D vectors
    DD_MATH_FORCEINLINE Vector Vector::Cross(Vector other) const noexcept { return Vector{DirectX::XMVector3Cross(data, other.data)}; }

    template<int N>
    DD_MATH_FORCEINLINE float Vector::Dot(Vector other) const noexcept
    {
        static_assert(N >= 2 && N <= 4, "Dot dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return DirectX::XMVectorGetX(DirectX::XMVector2Dot(data, other.data)); }
        else if constexpr (N == 3) { return DirectX::XMVectorGetX(DirectX::XMVector3Dot(data, other.data)); }
        else { return DirectX::XMVectorGetX(DirectX::XMVector4Dot(data, other.data)); }
    }

    DD_MATH_FORCEINLINE Vector Vector::Lerp(Vector target, float t) const noexcept { return Vector{DirectX::XMVectorLerp(data, target.data, t)}; }

    template<int N>
    DD_MATH_FORCEINLINE Vector Vector::Reflect(Vector normal) const noexcept
    {
        static_assert(N >= 2 && N <= 4, "Reflect dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return Vector{DirectX::XMVector2Reflect(data, normal.data)}; }
        else if constexpr (N == 3) { return Vector{DirectX::XMVector3Reflect(data, normal.data)}; }
        else { return Vector{DirectX::XMVector4Reflect(data, normal.data)}; }
    }

    DD_MATH_FORCEINLINE Vector Vector::Clamp(Vector min, Vector max) const noexcept { return Vector{DirectX::XMVectorClamp(data, min.data, max.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Saturate() const noexcept { return Vector{DirectX::XMVectorSaturate(data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Abs() const noexcept { return Vector{DirectX::XMVectorAbs(data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Min(Vector other) const noexcept { return Vector{DirectX::XMVectorMin(data, other.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Max(Vector other) const noexcept { return Vector{DirectX::XMVectorMax(data, other.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Pow(float exponent) const noexcept { return Vector{DirectX::XMVectorPow(data, DirectX::XMVectorReplicate(exponent))}; }
    DD_MATH_FORCEINLINE Vector Vector::Pow(Vector exponent) const noexcept { return Vector{DirectX::XMVectorPow(data, exponent.data)}; }

    DD_MATH_FORCEINLINE Vector Vector::Abs(Vector vec) noexcept { return Vector{DirectX::XMVectorAbs(vec.data)}; }
    DD_MATH_FORCEINLINE Vector Vector::Pow(Vector base, float exponent) noexcept { return Vector{DirectX::XMVectorPow(base.data, DirectX::XMVectorReplicate(exponent))}; }
    DD_MATH_FORCEINLINE Vector Vector::Pow(Vector base, Vector exponent) noexcept { return Vector{DirectX::XMVectorPow(base.data, exponent.data)}; }

    // ================================
    // Free Function Implementations (Vector)
    // ================================

    DD_MATH_FORCEINLINE Vector operator*(float scalar, Vector vec) noexcept { return vec * scalar; }

    DD_MATH_FORCEINLINE Vector ToVector(const Float2& in) noexcept { return Vector{DirectX::XMVectorSet(in.x, in.y, 0.0f, 0.0f)}; }
    DD_MATH_FORCEINLINE Vector ToVector(const Float3& in) noexcept { return Vector{DirectX::XMVectorSet(in.x, in.y, in.z, 0.0f)}; }
    DD_MATH_FORCEINLINE Vector ToVector(const Float4& in) noexcept { return Vector{DirectX::XMVectorSet(in.x, in.y, in.z, in.w)}; }

    template<>
    DD_MATH_FORCEINLINE Float2 FromVector(Vector vec) noexcept
    {
        DirectX::XMFLOAT2 result;
        DirectX::XMStoreFloat2(&result, vec.Data());
        return Float2(result.x, result.y);
    }

    template<>
    DD_MATH_FORCEINLINE Float3 FromVector(Vector vec) noexcept
    {
        DirectX::XMFLOAT3 result;
        DirectX::XMStoreFloat3(&result, vec.Data());
        return Float3(result.x, result.y, result.z);
    }

    template<>
    DD_MATH_FORCEINLINE Float4 FromVector(Vector vec) noexcept
    {
        DirectX::XMFLOAT4 result;
        DirectX::XMStoreFloat4(&result, vec.Data());
        return Float4(result.x, result.y, result.z, result.w);
    }

    // ================================
    // SIMD Matrix Implementation (DirectXMath)
    // ================================

    DD_MATH_FORCEINLINE Matrix::Matrix() noexcept : data(DirectX::XMMatrixIdentity()) {}

    DD_MATH_FORCEINLINE Matrix::Matrix(
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33
    ) noexcept : data{DirectX::XMMatrixSet(
        m00, m01, m02, m03,
        m10, m11, m12, m13,
        m20, m21, m22, m23,
        m30, m31, m32, m33
    )}
    {
    }

    DD_MATH_FORCEINLINE Matrix::Matrix(Vector row0, Vector row1, Vector row2, Vector row3) noexcept
        : data{DirectX::XMMATRIX{row0.Data(), row1.Data(), row2.Data(), row3.Data()}}
    {
    }

    DD_MATH_FORCEINLINE Matrix::Matrix(const Matrix& other) noexcept : data{other.data} {}
    DD_MATH_FORCEINLINE Matrix::Matrix(Matrix&& other) noexcept : data{std::move(other.data)} {}

    DD_MATH_FORCEINLINE Matrix& Matrix::operator=(const Matrix& other) noexcept
    {
        if (this != &other) { data = other.data; }
        return *this;
    }

    DD_MATH_FORCEINLINE Matrix& Matrix::operator=(Matrix&& other) noexcept
    {
        if (this != &other) { data = std::move(other.data); }
        return *this;
    }

    DD_MATH_FORCEINLINE Vector Matrix::GetRow(size_t index) const noexcept { return Vector{data.r[index]}; }
    DD_MATH_FORCEINLINE void Matrix::SetRow(size_t index, Vector row) noexcept { data.r[index] = row.Data(); }

    DD_MATH_FORCEINLINE Vector Matrix::GetColumn(size_t index) const noexcept
    {
        return Vector{
            DirectX::XMVectorGetByIndex(data.r[0], index),
            DirectX::XMVectorGetByIndex(data.r[1], index),
            DirectX::XMVectorGetByIndex(data.r[2], index),
            DirectX::XMVectorGetByIndex(data.r[3], index)
        };
    }

    DD_MATH_FORCEINLINE void Matrix::SetColumn(size_t index, Vector column) noexcept
    {
        data.r[0] = DirectX::XMVectorSetByIndex(data.r[0], DirectX::XMVectorGetX(column.Data()), index);
        data.r[1] = DirectX::XMVectorSetByIndex(data.r[1], DirectX::XMVectorGetY(column.Data()), index);
        data.r[2] = DirectX::XMVectorSetByIndex(data.r[2], DirectX::XMVectorGetZ(column.Data()), index);
        data.r[3] = DirectX::XMVectorSetByIndex(data.r[3], DirectX::XMVectorGetW(column.Data()), index);
    }

    DD_MATH_FORCEINLINE float Matrix::operator[](size_t row, size_t col) const noexcept
    {
        const float* matrix_data = reinterpret_cast<const float*>(&data);
        return matrix_data[row * 4 + col];
    }

    DD_MATH_FORCEINLINE void Matrix::SetElement(size_t row, size_t col, float value) noexcept
    {
        float* matrix_data = reinterpret_cast<float*>(&data);
        matrix_data[row * 4 + col] = value;
    }

    DD_MATH_FORCEINLINE Matrix Matrix::operator+(const Matrix& rhs) const noexcept
    {
        return Matrix{DirectX::XMMATRIX{
            DirectX::XMVectorAdd(data.r[0], rhs.data.r[0]),
            DirectX::XMVectorAdd(data.r[1], rhs.data.r[1]),
            DirectX::XMVectorAdd(data.r[2], rhs.data.r[2]),
            DirectX::XMVectorAdd(data.r[3], rhs.data.r[3])
        }};
    }

    DD_MATH_FORCEINLINE Matrix Matrix::operator-(const Matrix& rhs) const noexcept
    {
        return Matrix{DirectX::XMMATRIX{
            DirectX::XMVectorSubtract(data.r[0], rhs.data.r[0]),
            DirectX::XMVectorSubtract(data.r[1], rhs.data.r[1]),
            DirectX::XMVectorSubtract(data.r[2], rhs.data.r[2]),
            DirectX::XMVectorSubtract(data.r[3], rhs.data.r[3])
        }};
    }

    DD_MATH_FORCEINLINE Matrix Matrix::operator*(const Matrix& rhs) const noexcept { return Matrix{DirectX::XMMatrixMultiply(data, rhs.data)}; }

    DD_MATH_FORCEINLINE Matrix Matrix::operator*(float scalar) const noexcept
    {
        return Matrix{DirectX::XMMATRIX{
            DirectX::XMVectorScale(data.r[0], scalar),
            DirectX::XMVectorScale(data.r[1], scalar),
            DirectX::XMVectorScale(data.r[2], scalar),
            DirectX::XMVectorScale(data.r[3], scalar)
        }};
    }

    DD_MATH_FORCEINLINE Matrix Matrix::operator-() const noexcept
    {
        return Matrix{DirectX::XMMATRIX{
            DirectX::XMVectorNegate(data.r[0]),
            DirectX::XMVectorNegate(data.r[1]),
            DirectX::XMVectorNegate(data.r[2]),
            DirectX::XMVectorNegate(data.r[3])
        }};
    }

    DD_MATH_FORCEINLINE Matrix& Matrix::operator+=(const Matrix& rhs) noexcept
    {
        data.r[0] = DirectX::XMVectorAdd(data.r[0], rhs.data.r[0]);
        data.r[1] = DirectX::XMVectorAdd(data.r[1], rhs.data.r[1]);
        data.r[2] = DirectX::XMVectorAdd(data.r[2], rhs.data.r[2]);
        data.r[3] = DirectX::XMVectorAdd(data.r[3], rhs.data.r[3]);
        return *this;
    }

    DD_MATH_FORCEINLINE Matrix& Matrix::operator-=(const Matrix& rhs) noexcept
    {
        data.r[0] = DirectX::XMVectorSubtract(data.r[0], rhs.data.r[0]);
        data.r[1] = DirectX::XMVectorSubtract(data.r[1], rhs.data.r[1]);
        data.r[2] = DirectX::XMVectorSubtract(data.r[2], rhs.data.r[2]);
        data.r[3] = DirectX::XMVectorSubtract(data.r[3], rhs.data.r[3]);
        return *this;
    }

    DD_MATH_FORCEINLINE Matrix& Matrix::operator*=(const Matrix& rhs) noexcept { data = DirectX::XMMatrixMultiply(data, rhs.data); return *this; }

    DD_MATH_FORCEINLINE Matrix& Matrix::operator*=(float scalar) noexcept
    {
        data.r[0] = DirectX::XMVectorScale(data.r[0], scalar);
        data.r[1] = DirectX::XMVectorScale(data.r[1], scalar);
        data.r[2] = DirectX::XMVectorScale(data.r[2], scalar);
        data.r[3] = DirectX::XMVectorScale(data.r[3], scalar);
        return *this;
    }

    DD_MATH_FORCEINLINE Vector Matrix::operator*(Vector vec) const noexcept { return Vector{DirectX::XMVector4Transform(vec.Data(), data)}; }

    DD_MATH_FORCEINLINE Matrix Matrix::Transpose() const noexcept { return Matrix{DirectX::XMMatrixTranspose(data)}; }

    DD_MATH_FORCEINLINE Matrix Matrix::Inverse() const noexcept
    {
        DirectX::XMVECTOR determinant;
        return Matrix{DirectX::XMMatrixInverse(&determinant, data)};
    }

    DD_MATH_FORCEINLINE float Matrix::Determinant() const noexcept { return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(data)); }

    // Static factory methods for transformations
    DD_MATH_FORCEINLINE Matrix Matrix::Translation(Vector translation) noexcept { return Matrix{DirectX::XMMatrixTranslationFromVector(translation.Data())}; }
    DD_MATH_FORCEINLINE Matrix Matrix::Translation(float x, float y, float z) noexcept { return Matrix{DirectX::XMMatrixTranslation(x, y, z)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::Scale(Vector scale) noexcept { return Matrix{DirectX::XMMatrixScalingFromVector(scale.Data())}; }
    DD_MATH_FORCEINLINE Matrix Matrix::Scale(float x, float y, float z) noexcept { return Matrix{DirectX::XMMatrixScaling(x, y, z)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::Scale(float uniform_scale) noexcept { return Matrix{DirectX::XMMatrixScaling(uniform_scale, uniform_scale, uniform_scale)}; }

    DD_MATH_FORCEINLINE Matrix Matrix::RotationX(float radians) noexcept { return Matrix{DirectX::XMMatrixRotationX(radians)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::RotationY(float radians) noexcept { return Matrix{DirectX::XMMatrixRotationY(radians)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::RotationZ(float radians) noexcept { return Matrix{DirectX::XMMatrixRotationZ(radians)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::RotationAxis(Vector axis, float radians) noexcept { return Matrix{DirectX::XMMatrixRotationAxis(axis.Data(), radians)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::RotationQuaternion(Vector quaternion) noexcept { return Matrix{DirectX::XMMatrixRotationQuaternion(quaternion.Data())}; }

    DD_MATH_FORCEINLINE Matrix Matrix::TRS(Vector translation, Vector rotation_quaternion, Vector scale) noexcept
    {
        DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScalingFromVector(scale.Data());
        DirectX::XMMATRIX rotation_matrix = DirectX::XMMatrixRotationQuaternion(rotation_quaternion.Data());
        DirectX::XMMATRIX translation_matrix = DirectX::XMMatrixTranslationFromVector(translation.Data());
        return Matrix{DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(scale_matrix, rotation_matrix), translation_matrix)};
    }

    DD_MATH_FORCEINLINE Matrix Matrix::LookAt(Vector eye, Vector target, Vector up) noexcept { return Matrix{DirectX::XMMatrixLookAtRH(eye.Data(), target.Data(), up.Data())}; }
    DD_MATH_FORCEINLINE Matrix Matrix::LookTo(Vector eye, Vector direction, Vector up) noexcept { return Matrix{DirectX::XMMatrixLookToRH(eye.Data(), direction.Data(), up.Data())}; }

    DD_MATH_FORCEINLINE Matrix Matrix::Perspective(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixPerspectiveFovLH(fov_y_radians, aspect_ratio, near_plane, far_plane)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::PerspectiveLH(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixPerspectiveFovLH(fov_y_radians, aspect_ratio, near_plane, far_plane)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::PerspectiveRH(float fov_y_radians, float aspect_ratio, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixPerspectiveFovRH(fov_y_radians, aspect_ratio, near_plane, far_plane)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::Orthographic(float width, float height, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixOrthographicLH(width, height, near_plane, far_plane)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::OrthographicLH(float width, float height, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixOrthographicLH(width, height, near_plane, far_plane)}; }
    DD_MATH_FORCEINLINE Matrix Matrix::OrthographicRH(float width, float height, float near_plane, float far_plane) noexcept { return Matrix{DirectX::XMMatrixOrthographicRH(width, height, near_plane, far_plane)}; }

    DD_MATH_FORCEINLINE Matrix Matrix::Identity() noexcept { return Matrix{DirectX::XMMatrixIdentity()}; }

    DD_MATH_FORCEINLINE Matrix Matrix::Zero() noexcept
    {
        return Matrix{DirectX::XMMATRIX{
            DirectX::XMVectorZero(),
            DirectX::XMVectorZero(),
            DirectX::XMVectorZero(),
            DirectX::XMVectorZero()
        }};
    }

    DD_MATH_FORCEINLINE bool Matrix::IsIdentity() const noexcept { return DirectX::XMMatrixIsIdentity(data); }

    DD_MATH_FORCEINLINE bool Matrix::IsNearlyEqual(const Matrix& other, float epsilon) const noexcept
    {
        DirectX::XMVECTOR epsilon_vec = DirectX::XMVectorReplicate(epsilon);
        return DirectX::XMVector4NearEqual(data.r[0], other.data.r[0], epsilon_vec) &&
               DirectX::XMVector4NearEqual(data.r[1], other.data.r[1], epsilon_vec) &&
               DirectX::XMVector4NearEqual(data.r[2], other.data.r[2], epsilon_vec) &&
               DirectX::XMVector4NearEqual(data.r[3], other.data.r[3], epsilon_vec);
    }

    // ================================
    // Matrix Free Functions
    // ================================

    template<int N>
    DD_MATH_FORCEINLINE Vector Transform(Vector vector, Matrix matrix) noexcept
    {
        static_assert(N >= 2 && N <= 4, "Transform dimensionality must be 2, 3, or 4");
        if constexpr (N == 2) { return Vector{DirectX::XMVector2TransformCoord(vector.Data(), matrix.Data())}; }
        else if constexpr (N == 3) { return Vector{DirectX::XMVector3TransformCoord(vector.Data(), matrix.Data())}; }
        else { return Vector{DirectX::XMVector4Transform(vector.Data(), matrix.Data())}; }
    }

    DD_MATH_FORCEINLINE Vector TransformNormal(Vector normal, Matrix matrix) noexcept { return Vector{DirectX::XMVector3TransformNormal(normal.Data(), matrix.Data())}; }

    DD_MATH_FORCEINLINE Matrix operator*(float scalar, const Matrix& mat) noexcept { return mat * scalar; }

    // Matrix conversion functions
    DD_MATH_FORCEINLINE Matrix ToMatrix(const Float3x3& storage) noexcept
    {
        const auto& m = storage.Data();
        DirectX::XMFLOAT3X3 tmp(&m[0][0]);
        return Matrix{DirectX::XMLoadFloat3x3(&tmp)};
    }

    DD_MATH_FORCEINLINE Matrix ToMatrix(const Float4x3& storage) noexcept
    {
        const auto& m = storage.Data();
        DirectX::XMFLOAT4X3 tmp(&m[0][0]);
        return Matrix{DirectX::XMLoadFloat4x3(&tmp)};
    }

    DD_MATH_FORCEINLINE Matrix ToMatrix(const Float4x4& storage) noexcept
    {
        const auto& m = storage.Data();
        DirectX::XMFLOAT4X4 tmp(&m[0][0]);
        return Matrix{DirectX::XMLoadFloat4x4(&tmp)};
    }

    template<>
    DD_MATH_FORCEINLINE Float3x3 FromMatrix(const Matrix& mat) noexcept
    {
        DirectX::XMFLOAT3X3 result;
        DirectX::XMStoreFloat3x3(&result, mat.Data());
        return Float3x3(
            result.m[0][0], result.m[0][1], result.m[0][2],
            result.m[1][0], result.m[1][1], result.m[1][2],
            result.m[2][0], result.m[2][1], result.m[2][2]
        );
    }

    template<>
    DD_MATH_FORCEINLINE Float4x3 FromMatrix(const Matrix& mat) noexcept
    {
        DirectX::XMFLOAT4X3 result;
        DirectX::XMStoreFloat4x3(&result, mat.Data());
        return Float4x3(
            result.m[0][0], result.m[0][1], result.m[0][2],
            result.m[1][0], result.m[1][1], result.m[1][2],
            result.m[2][0], result.m[2][1], result.m[2][2],
            result.m[3][0], result.m[3][1], result.m[3][2]
        );
    }

    template<>
    DD_MATH_FORCEINLINE Float4x4 FromMatrix(const Matrix& mat) noexcept
    {
        DirectX::XMFLOAT4X4 result;
        DirectX::XMStoreFloat4x4(&result, mat.Data());
        return Float4x4(
            result.m[0][0], result.m[0][1], result.m[0][2], result.m[0][3],
            result.m[1][0], result.m[1][1], result.m[1][2], result.m[1][3],
            result.m[2][0], result.m[2][1], result.m[2][2], result.m[2][3],
            result.m[3][0], result.m[3][1], result.m[3][2], result.m[3][3]
        );
    }

} // namespace math
