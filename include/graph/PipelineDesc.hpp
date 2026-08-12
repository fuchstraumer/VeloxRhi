#pragma once
#ifndef VELOX_GRAPH_PIPELINE_DESC_HPP
#define VELOX_GRAPH_PIPELINE_DESC_HPP
#include <cstdint>
#include <string_view>

/**
 * @brief Descriptions of what is needed to declare a render pipeline (primarily, as compute pipelines
 * are just a shader and entrypoint). This is a reduced subset of the full pipeline descriptor,
 * because we use the shader cooker to reflect most of the more fiddly information and cook it 
 * into the variant for us. These fields are all intended to be unable to drift from the shader,
 * helping us reduce a class of errors and bugs that are otherwise easy to make and hard to diagnose.
 */
namespace velox
{

/**@brief Identifies one cooked shader variant and the entry point within it.
 * todo-soon: Gotta fix up the shader compiler to actually the requisite descriptor data
 * from reflection, and just use a key into memory/on-disk for the actual source.
 * This is a placeholder to start getting graph builder to work.
 */
struct ShaderRef
{
    std::string_view variantName{};
    /** @brief Slang preserves entrypoint names, so this will be variable */
    std::string_view entryPoint{};
    std::string_view wgslSource{};

    [[nodiscard]] bool IsValid() const noexcept;
};

enum class PrimitiveTopology : uint8_t
{
    Invalid = 0,
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip
};

enum class CullMode : uint8_t
{
    Invalid = 0,
    None,
    Front,
    Back
};

enum class FrontFace : uint8_t
{
    Invalid = 0,
    CounterClockwise,
    Clockwise
};

enum class CompareFunction : uint8_t
{
    Invalid = 0,
    Never,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always
};

/**@brief WebGPU full blending set is much more complex than this, so for now we're
 * working at intent level and not detail level. Will specialize as we prove out the demos */
enum class BlendMode : uint8_t
{
    Invalid = 0,
    Opaque,
    AlphaBlend,
    PremultipliedAlpha,
    Additive
};

/**@brief As described above, these values are all that actually changes vs what the shader
 *  declares and identifies for us. 
 * @note Whether or not depth is required is decided here by `depthWrite`
 * // todo-ship: that's a potential silent footgun, can we assess it from reflection as well?
*/
struct RenderState
{
    PrimitiveTopology topology{ PrimitiveTopology::TriangleList };
    CullMode cullMode{ CullMode::Back };
    FrontFace frontFace{ FrontFace::CounterClockwise };
    CompareFunction depthCompare{ CompareFunction::Less };
    bool depthWrite{ true };
    BlendMode blend{ BlendMode::Opaque };

    /** @brief Standard depth-tested geometry. Equivalent to the defaults above. */
    static RenderState Opaque() noexcept;
    /** @brief Transparency: depth-tested but not depth-writing, alpha blended. */
    static RenderState AlphaBlend() noexcept;
    /** @brief Additive transparency, for particles and light accumulation. */
    static RenderState Additive() noexcept;
    /** @brief A full-screen triangle or quad: no culling, no depth test, no depth write. */
    static RenderState FullscreenPost() noexcept;
    /** @brief Depth prepass or shadow render: writes depth, no color, front faces culled. */
    static RenderState DepthOnly() noexcept;
};

/** @brief A raster pipeline. Two entry points, plus the small state block.
 * @note fragment may be left invalid for a depth-only or shadow pass.
 */
struct RenderPipelineDesc
{
    ShaderRef vertex{};
    ShaderRef fragment{};
    RenderState state{};
};

/** @brief A compute pipeline, which is genuinely just a shader. Workgroup size lives in the shader and
 * is reflectable, and there is no render state to speak of.
 */
struct ComputePipelineDesc
{
    ShaderRef shader{};
};

} // namespace velox

#endif // !VELOX_GRAPH_PIPELINE_DESC_HPP
