#pragma once
#ifndef VELOX_RESOURCE_REGISTRY_HPP
#define VELOX_RESOURCE_REGISTRY_HPP
#include "ResourceDeclaration.hpp"
#include "ResourceHandle.hpp"
#include "async/Future.hpp"

namespace velox
{

struct BufferWriteData
{
    const void* DataPtr{ nullptr };
    uint32_t ByteSize{ 0u };
    uint32_t ByteOffset{ 0u };
};

class ResourceRegistry
{
public:
    BufferHandle CreateBuffer(const BufferDeclaration& declaration) noexcept;
    TextureHandle CreateTexture(const TextureDeclaration& declaration) noexcept;

    void WriteBuffer(BufferHandle handle, const BufferWriteData& data) noexcept;
};

} // namespace velox

#endif // !VELOX_RESOURCE_REGISTRY_HPP
