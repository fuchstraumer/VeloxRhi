#pragma once
#ifndef VELOX_GRAPH_RESOURCE_FLAG_CONVERSIONS_HPP
#define VELOX_GRAPH_RESOURCE_FLAG_CONVERSIONS_HPP
#include "ResourceFlags.hpp"
#include <webgpu/webgpu_cpp.h>

/**
 * @brief The single boundary between Velox's resource vocabulary and Dawn's.
 *
 * This is the only graph header that includes webgpu_cpp.h, and it should be included only by source
 * files - never by another header. That containment is the entire point: graph headers stay cheap, and
 * a backend swap or a Dawn version bump has one file to answer for.
 *
 * Every conversion is a spelled-out mapping defined out of line, not a reinterpreting cast. Velox's
 * enum values are not required to match Dawn's, and are not maintained to.
 */
namespace velox
{

[[nodiscard]] wgpu::BufferUsage ToWgpu(BufferUsageFlags usage) noexcept;
[[nodiscard]] wgpu::TextureUsage ToWgpu(TextureUsageFlags usage) noexcept;
[[nodiscard]] wgpu::TextureFormat ToWgpu(TextureFormat format) noexcept;
[[nodiscard]] wgpu::TextureDimension ToWgpu(TextureDimension dimension) noexcept;
[[nodiscard]] wgpu::TextureViewDimension ToWgpu(TextureViewDimension dimension) noexcept;

/** @brief Needed because surface capability queries hand back Dawn's formats and the graph has to
 * decide whether it can express them.
 * @return TextureFormat::Invalid for any format outside Velox's curated set, which callers are
 * expected to treat as an error rather than substitute a default for.
 */
[[nodiscard]] TextureFormat FromWgpu(wgpu::TextureFormat format) noexcept;

} // namespace velox

#endif // !VELOX_GRAPH_RESOURCE_FLAG_CONVERSIONS_HPP
