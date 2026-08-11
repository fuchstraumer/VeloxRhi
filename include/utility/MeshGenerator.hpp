#pragma once
#ifndef VELOX_UTILITY_MESH_GENERATOR_HPP
#define VELOX_UTILITY_MESH_GENERATOR_HPP
#include <memory>

namespace velox
{

struct MeshData
{
    size_t vertexCount{ 0 };
    size_t vertexStrideBytes{ 0 };
    std::unique_ptr<float[]> vertices;
    // need to describe vertex layout somehow...
    size_t indexCount{ 0 };
    std::unique_ptr<uint32_t[]> indicesU32;
    std::unique_ptr<uint16_t[]> indicesU16;
};

std::unique_ptr<MeshData> GenerateBox(uint32_t widthSegments,
                                      uint32_t heightSegments,
                                      uint32_t depthSegments) noexcept;

std::unique_ptr<MeshData> GenerateIcosphere(uint32_t subdivisions) noexcept;

} // namespace velox

#endif // !VELOX_UTILITY_MESH_GENERATOR_HPP