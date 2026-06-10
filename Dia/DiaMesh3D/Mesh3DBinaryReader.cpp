#include <DiaMesh3D/Mesh3DBinaryReader.h>
#include <DiaMaths/Vector/Vector3D.h>

#include <stdio.h>
#include <string.h>

namespace Dia { namespace Mesh3D {

static const unsigned char kMagic[4] = { 'M', 'E', 'S', 'H' };
static const unsigned char kVersion  = 1;
static const unsigned int  kHeaderSize = 41;

ReadResult ReadMesh3DFile(const char* filePath)
{
    ReadResult result;

#if defined(_MSC_VER)
    FILE* fp = nullptr;
    fopen_s(&fp, filePath, "rb");
#else
    FILE* fp = fopen(filePath, "rb");
#endif

    if (!fp)
    {
        result.status = ReadResult::Status::ReadError;
        return result;
    }

    // Read 41-byte header
    unsigned char buf[kHeaderSize];
    if (fread(buf, 1, kHeaderSize, fp) != kHeaderSize)
    {
        fclose(fp);
        result.status = ReadResult::Status::ReadError;
        return result;
    }

    // Validate magic bytes
    if (buf[0] != kMagic[0] || buf[1] != kMagic[1] ||
        buf[2] != kMagic[2] || buf[3] != kMagic[3])
    {
        fclose(fp);
        result.status = ReadResult::Status::BadMagic;
        return result;
    }

    // Validate version
    if (buf[4] != kVersion)
    {
        fclose(fp);
        result.status = ReadResult::Status::BadVersion;
        return result;
    }

    // Read counts
    uint32_t vertexCount  = 0;
    uint32_t indexCount   = 0;
    uint32_t submeshCount = 0;
    memcpy(&vertexCount,  buf + 5,  sizeof(uint32_t));
    memcpy(&indexCount,   buf + 9,  sizeof(uint32_t));
    memcpy(&submeshCount, buf + 13, sizeof(uint32_t));

    // Read AABB floats
    float aabbMinX = 0.0f; float aabbMinY = 0.0f; float aabbMinZ = 0.0f;
    float aabbMaxX = 0.0f; float aabbMaxY = 0.0f; float aabbMaxZ = 0.0f;
    memcpy(&aabbMinX, buf + 17, sizeof(float));
    memcpy(&aabbMinY, buf + 21, sizeof(float));
    memcpy(&aabbMinZ, buf + 25, sizeof(float));
    memcpy(&aabbMaxX, buf + 29, sizeof(float));
    memcpy(&aabbMaxY, buf + 33, sizeof(float));
    memcpy(&aabbMaxZ, buf + 37, sizeof(float));

    // Validate counts against kMax* constants
    if (vertexCount  > Mesh3DAsset::kMaxVertices  ||
        indexCount   > Mesh3DAsset::kMaxIndices   ||
        submeshCount > Mesh3DAsset::kMaxSubmeshes)
    {
        fclose(fp);
        result.status = ReadResult::Status::CountOverflow;
        return result;
    }

    // Read vertex data
    if (vertexCount > 0)
    {
        size_t bytesToRead = vertexCount * sizeof(Vertex3D);
        if (fread(result.vertices, 1, bytesToRead, fp) != bytesToRead)
        {
            fclose(fp);
            result.status = ReadResult::Status::ReadError;
            return result;
        }
    }

    // Read index data
    if (indexCount > 0)
    {
        size_t bytesToRead = indexCount * sizeof(uint16_t);
        if (fread(result.indices, 1, bytesToRead, fp) != bytesToRead)
        {
            fclose(fp);
            result.status = ReadResult::Status::ReadError;
            return result;
        }
    }

    // Read submesh data
    if (submeshCount > 0)
    {
        size_t bytesToRead = submeshCount * sizeof(Submesh);
        if (fread(result.submeshes, 1, bytesToRead, fp) != bytesToRead)
        {
            fclose(fp);
            result.status = ReadResult::Status::ReadError;
            return result;
        }
    }

    fclose(fp);

    result.vertexCount  = vertexCount;
    result.indexCount   = indexCount;
    result.submeshCount = submeshCount;
    result.bounds       = Dia::Geometry3D::AABB(
                              Dia::Maths::Vector3D(aabbMinX, aabbMinY, aabbMinZ),
                              Dia::Maths::Vector3D(aabbMaxX, aabbMaxY, aabbMaxZ));
    result.status       = ReadResult::Status::OK;

    return result;
}

} }
