#pragma once
#ifndef VELOX_GRAPH_SUBPASS_DESC_HPP
#define VELOX_GRAPH_SUBPASS_DESC_HPP
#include "graph/ResourceBinding.hpp"
#include "graph/ResourceHandle.hpp"
#include "graph/PipelineDesc.hpp"
#include <string_view>
#include <vector>


/**@brief Describes the actual resource usages (and other requisite information) required
 * to traverse and validate our Rendergraph. Declared down in src/graph/ as it's got a 
 * number of std includes, and isn't really used by the frontend user-facing code. The
 * cost of indirection for PImpl is not that bad, and we only pay for it during graph
 * compiliation, not during runtime traversal.
 */
namespace velox
{

struct SubpassDesc
{
    std::string_view Name;
    NodeHandle Handle{ NodeHandle::Invalid() };
    TemplateNodeHandle Template{ TemplateNodeHandle::Invalid() };
    std::vector<BufferBinding> Buffers;
    std::vector<TextureBinding> Textures;
    // Explicit deps, including sentinel nodes like "FrameStart" and "FrameEnd"
    std::vector<NodeHandle> Dependencies;
    // instead of using a variant or union, just going to store both of these
    // whichever one is valid, that's what kind of subpass this is
    RenderPipelineDesc RenderPipeline{};
    ComputePipelineDesc ComputePipeline{};
};

};
