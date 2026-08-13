#pragma once
#ifndef VELOX_SHADER_LIBRARY_TYPES_HPP
#define VELOX_SHADER_LIBRARY_TYPES_HPP
#include "resource/ResourceFlags.hpp"
#include <cstdint>
#include <span>
#include <string_view>

/**
 * @brief The vocabulary the shader cooker writes and the rendergraph reads.
 *
 * This header is hand-written, not generated. The rendergraph includes this file. It never includes
 * the generated shader library. Compile() therefore does not know that the library is generated code,
 * and a change to the cooker's output format does not reach the graph.
 *
 * Texture formats and view dimensions come from ResourceFlags.hpp. One definition serves both the
 * resource layer and the cooker, so the two cannot disagree.
 */
namespace velox
{

/** @brief The WebGPU binding type that one shader resource needs. */
enum class BindingKind : uint8_t
{
    Invalid = 0,
    UniformBuffer,
    StorageBuffer,
    ReadOnlyStorageBuffer,
    SampledTexture,
    StorageTexture,
    Sampler,
};

enum class ShaderStageKind : uint8_t
{
    Invalid = 0,
    Vertex,
    Fragment,
    Compute,
};

/** @brief The shape of a bound resource, as the shader declares it. This decides the view dimension
 * the graph must create. The shader is the only authority here: the CPU side never states it. */
enum class ResourceShape : uint8_t
{
    Invalid = 0,
    Buffer,
    Texture1D,
    Texture2D,
    Texture2DArray,
    Texture3D,
    TextureCube,
    TextureCubeArray,
    Texture2DMultisample,
};

/** @brief How a shader samples a texture. The graph checks this against the format it creates: a
 * shader that samples a float texture cannot bind an integer one. */
enum class TextureSampleType : uint8_t
{
    Invalid = 0,
    Float,
    UnfilterableFloat,
    Depth,
    SignedInteger,
    UnsignedInteger,
};

/** @brief What a shader does to a storage texture. */
enum class StorageTextureAccess : uint8_t
{
    Invalid = 0,
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

enum class SamplerBindingType : uint8_t
{
    Invalid = 0,
    Filtering,
    NonFiltering,
    Comparison,
};

/** @brief Compute workgroup dimensions. The shader declares these, so they are always reflectable. */
struct WorkgroupSize
{
    uint32_t X{ 1u };
    uint32_t Y{ 1u };
    uint32_t Z{ 1u };
};

/** @brief One resource a shader binds, as the generated library states it.
 *
 * This is the runtime form of the cooker's ReflectedBinding. It views strings instead of owning them,
 * because every string points into the generated data.
 *
 * Compile() looks up a name here and takes the group and the binding from the result. No caller ever
 * writes a group index or a binding index, so the two sides cannot drift apart.
 */
struct BindingInfo
{
    std::string_view Name;
    uint32_t Group{ 0u };
    uint32_t Binding{ 0u };
    BindingKind Kind{ BindingKind::Invalid };

    /** @brief Size of one structured buffer element, in bytes. Zero for a texture or a sampler. */
    uint32_t ElementStride{ 0u };
    /** @brief Total size of a uniform block, in bytes. Zero for every other binding kind. */
    uint64_t ByteSize{ 0u };
    uint32_t ArrayCount{ 1u };

    ResourceShape Shape{ ResourceShape::Invalid };
    TextureSampleType SampleType{ TextureSampleType::Invalid };
    TextureFormat StorageFormat{ TextureFormat::Invalid };
    StorageTextureAccess StorageAccess{ StorageTextureAccess::Invalid };
    SamplerBindingType SamplerType{ SamplerBindingType::Invalid };

    /** @brief Element count from a `[vx_element_count]` annotation, already evaluated for this
     * variant. Zero means the shader did not annotate the resource, so the caller must give a size. */
    uint64_t DerivedElementCount{ 0u };
    /** @brief Texture extent from a `[vx_extent_2d]` or `[vx_extent_3d]` annotation. Zero width means
     * the shader did not annotate the resource. */
    uint32_t DerivedExtentX{ 0u };
    uint32_t DerivedExtentY{ 0u };
    uint32_t DerivedExtentZ{ 0u };

    /** @brief Byte size the graph must create, or zero when the shader states no element count. */
    [[nodiscard]] uint64_t DerivedByteSize() const noexcept;
};

/**
 * @brief Where the rendergraph gets shader sources and layouts.
 *
 * The generated library implements this. A future watch-and-serve cooker implements it again, and
 * talks to a running engine. The graph sees no difference between the two.
 *
 * The interface takes a raw `uint16_t` for the entry point, not the generated `EntryPointId` enum.
 * The graph therefore never includes the generated header, and a change to the shader set does not
 * recompile the graph.
 *
 * `Generation()` is the hot-reload hook. The baked provider returns a constant. A live provider
 * increments the counter when a shader changes, and the graph re-realizes the affected pipelines.
 */
class ShaderSourceProvider
{
public:
    ShaderSourceProvider() noexcept;
    virtual ~ShaderSourceProvider();
    ShaderSourceProvider(const ShaderSourceProvider&) = delete;
    ShaderSourceProvider& operator=(const ShaderSourceProvider&) = delete;

    /** @brief WGSL for one entry point of one variant. An unknown pair returns an empty view. */
    [[nodiscard]] virtual std::string_view Source(uint16_t entry_point,
                                                  uint32_t variant_index) const noexcept = 0;
    [[nodiscard]] virtual std::span<const BindingInfo> Bindings(uint16_t entry_point,
                                                                uint32_t variant_index) const noexcept = 0;
    [[nodiscard]] virtual WorkgroupSize Workgroup(uint16_t entry_point,
                                                  uint32_t variant_index) const noexcept = 0;
    /** @brief Increments when any source above changes. A constant means sources never change. */
    [[nodiscard]] virtual uint64_t Generation() const noexcept = 0;
};

/** @brief Finds one binding by the name the shader gave it.
 *
 * Compile() uses this to turn a declared binding name into a group and a binding index. A missing
 * name returns nullptr, and that must be an error naming both the shader and the declaration. A
 * silent default here would bind the wrong resource. */
[[nodiscard]] const BindingInfo* FindBindingByName(std::span<const BindingInfo> bindings,
                                                   std::string_view name) noexcept;

[[nodiscard]] bool IsBufferBinding(BindingKind kind) noexcept;
[[nodiscard]] bool IsTextureBinding(BindingKind kind) noexcept;

} // namespace velox

#endif // !VELOX_SHADER_LIBRARY_TYPES_HPP
