#include <DiaMesh3D/Mesh3DBinaryReader.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaObservation/Log/DiaLog.h>

#include <stdio.h>
#include <string.h>

namespace Dia { namespace Mesh3D {

static const unsigned char kMagic[4] = { 'M', 'E', 'S', 'H' };
static const unsigned char kVersion  = 1;
static const unsigned int  kHeaderSize = 41;

void ReadMesh3DFile(const char* filePath, ReadResult* outResult)
{
#if defined(_MSC_VER)
    FILE* fp = nullptr;
    fopen_s(&fp, filePath, "rb");
#else
    FILE* fp = fopen(filePath, "rb");
#endif

    if (!fp)
    {
        DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: failed to open '%s'", filePath);
        outResult->status = ReadResult::Status::ReadError;
        return;
    }

    // Read 41-byte header
    unsigned char buf[kHeaderSize];
    if (fread(buf, 1, kHeaderSize, fp) != kHeaderSize)
    {
        DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: incomplete header in '%s'", filePath);
        fclose(fp);
        outResult->status = ReadResult::Status::ReadError;
        return;
    }

    // Validate magic bytes
    if (buf[0] != kMagic[0] || buf[1] != kMagic[1] ||
        buf[2] != kMagic[2] || buf[3] != kMagic[3])
    {
        DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: bad magic in '%s' (got 0x%02X%02X%02X%02X)",
                      filePath, buf[0], buf[1], buf[2], buf[3]);
        fclose(fp);
        outResult->status = ReadResult::Status::BadMagic;
        return;
    }

    // Validate version
    if (buf[4] != kVersion)
    {
        DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: unsupported version %u in '%s'",
                      static_cast<unsigned>(buf[4]), filePath);
        fclose(fp);
        outResult->status = ReadResult::Status::BadVersion;
        return;
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
        DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: count overflow in '%s' (v=%u i=%u s=%u)",
                      filePath, vertexCount, indexCount, submeshCount);
        fclose(fp);
        outResult->status = ReadResult::Status::CountOverflow;
        return;
    }

    // Read vertex data
    if (vertexCount > 0)
    {
        size_t bytesToRead = vertexCount * sizeof(Vertex3D);
        if (fread(outResult->vertices, 1, bytesToRead, fp) != bytesToRead)
        {
            DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: truncated vertex data in '%s'", filePath);
            fclose(fp);
            outResult->status = ReadResult::Status::ReadError;
            return;
        }
    }

    // Read index data
    if (indexCount > 0)
    {
        size_t bytesToRead = indexCount * sizeof(uint16_t);
        if (fread(outResult->indices, 1, bytesToRead, fp) != bytesToRead)
        {
            DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: truncated index data in '%s'", filePath);
            fclose(fp);
            outResult->status = ReadResult::Status::ReadError;
            return;
        }
    }

    // Read submesh data
    if (submeshCount > 0)
    {
        size_t bytesToRead = submeshCount * sizeof(Submesh);
        if (fread(outResult->submeshes, 1, bytesToRead, fp) != bytesToRead)
        {
            DIA_LOG_ERROR("DiaMesh3D", "ReadMesh3DFile: truncated submesh data in '%s'", filePath);
            fclose(fp);
            outResult->status = ReadResult::Status::ReadError;
            return;
        }
    }

    fclose(fp);

    outResult->vertexCount  = vertexCount;
    outResult->indexCount   = indexCount;
    outResult->submeshCount = submeshCount;
    outResult->bounds       = Dia::Geometry3D::AABB(
                              Dia::Maths::Vector3D(aabbMinX, aabbMinY, aabbMinZ),
                              Dia::Maths::Vector3D(aabbMaxX, aabbMaxY, aabbMaxZ));
    outResult->status       = ReadResult::Status::OK;
}

ReadResult ReadMesh3DFile(const char* filePath)
{
    ReadResult result;
    ReadMesh3DFile(filePath, &result);
    return result;
}

} }
