#include "graph/PipelineDesc.hpp"

namespace velox
{

bool ShaderRef::IsValid() const noexcept
{
    return !variantName.empty() && !entryPoint.empty();
}

RenderState RenderState::Opaque() noexcept
{
    return RenderState{};
}

RenderState RenderState::AlphaBlend() noexcept
{
    RenderState state;
    state.depthWrite = false;
    state.blend = BlendMode::AlphaBlend;
    return state;
}

RenderState RenderState::Additive() noexcept
{
    RenderState state;
    state.depthWrite = false;
    state.blend = BlendMode::Additive;
    return state;
}

RenderState RenderState::FullscreenPost() noexcept
{
    RenderState state;
    state.cullMode = CullMode::None;
    state.depthCompare = CompareFunction::Always;
    state.depthWrite = false;
    return state;
}

RenderState RenderState::DepthOnly() noexcept
{
    RenderState state;
    state.cullMode = CullMode::Front;
    return state;
}

} // namespace velox
