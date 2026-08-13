#pragma once
#ifndef VELOX_GRAPH_RESOURCE_DECLARATION_HPP
#define VELOX_GRAPH_RESOURCE_DECLARATION_HPP
#include "ResourceFlags.hpp"
#include "ResourceHandle.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

/**
 * @brief Declarations describing how a resource comes into existence, separate from how a subpass
 * uses one (ResourceUsage.hpp).
 *
 * This split is vital for handling cases like a graph refreshing due to a changed feature (which could 
 * come from something like performance scaling, for example). The graph is recompiled, but for resources
 * that already exist and don't need to respond, we can just rebind them to the new graph. This helps
 * reduce a whole class of hitches and perf sinks.
 *
 * Another vital design choice is having sizes be a *source* rather than an explicit value. If we
 * say a resource has a derived size, the backend is free to automatically create that and save 
 * us from having to manually do so. If it's explicit, then we'll validate the size is at least a
 * multiple of the stride (when possible) and make that a graph-build-time failure.
 * 
 * This helps make sure that for buffers and resources sized on things like fixed shader permutation
 * constants, we don't have to manually update CPU code as well!
 */
namespace velox
{

/** @brief Where a buffer's byte size comes from. Invalid is the default deliberately: a declaration
 * that says neither "derive this" nor "here is the size" is a Compile() error naming the resource, not
 * a silent zero-byte allocation.
 */
enum class SizeSource : uint8_t
{
    Invalid = 0,
    /** @brief Computed at cook time from the reflected element stride and a constant expression over
     * the permutation constants. The caller may not override it. Compile() resolves the size from the
     * shaders that actually bind this buffer, and two shaders disagreeing about it is an error naming
     * both - which is the check this whole mechanism exists to enable.
     */
    Derived,
    /** @brief Supplied by the caller. Still validated against the reflected stride where one exists: a
     * size that is not a whole multiple of the element stride is a Compile() error.
     */
    Explicit
};

/** @brief Initial contents for a resource, uploaded during Realize() via the map awaitables.
 * @note Non-owning. The referenced storage must outlive realization, not merely the declaration call -
 * uploads are async and may span frames. Static tables and generated LUTs are the intended source.
 */
// todo-ship: InitialData's lifetime contract is documented but unenforced. Passing a local container
// here compiles cleanly and use-after-frees inside a map callback, several frames from the call site.
// Take an owned copy into a staging arena once one exists.
using InitialData = std::span<const std::byte>;

struct BufferDeclaration
{
    /** @brief Used by the graph visualizer and named in every Compile() diagnostic about this resource.
     * Not optional - an unnamed resource makes every error about it useless.
     */
    std::string_view name{};
    SizeSource sizeSource{ SizeSource::Invalid };
    /** @brief Ignored, and required to be zero, when sizeSource is Derived. */
    size_t byteSize{ 0u };
    /** @brief The capability the buffer is created with. Cross-checked against declared access: a
     * subpass writing a buffer created without Storage is a Compile() error rather than a Dawn
     * validation message arriving at runtime.
     */
    BufferUsageFlags creationUsage{ BufferUsageFlags::Invalid };
    InitialData initialData{};
};

/** @brief Where a texture's dimensions come from. Distinct from SizeSource because the modes genuinely
 * differ - the surface-relative case has no buffer analogue, and buffers have no notion of extent.
 */
enum class ExtentSource : uint8_t
{
    Invalid = 0,
    /** @brief Derived at cook time from permutation constants, the same way buffer sizes are. This is
     * what an OceanFFT spectrum or displacement target wants: IFFT_SIZE square, per variant, with no
     * number written on the CPU side at all.
     */
    Derived,
    /** @brief A fixed extent supplied by the caller. */
    Explicit,
    /** @brief A fraction of the current surface size, recomputed on resize. Half-res bloom chains and
     * quarter-res ambient occlusion targets live here, and this is what stops OnResize from becoming a
     * hand-maintained list of every intermediate target.
     */
    SurfaceRelative
};

struct SurfaceRelativeExtent
{
    float widthScale{ 1.0f };
    float heightScale{ 1.0f };
};

struct TextureExtent
{
    uint32_t width{ 0u };
    uint32_t height{ 0u };
    uint32_t depthOrArrayLayers{ 1u };
};

struct TextureDeclaration
{
    std::string_view name{};
    ExtentSource extentSource{ ExtentSource::Invalid };
    /** @brief Read only when extentSource is Explicit. */
    TextureExtent extent{};
    /** @brief Read only when extentSource is SurfaceRelative. */
    SurfaceRelativeExtent surfaceRelativeExtent{};
    /** @brief Caller's decision for sampled textures - a shader declares that it samples a float4, not
     * that the texture is Rgba16Float rather than Rgba8Unorm.
     * @note Storage bindings are the exception: the shader spells the format into the binding type, so
     * it is reflectable there and Compile() should reject a declaration that disagrees with it.
     */
    TextureFormat format{ TextureFormat::Invalid };
    TextureDimension dimension{ TextureDimension::Texture2D };
    uint32_t mipLevelCount{ 1u };
    uint32_t sampleCount{ 1u };
    TextureUsageFlags creationUsage{ TextureUsageFlags::Invalid };
    InitialData initialData{};
};

} // namespace velox

#endif // !VELOX_GRAPH_RESOURCE_DECLARATION_HPP
