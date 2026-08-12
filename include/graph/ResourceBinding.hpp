#pragma once
#ifndef VELOX_GRAPH_RESOURCE_USAGE_HPP
#define VELOX_GRAPH_RESOURCE_USAGE_HPP
#include "ResourceHandle.hpp"
#include "utility/EnumClassUtils.hpp"
#include <cstdint>
#include <string_view>

/**
 * @brief Declarations describing how a single subpass touches an already-created resource.
 *
 * These do triple duty at compile time: they construct bind groups, they derive dependency edges
 * (a resource shared by two nodes with at least one writer implies an edge), and they populate the
 * dirty-watch list that gates whether a node re-runs. One declaration, three consumers, so a node
 * cannot read a resource it forgot to declare an edge for.
 *
 * Deliberately *not* the same thing as BufferUsageFlags / TextureUsageFlags in ResourceFlags.hpp. Those
 * describe the capability a resource was created with; these describe what one node does with it, and
 * Compile() checks the latter against the former. Nor do they carry binding kind, group, or binding
 * index - those come from the cooker's reflection table and are never written by hand, so they cannot
 * drift.
 */
namespace velox
{

/** @brief How a subpass touches a resource. Read/Write is the entirety of what edge derivation and
 * dirty tracking need to know; anything finer-grained (stage visibility, storage vs. uniform) is
 * reflected from the shader rather than declared here.
 */
enum class ResourceAccess : uint8_t
{
    Invalid = 0,
    Read = 1 << 0,
    Write = 1 << 1,
    ReadWrite = Read | Write
};

MAKE_ENUM_CLASS_FLAGS(ResourceAccess);

/** @brief Whether a declared read participates in the generation-comparison that decides if a node
 * re-runs this frame. Encoded as an enum rather than a bool because the third case already exists in
 * the design, and because a struct with one bool reliably grows a second one.
 */
enum class DirtyPolicy : uint8_t
{
    Invalid = 0,
    /** @brief Default. The node re-runs when this resource's generation exceeds its last run. */
    Watch,
    /** @brief Read every frame, but changes to it do not by themselves justify re-running. Frame
     * uniforms and other always-changing inputs belong here, or every node watching them never sleeps.
     */
    Ignore
};

/** @brief Identifies the shader binding this resource is bound to. Resolved against the cooked
 * reflection table at Compile(): a name with no matching binding, or a binding with no declaration,
 * are both errors naming the shader and the subpass.
 * @note Expected to reference static storage - a string literal, or a name owned by the generated
 * reflection table. Nothing here copies it.
 */
using BindingName = std::string_view;

/** @brief Sentinel binding name for a watch-only declaration: the node does not bind this resource,
 * but changes to it should still wake the node. This is the plan's RerunOn(), expressed as an ordinary
 * usage entry so bind-group construction and edge derivation need no special case - they simply skip
 * entries whose binding name is empty.
 */
inline constexpr BindingName k_watchOnlyBinding{};

/** @brief Describes one buffer a subpass binds, and the terms on which it does so. */
struct BufferBinding
{
    BufferHandle resource{};
    BindingName bindingName{ k_watchOnlyBinding };
    ResourceAccess access{ ResourceAccess::Invalid };
    DirtyPolicy dirtyPolicy{ DirtyPolicy::Watch };
};

/** @brief Selects a subresource span of a texture. This is the one piece of texture usage information
 * reflection genuinely cannot supply: which mips and layers a given node touches is a property of the
 * node, not the shader. A node writing mip N while reading mip N-1 is a real dependency the graph can
 * only see if it is declared.
 */
struct TextureRange
{
    uint32_t baseMipLevel{ 0u };
    uint32_t mipLevelCount{ 1u };
    uint32_t baseArrayLayer{ 0u };
    uint32_t arrayLayerCount{ 1u };
};

/** @brief Default range covering a whole texture, for the common case where a node uses all of it. */
inline constexpr TextureRange k_wholeTexture{};

/** @brief Describes one texture a subpass binds, and the terms on which it does so.
 * @note Carries no view dimension, format, or sampled-vs-storage distinction. All three are reflected
 * from the shader, and the emitted view is derived from them rather than declared alongside them.
 */
struct TextureBinding
{
    TextureHandle resource{};
    BindingName bindingName{ k_watchOnlyBinding };
    ResourceAccess access{ ResourceAccess::Invalid };
    DirtyPolicy dirtyPolicy{ DirtyPolicy::Watch };
    TextureRange range{ k_wholeTexture };
};

} // namespace velox

#endif // !VELOX_GRAPH_RESOURCE_USAGE_HPP
