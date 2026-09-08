#pragma once

#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace Dia { namespace Mesh3D { namespace Testing {

// Writes a valid .mesh3d binary file to the given path.
// Returns true on success.
inline bool WriteMesh3DBinary(
    const char* filePath,
    const Vertex3D* vertices,  uint32_t vertexCount,
    const uint16_t* indices,   uint32_t indexCount,
    const Submesh*  submeshes, uint32_t submeshCount,
    const Dia::Geometry3D::AABB& bounds)
{
    FILE* fp = nullptr;
#if defined(_MSC_VER)
    fopen_s(&fp, filePath, "wb");
#else
    fp = fopen(filePath, "wb");
#endif
    if (!fp) return false;

    // Header
    unsigned char buf[41];
    memset(buf, 0, sizeof(buf));
    buf[0] = 'M'; buf[1] = 'E'; buf[2] = 'S'; buf[3] = 'H';
    buf[4] = 1; // version
    memcpy(buf + 5,  &vertexCount,  4);
    memcpy(buf + 9,  &indexCount,   4);
    memcpy(buf + 13, &submeshCount, 4);
    float minX = bounds.GetMin().x; float minY = bounds.GetMin().y; float minZ = bounds.GetMin().z;
    float maxX = bounds.GetMax().x; float maxY = bounds.GetMax().y; float maxZ = bounds.GetMax().z;
    memcpy(buf + 17, &minX, 4); memcpy(buf + 21, &minY, 4); memcpy(buf + 25, &minZ, 4);
    memcpy(buf + 29, &maxX, 4); memcpy(buf + 33, &maxY, 4); memcpy(buf + 37, &maxZ, 4);
    fwrite(buf, 1, 41, fp);

    // Data
    if (vertexCount  > 0) fwrite(vertices,  sizeof(Vertex3D), vertexCount,  fp);
    if (indexCount   > 0) fwrite(indices,   sizeof(uint16_t), indexCount,   fp);
    if (submeshCount > 0) fwrite(submeshes, sizeof(Submesh),  submeshCount, fp);

    fclose(fp);
    return true;
}

} } }
