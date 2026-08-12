#include "graph/ResourceFlagConversions.hpp"

namespace velox
{

wgpu::BufferUsage ToWgpu(BufferUsageFlags usage) noexcept
{
    wgpu::BufferUsage result = wgpu::BufferUsage::None;

    if (HasAllFlags(usage, BufferUsageFlags::MapRead))
    {
        result |= wgpu::BufferUsage::MapRead;
    }
    if (HasAllFlags(usage, BufferUsageFlags::MapWrite))
    {
        result |= wgpu::BufferUsage::MapWrite;
    }
    if (HasAllFlags(usage, BufferUsageFlags::CopySource))
    {
        result |= wgpu::BufferUsage::CopySrc;
    }
    if (HasAllFlags(usage, BufferUsageFlags::CopyDestination))
    {
        result |= wgpu::BufferUsage::CopyDst;
    }
    if (HasAllFlags(usage, BufferUsageFlags::Index))
    {
        result |= wgpu::BufferUsage::Index;
    }
    if (HasAllFlags(usage, BufferUsageFlags::Vertex))
    {
        result |= wgpu::BufferUsage::Vertex;
    }
    if (HasAllFlags(usage, BufferUsageFlags::Uniform))
    {
        result |= wgpu::BufferUsage::Uniform;
    }
    if (HasAllFlags(usage, BufferUsageFlags::Storage))
    {
        result |= wgpu::BufferUsage::Storage;
    }
    if (HasAllFlags(usage, BufferUsageFlags::Indirect))
    {
        result |= wgpu::BufferUsage::Indirect;
    }
    if (HasAllFlags(usage, BufferUsageFlags::QueryResolve))
    {
        result |= wgpu::BufferUsage::QueryResolve;
    }

    return result;
}

wgpu::TextureUsage ToWgpu(TextureUsageFlags usage) noexcept
{
    wgpu::TextureUsage result = wgpu::TextureUsage::None;

    if (HasAllFlags(usage, TextureUsageFlags::CopySource))
    {
        result |= wgpu::TextureUsage::CopySrc;
    }
    if (HasAllFlags(usage, TextureUsageFlags::CopyDestination))
    {
        result |= wgpu::TextureUsage::CopyDst;
    }
    if (HasAllFlags(usage, TextureUsageFlags::Sampled))
    {
        result |= wgpu::TextureUsage::TextureBinding;
    }
    if (HasAllFlags(usage, TextureUsageFlags::Storage))
    {
        result |= wgpu::TextureUsage::StorageBinding;
    }
    if (HasAllFlags(usage, TextureUsageFlags::RenderAttachment))
    {
        result |= wgpu::TextureUsage::RenderAttachment;
    }

    return result;
}

wgpu::TextureFormat ToWgpu(TextureFormat format) noexcept
{
    switch (format)
    {
    case TextureFormat::R8Unorm:
        return wgpu::TextureFormat::R8Unorm;
    case TextureFormat::Rg8Unorm:
        return wgpu::TextureFormat::RG8Unorm;
    case TextureFormat::Rgba8Unorm:
        return wgpu::TextureFormat::RGBA8Unorm;
    case TextureFormat::Rgba8UnormSrgb:
        return wgpu::TextureFormat::RGBA8UnormSrgb;
    case TextureFormat::Bgra8Unorm:
        return wgpu::TextureFormat::BGRA8Unorm;
    case TextureFormat::Bgra8UnormSrgb:
        return wgpu::TextureFormat::BGRA8UnormSrgb;
    case TextureFormat::R16Float:
        return wgpu::TextureFormat::R16Float;
    case TextureFormat::Rg16Float:
        return wgpu::TextureFormat::RG16Float;
    case TextureFormat::Rgba16Float:
        return wgpu::TextureFormat::RGBA16Float;
    case TextureFormat::R32Float:
        return wgpu::TextureFormat::R32Float;
    case TextureFormat::Rg32Float:
        return wgpu::TextureFormat::RG32Float;
    case TextureFormat::Rgba32Float:
        return wgpu::TextureFormat::RGBA32Float;
    case TextureFormat::R32Uint:
        return wgpu::TextureFormat::R32Uint;
    case TextureFormat::Rg32Uint:
        return wgpu::TextureFormat::RG32Uint;
    case TextureFormat::Rgba32Uint:
        return wgpu::TextureFormat::RGBA32Uint;
    case TextureFormat::Rgb10A2Unorm:
        return wgpu::TextureFormat::RGB10A2Unorm;
    case TextureFormat::Rg11B10Ufloat:
        return wgpu::TextureFormat::RG11B10Ufloat;
    case TextureFormat::Depth16Unorm:
        return wgpu::TextureFormat::Depth16Unorm;
    case TextureFormat::Depth24Plus:
        return wgpu::TextureFormat::Depth24Plus;
    case TextureFormat::Depth24PlusStencil8:
        return wgpu::TextureFormat::Depth24PlusStencil8;
    case TextureFormat::Depth32Float:
        return wgpu::TextureFormat::Depth32Float;
    default:
        return wgpu::TextureFormat::Undefined;
    }
}

wgpu::TextureDimension ToWgpu(TextureDimension dimension) noexcept
{
    switch (dimension)
    {
    case TextureDimension::Texture1D:
        return wgpu::TextureDimension::e1D;
    case TextureDimension::Texture2D:
        return wgpu::TextureDimension::e2D;
    case TextureDimension::Texture3D:
        return wgpu::TextureDimension::e3D;
    default:
        return wgpu::TextureDimension::Undefined;
    }
}

wgpu::TextureViewDimension ToWgpu(TextureViewDimension dimension) noexcept
{
    switch (dimension)
    {
    case TextureViewDimension::View1D:
        return wgpu::TextureViewDimension::e1D;
    case TextureViewDimension::View2D:
        return wgpu::TextureViewDimension::e2D;
    case TextureViewDimension::View2DArray:
        return wgpu::TextureViewDimension::e2DArray;
    case TextureViewDimension::ViewCube:
        return wgpu::TextureViewDimension::Cube;
    case TextureViewDimension::ViewCubeArray:
        return wgpu::TextureViewDimension::CubeArray;
    case TextureViewDimension::View3D:
        return wgpu::TextureViewDimension::e3D;
    default:
        return wgpu::TextureViewDimension::Undefined;
    }
}

TextureFormat FromWgpu(wgpu::TextureFormat format) noexcept
{
    switch (format)
    {
    case wgpu::TextureFormat::R8Unorm:
        return TextureFormat::R8Unorm;
    case wgpu::TextureFormat::RG8Unorm:
        return TextureFormat::Rg8Unorm;
    case wgpu::TextureFormat::RGBA8Unorm:
        return TextureFormat::Rgba8Unorm;
    case wgpu::TextureFormat::RGBA8UnormSrgb:
        return TextureFormat::Rgba8UnormSrgb;
    case wgpu::TextureFormat::BGRA8Unorm:
        return TextureFormat::Bgra8Unorm;
    case wgpu::TextureFormat::BGRA8UnormSrgb:
        return TextureFormat::Bgra8UnormSrgb;
    case wgpu::TextureFormat::R16Float:
        return TextureFormat::R16Float;
    case wgpu::TextureFormat::RG16Float:
        return TextureFormat::Rg16Float;
    case wgpu::TextureFormat::RGBA16Float:
        return TextureFormat::Rgba16Float;
    case wgpu::TextureFormat::R32Float:
        return TextureFormat::R32Float;
    case wgpu::TextureFormat::RG32Float:
        return TextureFormat::Rg32Float;
    case wgpu::TextureFormat::RGBA32Float:
        return TextureFormat::Rgba32Float;
    case wgpu::TextureFormat::R32Uint:
        return TextureFormat::R32Uint;
    case wgpu::TextureFormat::RG32Uint:
        return TextureFormat::Rg32Uint;
    case wgpu::TextureFormat::RGBA32Uint:
        return TextureFormat::Rgba32Uint;
    case wgpu::TextureFormat::RGB10A2Unorm:
        return TextureFormat::Rgb10A2Unorm;
    case wgpu::TextureFormat::RG11B10Ufloat:
        return TextureFormat::Rg11B10Ufloat;
    case wgpu::TextureFormat::Depth16Unorm:
        return TextureFormat::Depth16Unorm;
    case wgpu::TextureFormat::Depth24Plus:
        return TextureFormat::Depth24Plus;
    case wgpu::TextureFormat::Depth24PlusStencil8:
        return TextureFormat::Depth24PlusStencil8;
    case wgpu::TextureFormat::Depth32Float:
        return TextureFormat::Depth32Float;
    default:
        return TextureFormat::Invalid;
    }
}

} // namespace velox
