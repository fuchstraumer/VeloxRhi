#pragma once
// WASM SIMD128 transcendentals for math::Vector. Included from MathBackendWASM.inl; include nothing.
//
// All of these range-reduce, evaluate a Horner polynomial, then reapply what was factored out.
// Reduction uses VectorMask + Select rather than branching: four lanes can want different branches.
namespace velox::math
{
namespace detail
{
    // Explicit MulAdd, not a * x + b: strict FP rules forbid the compiler contracting that into an
    // FMA, so spelling it out is the only way to get one.

    // 2^f for f in [-0.5, 0.5]. Six-term exponential series, ~1.2e-7 absolute
    VX_MATH_FORCEINLINE v128_t Exp2Fraction(v128_t fraction) noexcept
    {
        v128_t polynomial = wasm_f32x4_const_splat(k_Exp2Poly6);
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly5));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly4));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly3));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly2));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly1));
        return MulAdd(polynomial, fraction, wasm_f32x4_const_splat(1.0f));
    }

    // Four terms; next term bounds error at (ln2^5 / 5!) * 0.5^5 ~= 4e-5
    VX_MATH_FORCEINLINE v128_t Exp2FractionEst(v128_t fraction) noexcept
    {
        v128_t polynomial = wasm_f32x4_const_splat(k_Exp2Poly4);
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly3));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly2));
        polynomial = MulAdd(polynomial, fraction, wasm_f32x4_const_splat(k_Exp2Poly1));
        return MulAdd(polynomial, fraction, wasm_f32x4_const_splat(1.0f));
    }

    // 2^n straight into the IEEE-754 exponent field. No multiply, no table
    VX_MATH_FORCEINLINE v128_t Exp2FromIntegerPart(v128_t integer_part) noexcept
    {
        const v128_t biased = wasm_i32x4_add(integer_part, wasm_i32x4_const_splat(127));
        return wasm_i32x4_shl(biased, 23);
    }

    VX_MATH_FORCEINLINE v128_t TruncateToInt(v128_t value) noexcept
    {
#if defined(VX_MATH_RELAXED_SIMD)
        // out-of-range input is outside the documented domain, so saturation is not worth paying for
        return wasm_i32x4_relaxed_trunc_f32x4(value);
#else
        return wasm_i32x4_trunc_sat_f32x4(value);
#endif
    }

    // 2^x = 2^n * 2^f. Nearest, not floor: f lands in [-0.5, 0.5] rather than [0, 1), which is the
    // interval MathPolynomials.hpp's error bounds assume. Flooring costs two orders of magnitude
    template<bool Estimate>
    VX_MATH_FORCEINLINE v128_t Exp2Impl(v128_t value) noexcept
    {
        const v128_t nearest = wasm_f32x4_nearest(value);
        const v128_t fraction = wasm_f32x4_sub(value, nearest);
        const v128_t mantissa = Estimate ? Exp2FractionEst(fraction) : Exp2Fraction(fraction);
        const v128_t scale = Exp2FromIntegerPart(TruncateToInt(nearest));
        return wasm_f32x4_mul(mantissa, scale);
    }

    // log2 of a positive finite value: exponent + atanh series in t = (m - 1) / (m + 1)
    template<bool Estimate>
    VX_MATH_FORCEINLINE v128_t Log2Impl(v128_t value) noexcept
    {
        const v128_t bits = value;
        const v128_t exponentBits =
            wasm_u32x4_shr(wasm_v128_and(bits, wasm_i32x4_const_splat(0x7F800000)), 23);
        const v128_t exponent =
            wasm_f32x4_convert_i32x4(wasm_i32x4_sub(exponentBits, wasm_i32x4_const_splat(127)));

        // exponent field forced to biased 127, so this reads back as a value in [1, 2)
        const v128_t mantissaBits = wasm_v128_or(wasm_v128_and(bits, wasm_i32x4_const_splat(0x007FFFFF)),
                                                 wasm_i32x4_const_splat(0x3F800000));

        // folding [sqrt2, 2) down and carrying the factor bounds |t| at ~0.1716 instead of ~0.333,
        // which is why four series terms suffice
        constexpr float k_Sqrt2 = 1.41421356f;
        const VectorMask aboveSqrt2 =
            VectorMask{ wasm_f32x4_gt(mantissaBits, wasm_f32x4_const_splat(k_Sqrt2)) };
        const v128_t mantissa = Select(aboveSqrt2,
                                      Vector{ mantissaBits },
                                      Vector{ wasm_f32x4_mul(mantissaBits, wasm_f32x4_const_splat(0.5f)) })
                                    .Data();
        const v128_t adjustedExponent =
            Select(aboveSqrt2, Vector{ exponent }, Vector{ wasm_f32x4_add(exponent, wasm_f32x4_const_splat(1.0f)) })
                .Data();

        const v128_t one = wasm_f32x4_const_splat(1.0f);
        const v128_t ratio = wasm_f32x4_div(wasm_f32x4_sub(mantissa, one), wasm_f32x4_add(mantissa, one));
        const v128_t ratioSquared = wasm_f32x4_mul(ratio, ratio);

        v128_t series = wasm_f32x4_const_splat(Estimate ? k_Log2Poly5 : k_Log2Poly7);
        if constexpr (!Estimate)
        {
            series = MulAdd(series, ratioSquared, wasm_f32x4_const_splat(k_Log2Poly5));
        }
        series = MulAdd(series, ratioSquared, wasm_f32x4_const_splat(k_Log2Poly3));
        series = MulAdd(series, ratioSquared, wasm_f32x4_const_splat(k_Log2Poly1));

        const v128_t mantissaLog =
            wasm_f32x4_mul(wasm_f32x4_mul(series, ratio), wasm_f32x4_const_splat(k_Log2Scale));
        return wasm_f32x4_add(adjustedExponent, mantissaLog);
    }

    // ModAngles gives [-pi, pi]; the polynomials only hold on [-pi/2, pi/2]. Outer quadrants reflect
    // via sin(pi - x) == sin(x), signed pi so the result stays in range. cos(pi - x) == -cos(x), so
    // cosine picks up a sign flip there and sine does not.
    struct TrigReduction
    {
        v128_t angle;    // in [-pi/2, pi/2]
        v128_t cosSign;  // +-1 per lane, applied to cosine only
    };

    VX_MATH_FORCEINLINE TrigReduction ReduceForTrig(v128_t radians) noexcept
    {
        constexpr float k_Pi = std::numbers::pi_v<float>;
        constexpr float k_HalfPi = k_Pi * 0.5f;

        const v128_t wrapped = Vector{ radians }.ModAngles().Data();
        const v128_t signBit =
            wasm_v128_and(wrapped, wasm_i32x4_const_splat(static_cast<int32_t>(0x80000000)));
        // +pi where positive, -pi where negative
        const v128_t signedPi = wasm_v128_or(wasm_f32x4_const_splat(k_Pi), signBit);
        const v128_t reflected = wasm_f32x4_sub(signedPi, wrapped);

        const VectorMask inner =
            VectorMask{ wasm_f32x4_le(wasm_f32x4_abs(wrapped), wasm_f32x4_const_splat(k_HalfPi)) };

        TrigReduction reduction{};
        reduction.angle = Select(inner, Vector{ reflected }, Vector{ wrapped }).Data();
        reduction.cosSign = Select(inner,
                                   Vector{ wasm_f32x4_const_splat(-1.0f) },
                                   Vector{ wasm_f32x4_const_splat(1.0f) })
                                .Data();
        return reduction;
    }

    // odd by construction: x * (1 + x^2 * ...)
    template<bool Estimate>
    VX_MATH_FORCEINLINE v128_t SinPolynomial(v128_t angle) noexcept
    {
        const v128_t angleSquared = wasm_f32x4_mul(angle, angle);

        v128_t polynomial;
        if constexpr (Estimate)
        {
            polynomial = wasm_f32x4_const_splat(k_SinEstPoly7);
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinEstPoly5));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinEstPoly3));
        }
        else
        {
            polynomial = wasm_f32x4_const_splat(k_SinPoly11);
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinPoly9));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinPoly7));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinPoly5));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_SinPoly3));
        }
        polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(1.0f));
        return wasm_f32x4_mul(polynomial, angle);
    }

    // even by construction: 1 + x^2 * ...
    template<bool Estimate>
    VX_MATH_FORCEINLINE v128_t CosPolynomial(v128_t angle) noexcept
    {
        const v128_t angleSquared = wasm_f32x4_mul(angle, angle);

        v128_t polynomial;
        if constexpr (Estimate)
        {
            polynomial = wasm_f32x4_const_splat(k_CosEstPoly6);
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosEstPoly4));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosEstPoly2));
        }
        else
        {
            polynomial = wasm_f32x4_const_splat(k_CosPoly10);
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosPoly8));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosPoly6));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosPoly4));
            polynomial = MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(k_CosPoly2));
        }
        return MulAdd(polynomial, angleSquared, wasm_f32x4_const_splat(1.0f));
    }
} // namespace detail

VX_MATH_FORCEINLINE Vector Vector::Exp2() const noexcept
{
    return Vector{ detail::Exp2Impl<false>(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Exp2Est() const noexcept
{
    return Vector{ detail::Exp2Impl<true>(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Log2() const noexcept
{
    return Vector{ detail::Log2Impl<false>(data) };
}

VX_MATH_FORCEINLINE Vector Vector::Log2Est() const noexcept
{
    return Vector{ detail::Log2Impl<true>(data) };
}

VX_MATH_FORCEINLINE SinCosResult Vector::SinCos() const noexcept
{
    const detail::TrigReduction reduction = detail::ReduceForTrig(data);
    return SinCosResult{ Vector{ detail::SinPolynomial<false>(reduction.angle) },
                         Vector{ wasm_f32x4_mul(detail::CosPolynomial<false>(reduction.angle),
                                                reduction.cosSign) } };
}

VX_MATH_FORCEINLINE SinCosResult Vector::SinCosEst() const noexcept
{
    const detail::TrigReduction reduction = detail::ReduceForTrig(data);
    return SinCosResult{ Vector{ detail::SinPolynomial<true>(reduction.angle) },
                         Vector{ wasm_f32x4_mul(detail::CosPolynomial<true>(reduction.angle),
                                                reduction.cosSign) } };
}

VX_MATH_FORCEINLINE Vector Vector::Sin() const noexcept
{
    return Vector{ detail::SinPolynomial<false>(detail::ReduceForTrig(data).angle) };
}

VX_MATH_FORCEINLINE Vector Vector::Cos() const noexcept
{
    const detail::TrigReduction reduction = detail::ReduceForTrig(data);
    return Vector{ wasm_f32x4_mul(detail::CosPolynomial<false>(reduction.angle), reduction.cosSign) };
}

VX_MATH_FORCEINLINE Vector Vector::SinEst() const noexcept
{
    return Vector{ detail::SinPolynomial<true>(detail::ReduceForTrig(data).angle) };
}

VX_MATH_FORCEINLINE Vector Vector::CosEst() const noexcept
{
    const detail::TrigReduction reduction = detail::ReduceForTrig(data);
    return Vector{ wasm_f32x4_mul(detail::CosPolynomial<true>(reduction.angle), reduction.cosSign) };
}

} // namespace velox::math
