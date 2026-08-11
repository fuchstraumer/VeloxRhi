#include "utility/MeshGenerator.hpp"
#include "math/Math.hpp"
#include <array>
#include <span>
#include <vector>
#include <algorithm>
#include <unordered_map>

// ripped from my age-old code here:
// https://github.com/fuchstraumer/DiamondDogs/blob/master/modules/ContentCompiler/src/MeshPrimitives.cpp
namespace velox
{

using namespace math;

static const size_t k_MeshVertexStrideBytes = sizeof(float) * 14;;
static const size_t k_MeshVertexStrideFloats = k_MeshVertexStrideBytes / sizeof(float);

static const std::array<Float4, 8> BOX_POSITIONS
{
    Float4{ -1.0f, -1.0f, +1.0f, 1.0f }, 
    Float4{ +1.0f, -1.0f, +1.0f, 1.0f }, 
    Float4{ +1.0f, +1.0f, +1.0f, 1.0f }, 
    Float4{ -1.0f, +1.0f, +1.0f, 1.0f },
    Float4{ +1.0f, -1.0f, -1.0f, 1.0f }, 
    Float4{ -1.0f, -1.0f, -1.0f, 1.0f }, 
    Float4{ -1.0f, +1.0f, -1.0f, 1.0f }, 
    Float4{ +1.0f, +1.0f, -1.0f, 1.0f },
};
    
static const std::array<Float4, 6> BOX_NORMALS
{
    Float4{  0.0f,  0.0f,  1.0f, 0.0f },
    Float4{  1.0f,  0.0f,  0.0f, 0.0f },
    Float4{  0.0f,  1.0f,  0.0f, 0.0f },
    Float4{ -1.0f,  0.0f,  0.0f, 0.0f },
    Float4{  0.0f, -1.0f,  0.0f, 0.0f },
    Float4{  0.0f,  0.0f, -1.0f, 0.0f }
};

static const std::array<Float4, 6> BOX_TANGENTS
{
    Float4{  1.0f,  0.0f,  0.0f, 1.0f },
    Float4{  0.0f,  0.0f, -1.0f, 1.0f },
    Float4{  1.0f,  0.0f,  0.0f, 1.0f },
    Float4{  0.0f,  0.0f, +1.0f, 1.0f },
    Float4{ -1.0f,  0.0f,  0.0f, 1.0f },
    Float4{ -1.0f,  0.0f,  0.0f, 1.0f }
};

static const std::array<Float2, 4> BOX_UVS
{
    Float2{ 0.0f, 0.0f },
    Float2{ 1.0f, 0.0f },
    Float2{ 1.0f, 1.0f },
    Float2{ 0.0f, 1.0f }
};

constexpr size_t BOX_FACE_INDICES[6][4]
{
    {0, 1, 2, 3}, // Face 0
    {1, 4, 7, 2}, // Face 1
    {3, 2, 7, 6}, // Face 2
    {5, 0, 3, 6}, // Face 3
    {5, 4, 1, 0}, // Face 4
    {4, 5, 6, 7}  // Face 5
};

std::unique_ptr<MeshData> GenerateBox(uint32_t widthSegments,
                                      uint32_t heightSegments,
                                      uint32_t depthSegments) noexcept
{
    MeshData mesh;
    
    mesh.vertexCount = 24; // 6 faces * 4 vertices
    mesh.indexCount  = 36; // 6 faces * 2 triangles * 3 indices
    
    // Layout: Pos(4) + Normal(4) + Tangent(4) + UV(2) = 14 floats per vertex
    mesh.vertexStrideBytes = k_MeshVertexStrideBytes;

    mesh.vertices = std::make_unique<float[]>(mesh.vertexCount * k_MeshVertexStrideFloats);
    mesh.indicesU16 = std::make_unique<uint16_t[]>(mesh.indexCount);
    mesh.indicesU32 = nullptr;

    size_t vOffset = 0;
    size_t iOffset = 0;

    for (size_t face = 0; face < 6; ++face)
    {
        // 1. Generate the 4 vertices for this face
        for (size_t v = 0; v < 4; ++v)
        {
            size_t posIdx = BOX_FACE_INDICES[face][v];

            // Position (Aligned to 4 floats)
            mesh.vertices[vOffset++] = BOX_POSITIONS[posIdx].x;
            mesh.vertices[vOffset++] = BOX_POSITIONS[posIdx].y;
            mesh.vertices[vOffset++] = BOX_POSITIONS[posIdx].z;
            mesh.vertices[vOffset++] = 1.0f; // w

            // Normal (Aligned to 4 floats)
            mesh.vertices[vOffset++] = BOX_NORMALS[face].x;
            mesh.vertices[vOffset++] = BOX_NORMALS[face].y;
            mesh.vertices[vOffset++] = BOX_NORMALS[face].z;
            mesh.vertices[vOffset++] = 0.0f; // w direction vector

            // Tangent (Aligned to 4 floats)
            mesh.vertices[vOffset++] = BOX_TANGENTS[face].x;
            mesh.vertices[vOffset++] = BOX_TANGENTS[face].y;
            mesh.vertices[vOffset++] = BOX_TANGENTS[face].z;
            mesh.vertices[vOffset++] = 1.0f; // w sign bit

            // UV (2 floats)
            mesh.vertices[vOffset++] = BOX_UVS[v].u;
            mesh.vertices[vOffset++] = BOX_UVS[v].v;
        }

        // 2. Generate the 6 indices for the two triangles of this face
        // We do this relative to the base index of this face's vertices
        uint16_t baseIdx = static_cast<uint16_t>(face * 4);
        
        // Triangle 1: 0, 1, 2
        mesh.indicesU16[iOffset++] = baseIdx + 0;
        mesh.indicesU16[iOffset++] = baseIdx + 1;
        mesh.indicesU16[iOffset++] = baseIdx + 2;
        
        // Triangle 2: 0, 2, 3
        mesh.indicesU16[iOffset++] = baseIdx + 0;
        mesh.indicesU16[iOffset++] = baseIdx + 2;
        mesh.indicesU16[iOffset++] = baseIdx + 3;
    }

    return std::make_unique<MeshData>(std::move(mesh));

}

static constexpr float GOLDEN_RATIO = 1.61803398875f;
static constexpr float FLOAT_PI = 3.141592653589793238462f;

std::unique_ptr<MeshData> CreateIcosphere(size_t detail_level)
{
    // 1. Initial 12 positions of the Icosahedron
    std::vector<Float4> positions
    {
        {-GOLDEN_RATIO,  1.0f,  0.0f, 1.0f}, { GOLDEN_RATIO,  1.0f,  0.0f, 1.0f},
        {-GOLDEN_RATIO, -1.0f,  0.0f, 1.0f}, { GOLDEN_RATIO, -1.0f,  0.0f, 1.0f},
        { 0.0f, -GOLDEN_RATIO,  1.0f, 1.0f}, { 0.0f,  GOLDEN_RATIO,  1.0f, 1.0f},
        { 0.0f, -GOLDEN_RATIO, -1.0f, 1.0f}, { 0.0f,  GOLDEN_RATIO, -1.0f, 1.0f},
        { 1.0f,  0.0f, -GOLDEN_RATIO, 1.0f}, { 1.0f,  0.0f,  GOLDEN_RATIO, 1.0f},
        {-1.0f,  0.0f, -GOLDEN_RATIO, 1.0f}, {-1.0f,  0.0f,  GOLDEN_RATIO, 1.0f}
    };

    std::vector<uint16_t> indices
    {
         0, 11,  5,    0,  5,  1,    0,  1,  7,    0,  7, 10,    0, 10, 11,
         1,  5,  9,    5, 11,  4,   11, 10,  2,   10,  7,  6,    7,  1,  8,
         3,  9,  4,    3,  4,  2,    3,  2,  6,    3,  6,  8,    3,  8,  9,
         4,  9,  5,    2,  4, 11,    6,  2, 10,    8,  6,  7,    9,  8,  1
    };

    // Normalize<3> divides every lane by the xyz length, w included, so w has to be restored
    std::transform(positions.begin(), positions.end(), positions.begin(), [](const Float4& p)
    {
        Float4 onSphere = FromVector(ToVector(p).Normalize<3>());
        onSphere.w = 1.0f;
        return onSphere;
    });

    // 2. Subdivide using an Edge Cache to avoid duplicate vertices
    std::unordered_map<uint32_t, uint16_t> edgeCache;
    auto getMidpoint = [&](uint16_t v1, uint16_t v2) -> uint16_t
    {
        uint16_t smaller = std::min(v1, v2);
        uint16_t greater = std::max(v1, v2);
        uint32_t key = (static_cast<uint32_t>(smaller) << 16) | greater;

        if (edgeCache.find(key) != edgeCache.end())
        {
            return edgeCache[key];
        }

        Vector pos1 = ToVector(positions[v1]);
        Vector pos2 = ToVector(positions[v2]);
        // <3>, not <4>: both operands have w == 1, so their sum has w == 2, and Normalize<4> would
        // divide by sqrt(x^2 + y^2 + z^2 + 4), pulling the midpoint inside the unit sphere
        Vector mid = (pos1 + pos2).Normalize<3>();
        Float4 midpoint = FromVector(mid);
        midpoint.w = 1.0f;
        positions.push_back(midpoint);
        uint16_t index = static_cast<uint16_t>(positions.size() - 1);
        edgeCache[key] = index;
        return index;
    };

    for (size_t i = 0; i < detail_level; ++i)
    {
        std::vector<uint16_t> nextIndices;
        nextIndices.reserve(indices.size() * 4); // Each triangle becomes 4

        for (size_t j = 0; j < indices.size(); j += 3)
        {
            uint16_t v0 = indices[j + 0];
            uint16_t v1 = indices[j + 1];
            uint16_t v2 = indices[j + 2];

            uint16_t a = getMidpoint(v0, v1);
            uint16_t b = getMidpoint(v1, v2);
            uint16_t c = getMidpoint(v2, v0);
            uint16_t new_tris[] = { v0, a, c,   v1, b, a,   v2, c, b,   a, b, c };
            std::span<const uint16_t> new_tris_span(new_tris, std::size(new_tris));

            // use std::move_iterators to avoid copying the array into the vector
            nextIndices.insert(nextIndices.end(),
                               std::make_move_iterator(new_tris_span.begin()),
                               std::make_move_iterator(new_tris_span.end()));
        }
        indices = std::move(nextIndices);
    }

    // 3. Generate UVs and Fix Seams (by actually duplicating vertices)
    std::vector<Float2> uvs(positions.size());
    for (size_t i = 0; i < positions.size(); ++i)
    {
        const auto& p = positions[i];
        uvs[i].u = (std::atan2(p.x, -p.z) / FLOAT_PI) * 0.5f + 0.5f;
        uvs[i].v = -p.y * 0.5f + 0.5f;
    }

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        uint16_t i0 = indices[i + 0];
        uint16_t i1 = indices[i + 1];
        uint16_t i2 = indices[i + 2];

        float d1 = uvs[i1].u - uvs[i0].u;
        float d2 = uvs[i2].u - uvs[i0].u;

        // If the triangle spans more than half the UV space, it crosses the seam
        if (std::abs(d1) > 0.5f || std::abs(d2) > 0.5f)
        {
            // For any vertex on the "wrong" side of the seam, duplicate it and offset UV
            auto fixSeamVertex = [&](uint16_t& idx)
            { 
                // If it's on the left edge, wrap it to the right
                if (uvs[idx].u < 0.25f)
                {
                    positions.push_back(positions[idx]);
                    uvs.push_back({ uvs[idx].u + 1.0f, uvs[idx].v });
                    idx = static_cast<uint16_t>(positions.size() - 1);
                }
            };
            fixSeamVertex(indices[i + 0]);
            fixSeamVertex(indices[i + 1]);
            fixSeamVertex(indices[i + 2]);
        }
    }

    // 4. Pack into the final flattened MeshData buffer
    std::unique_ptr<MeshData> mesh = std::make_unique<MeshData>();
    mesh->vertexCount = positions.size();
    mesh->indexCount = indices.size();
    mesh->vertexStrideBytes = 14 * sizeof(float); // Pos(4) + Normal(4) + Tan(4) + UV(2)

    mesh->vertices = std::make_unique<float[]>(mesh->vertexCount * (mesh->vertexStrideBytes / sizeof(float)));
    mesh->indicesU16 = std::make_unique<uint16_t[]>(mesh->indexCount);
    mesh->indicesU32 = nullptr;

    // Copy indices
    std::copy(indices.begin(), indices.end(), mesh->indicesU16.get());

    // Interleave vertex data
    size_t offset = 0;
    for (size_t i = 0; i < mesh->vertexCount; ++i)
    {
        const auto& p = positions[i];
        
        // Position (w=1.0)
        mesh->vertices[offset++] = p.x;
        mesh->vertices[offset++] = p.y;
        mesh->vertices[offset++] = p.z;
        mesh->vertices[offset++] = 1.0f;

        // Normal (For a unit sphere at origin, normal == position, but w=0.0)
        mesh->vertices[offset++] = p.x;
        mesh->vertices[offset++] = p.y;
        mesh->vertices[offset++] = p.z;
        mesh->vertices[offset++] = 0.0f; 

        // Tangent placeholder (w=1.0 for sign)
        mesh->vertices[offset++] = 0.0f;
        mesh->vertices[offset++] = 0.0f;
        mesh->vertices[offset++] = 0.0f;
        mesh->vertices[offset++] = 1.0f;

        // UV
        mesh->vertices[offset++] = uvs[i].u;
        mesh->vertices[offset++] = uvs[i].v;
    }

    return mesh;
}

} // namespace velox
