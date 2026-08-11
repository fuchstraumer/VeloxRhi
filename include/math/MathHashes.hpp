#pragma once
#ifndef VELOX_RHI_MATH_HASHES_HPP
#define VELOX_RHI_MATH_HASHES_HPP
#include "Math.hpp"
#include <cstddef>
#include <functional>

// std::hash specializations for the storage vector types, so they can be used as keys in
// unordered containers (vertex welding during mesh generation being the motivating case).
// The SIMD Vector/Matrix types are deliberately not hashable: they are not meant to be stored,
// and their unused lanes may hold garbage that would produce unequal hashes for equal values.
//
// NOTE: these hash the float bit patterns as-is. +0.0f and -0.0f compare equal under
// Float2::operator== but hash differently, and any NaN hashes to something that never matches
// itself. Callers welding geometry should quantize before hashing rather than relying on
// exact float equality.

namespace velox::math::detail
{
    // boost::hash_combine, with the golden-ratio constant for distribution
    constexpr std::size_t HashCombine(std::size_t seed, std::size_t hash) noexcept
    {
        return seed ^ (hash + 0x9e3779b9u + (seed << 6) + (seed >> 2));
    }

    inline std::size_t HashFloat(float value) noexcept
    {
        return std::hash<float>{}(value);
    }
} // namespace velox::math::detail

namespace std
{
    template<>
    struct hash<velox::math::Float2>
    {
        std::size_t operator()(const velox::math::Float2& vec) const noexcept
        {
            using namespace velox::math::detail;
            return HashCombine(HashFloat(vec.x), HashFloat(vec.y));
        }
    };

    template<>
    struct hash<velox::math::Float3>
    {
        std::size_t operator()(const velox::math::Float3& vec) const noexcept
        {
            using namespace velox::math::detail;
            std::size_t seed = HashCombine(HashFloat(vec.x), HashFloat(vec.y));
            return HashCombine(seed, HashFloat(vec.z));
        }
    };

    template<>
    struct hash<velox::math::Float4>
    {
        std::size_t operator()(const velox::math::Float4& vec) const noexcept
        {
            using namespace velox::math::detail;
            std::size_t seed = HashCombine(HashFloat(vec.x), HashFloat(vec.y));
            seed = HashCombine(seed, HashFloat(vec.z));
            return HashCombine(seed, HashFloat(vec.w));
        }
    };
} // namespace std

#endif // !VELOX_RHI_MATH_HASHES_HPP
