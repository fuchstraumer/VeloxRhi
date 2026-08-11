#pragma once
// DirectXMath transcendentals for math::Vector. Included from MathBackendDX.inl; include nothing.
//
// Forwarding: DirectXMath already handles the edge cases the WASM side spells out by hand. Results do
// not match that backend bit for bit; only the documented accuracy bounds are shared.
namespace velox::math
{

VX_MATH_FORCEINLINE Vector Vector::Exp2() const noexcept
{
    return Vector{ DirectX::XMVectorExp2(data) };
}

// No XMVectorExp2Est exists. The estimate is for the WASM target's benefit; a speed divergence
// between backends does not matter the way a semantic one would
VX_MATH_FORCEINLINE Vector Vector::Exp2Est() const noexcept
{
    return Vector{ DirectX::XMVectorExp2(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Log2() const noexcept
{
    return Vector{ DirectX::XMVectorLog2(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Log2Est() const noexcept
{
    return Vector{ DirectX::XMVectorLog2(data) };
}

VX_MATH_FORCEINLINE SinCosResult Vector::SinCos() const noexcept
{
    DirectX::XMVECTOR sine;
    DirectX::XMVECTOR cosine;
    DirectX::XMVectorSinCos(&sine, &cosine, data);
    return SinCosResult{ Vector{ sine }, Vector{ cosine } };
}

VX_MATH_FORCEINLINE SinCosResult Vector::SinCosEst() const noexcept
{
    DirectX::XMVECTOR sine;
    DirectX::XMVECTOR cosine;
    DirectX::XMVectorSinCosEst(&sine, &cosine, data);
    return SinCosResult{ Vector{ sine }, Vector{ cosine } };
}

VX_MATH_FORCEINLINE Vector Vector::Sin() const noexcept
{
    return Vector{ DirectX::XMVectorSin(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Cos() const noexcept
{
    return Vector{ DirectX::XMVectorCos(data) };
}

VX_MATH_FORCEINLINE Vector Vector::SinEst() const noexcept
{
    return Vector{ DirectX::XMVectorSinEst(data) };
}

VX_MATH_FORCEINLINE Vector Vector::CosEst() const noexcept
{
    return Vector{ DirectX::XMVectorCosEst(data) };
}

} // namespace velox::math
