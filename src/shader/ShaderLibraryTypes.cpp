#include "shader/ShaderLibraryTypes.hpp"

namespace velox
{

ShaderSourceProvider::ShaderSourceProvider() noexcept = default;
ShaderSourceProvider::~ShaderSourceProvider() = default;

const BindingInfo* FindBindingByName(std::span<const BindingInfo> bindings,
                                     std::string_view name) noexcept
{
    for (const BindingInfo& binding : bindings)
    {
        if (binding.Name == name)
        {
            return &binding;
        }
    }

    return nullptr;
}

uint64_t BindingInfo::DerivedByteSize() const noexcept
{
    if (DerivedElementCount == 0u || ElementStride == 0u)
    {
        return 0u;
    }

    return DerivedElementCount * static_cast<uint64_t>(ElementStride);
}

bool IsBufferBinding(BindingKind kind) noexcept
{
    return kind == BindingKind::UniformBuffer || kind == BindingKind::StorageBuffer ||
           kind == BindingKind::ReadOnlyStorageBuffer;
}

bool IsTextureBinding(BindingKind kind) noexcept
{
    return kind == BindingKind::SampledTexture || kind == BindingKind::StorageTexture;
}

} // namespace velox
