#pragma once

#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <stdint.h>

namespace Dia { namespace Mesh3D {

struct ReadResult
{
    enum class Status : unsigned char { OK = 0, BadMagic = 1, BadVersion = 2, CountOverflow = 3, ReadError = 4 };

    Status   status       = Status::OK;
    uint32_t vertexCount  = 0;
    uint32_t indexCount   = 0;
    uint32_t submeshCount = 0;

    // Staging buffers — only valid when status == OK
    Vertex3D vertices [Mesh3DAsset::kMaxVertices];
    uint16_t indices  [Mesh3DAsset::kMaxIndices];
    Submesh  submeshes[Mesh3DAsset::kMaxSubmeshes];
    Dia::Geometry3D::AABB bounds;
};

// Reads a .mesh3d file directly into a pre-allocated ReadResult.
// Avoids ~4MB stack allocation by writing into caller-owned memory.
void ReadMesh3DFile(const char* filePath, ReadResult* outResult);

// Convenience overload — allocates ReadResult on the stack (only safe
// when calling from a thread with sufficient stack size).
ReadResult ReadMesh3DFile(const char* filePath);

} }
