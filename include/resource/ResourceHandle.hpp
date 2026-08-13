#pragma once
#ifndef VELOX_GRAPH_RESOURCE_HANDLE_HPP
#define VELOX_GRAPH_RESOURCE_HANDLE_HPP
#include "utility/SlotMap.hpp"

namespace velox
{

template<typename TagType>
class TypedHandle
{
public:
    using Tag = TagType;
    using HandleType = SlotHandle;

    constexpr explicit TypedHandle(HandleType::IndexType index_in,
                                   HandleType::GenerationType generation_in) noexcept
        : handle(index_in, generation_in)
    {
    }

    constexpr explicit TypedHandle(HandleType handle_in) noexcept
        : handle(handle_in)
    {
    }

    constexpr TypedHandle() noexcept = default;

    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return handle.IsValid();
    }

    [[nodiscard]] friend constexpr bool operator==(const TypedHandle& lhs,
                                                   const TypedHandle& rhs) noexcept = default;

    constexpr static TypedHandle Invalid() noexcept
    {
        return TypedHandle{ HandleType::kInvalidIndex, HandleType::kInvalidGeneration };
    }

private:
    template<typename /* TagType */, std::size_t>
    friend class SlotMap;

    friend class ResourceRegistry;

    SlotHandle handle{ SlotHandle::kInvalidIndex, SlotHandle::kInvalidGeneration };
};

namespace detail
{
    struct BufferTag{};
    struct TextureTag{};
    struct SamplerTag{};
    struct PipelineTag{};
    struct ShaderTag{};
    struct NodeTag{};
    struct TemplateNodeTag{};
} // namespace detail

using BufferHandle = TypedHandle<detail::BufferTag>;
using TextureHandle = TypedHandle<detail::TextureTag>;
using SamplerHandle = TypedHandle<detail::SamplerTag>;
using PipelineHandle = TypedHandle<detail::PipelineTag>;
using ShaderHandle = TypedHandle<detail::ShaderTag>;
using NodeHandle = TypedHandle<detail::NodeTag>;
using TemplateNodeHandle = TypedHandle<detail::TemplateNodeTag>;

} // namespace velox

#endif // !VELOX_GRAPH_RESOURCE_HANDLE_HPP
