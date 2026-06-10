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

// Reads a .mesh3d file from disk into result. The file path must be a
// null-terminated UTF-8 string. All validation is performed before any
// data is written to result.
ReadResult ReadMesh3DFile(const char* filePath);

} }
