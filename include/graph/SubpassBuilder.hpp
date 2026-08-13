#pragma once
#ifndef VELOX_GRAPH_SUBPASS_BUILDER_HPP
#define VELOX_GRAPH_SUBPASS_BUILDER_HPP
#include "PipelineDesc.hpp"
#include "ResourceHandle.hpp"
#include "ResourceBinding.hpp"
#include <memory>

namespace velox
{

struct SubpassDesc;

// just some vague function pointer syntax for ExecutionCallback for now
using ExecutionCallback = void(*)(void* userData, NodeHandle node);

class SubpassBuilder
{
public:
    SubpassBuilder& AddBuffer(BufferBinding binding) noexcept;
    SubpassBuilder& AddTexture(TextureBinding binding) noexcept;
    SubpassBuilder& SetRenderPipeline(RenderPipelineDesc pipeline) noexcept;
    SubpassBuilder& SetComputePipeline(ComputePipelineDesc pipeline) noexcept;
    SubpassBuilder& AddDependency(NodeHandle dependency) noexcept;
    SubpassBuilder& SetExecutionCallback(ExecutionCallback callback) noexcept;

    SubpassBuilder(SubpassBuilder&&) noexcept;
    ~SubpassBuilder() noexcept;
    /** Allocated as soon as we open it: valid before final commit. */
    NodeHandle Handle() const noexcept;
private:

    SubpassBuilder(class GraphBuilder* parentGraph_in, NodeHandle handle_in) noexcept;
    friend class GraphBuilder;
    GraphBuilder* parentGraph{ nullptr };
    NodeHandle handle{ NodeHandle::Invalid() };
    std::unique_ptr<SubpassDesc> desc{ nullptr };
};

} // namespace velox

#endif // !VELOX_GRAPH_SUBPASS_BUILDER_HPP
