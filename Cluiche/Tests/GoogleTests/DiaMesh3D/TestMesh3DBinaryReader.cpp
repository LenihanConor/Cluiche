#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaMesh3D/Mesh3DBinaryReader.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaMesh3D/Testing/MeshBuilder3D.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    void BuildTempFilePath(char* pathOut, unsigned int pathOutSize, const char* filename)
    {
        char tmpDir[256];
#if defined(_MSC_VER)
        GetTempPathA(sizeof(tmpDir), tmpDir);
#else
        strncpy(tmpDir, "/tmp/", sizeof(tmpDir) - 1);
        tmpDir[sizeof(tmpDir) - 1] = '\0';
#endif
        snprintf(pathOut, pathOutSize, "%s%s", tmpDir, filename);
    }

    // Writes a 41-byte .mesh3d header with the specified magic/version/counts
    // and zero AABB. Does NOT write any vertex/index/submesh data after the
    // header, so callers that pass non-zero counts will produce a truncated
    // file (useful for CountOverflow tests where the reader checks counts
    // before reading arrays).
    bool WriteRawHeader(
        const char*   path,
        unsigned char magic0, unsigned char magic1,
        unsigned char magic2, unsigned char magic3,
        unsigned char version,
        uint32_t      vertexCount,
        uint32_t      indexCount,
        uint32_t      submeshCount)
    {
        FILE* fp = nullptr;
#if defined(_MSC_VER)
        fopen_s(&fp, path, "wb");
#else
        fp = fopen(path, "wb");
#endif
        if (!fp) return false;

        unsigned char buf[41];
        memset(buf, 0, sizeof(buf));
        buf[0] = magic0; buf[1] = magic1; buf[2] = magic2; buf[3] = magic3;
        buf[4] = version;
        memcpy(buf + 5,  &vertexCount,  4);
        memcpy(buf + 9,  &indexCount,   4);
        memcpy(buf + 13, &submeshCount, 4);
        // buf[17..40] — AABB floats — remain zero

        fwrite(buf, 1, 41, fp);
        fclose(fp);
        return true;
    }

    // Build a minimal single-triangle mesh for round-trip tests.
    void MakeMinimalMesh(
        Dia::Mesh3D::Vertex3D* vertOut,
        uint16_t*              idxOut,
        Dia::Mesh3D::Submesh*  subOut,
        Dia::Geometry3D::AABB& boundsOut)
    {
        memset(vertOut, 0, sizeof(Dia::Mesh3D::Vertex3D));
        vertOut[0].position = Dia::Maths::Vector3D(0.0f, 0.0f, 0.0f);
        vertOut[0].colour   = 0xFFFFFFFF;

        idxOut[0] = 0; idxOut[1] = 0; idxOut[2] = 0;

        subOut[0].indexStart = 0;
        subOut[0].indexCount = 3;
        subOut[0].materialId = Dia::Core::StringCRC("mat.minimal");

        boundsOut = Dia::Geometry3D::AABB(
            Dia::Maths::Vector3D(-1.0f, -2.0f, -3.0f),
            Dia::Maths::Vector3D( 1.0f,  2.0f,  3.0f));
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Mesh3DBinaryReaderTest
// ---------------------------------------------------------------------------

TEST(Mesh3DBinaryReaderTest, ValidFile_ReturnsOK)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_valid.mesh3d");

    Dia::Mesh3D::Vertex3D vert;
    uint16_t              idxs[3];
    Dia::Mesh3D::Submesh  sub;
    Dia::Geometry3D::AABB bounds;
    MakeMinimalMesh(&vert, idxs, &sub, bounds);

    ASSERT_TRUE(Dia::Mesh3D::Testing::WriteMesh3DBinary(path, &vert, 1, idxs, 3, &sub, 1, bounds));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    EXPECT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::OK);
}

TEST(Mesh3DBinaryReaderTest, ValidFile_VertexCountMatches)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_vcount.mesh3d");

    Dia::Mesh3D::Vertex3D vert;
    uint16_t              idxs[3];
    Dia::Mesh3D::Submesh  sub;
    Dia::Geometry3D::AABB bounds;
    MakeMinimalMesh(&vert, idxs, &sub, bounds);

    ASSERT_TRUE(Dia::Mesh3D::Testing::WriteMesh3DBinary(path, &vert, 1, idxs, 3, &sub, 1, bounds));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    ASSERT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::OK);
    EXPECT_EQ(result.vertexCount, 1u);
}

TEST(Mesh3DBinaryReaderTest, ValidFile_IndexCountMatches)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_icount.mesh3d");

    Dia::Mesh3D::Vertex3D vert;
    uint16_t              idxs[3];
    Dia::Mesh3D::Submesh  sub;
    Dia::Geometry3D::AABB bounds;
    MakeMinimalMesh(&vert, idxs, &sub, bounds);

    ASSERT_TRUE(Dia::Mesh3D::Testing::WriteMesh3DBinary(path, &vert, 1, idxs, 3, &sub, 1, bounds));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    ASSERT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::OK);
    EXPECT_EQ(result.indexCount, 3u);
}

TEST(Mesh3DBinaryReaderTest, ValidFile_SubmeshCountMatches)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_scount.mesh3d");

    Dia::Mesh3D::Vertex3D vert;
    uint16_t              idxs[3];
    Dia::Mesh3D::Submesh  sub;
    Dia::Geometry3D::AABB bounds;
    MakeMinimalMesh(&vert, idxs, &sub, bounds);

    ASSERT_TRUE(Dia::Mesh3D::Testing::WriteMesh3DBinary(path, &vert, 1, idxs, 3, &sub, 1, bounds));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    ASSERT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::OK);
    EXPECT_EQ(result.submeshCount, 1u);
}

TEST(Mesh3DBinaryReaderTest, ValidFile_AABBMatches)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_aabb.mesh3d");

    Dia::Mesh3D::Vertex3D vert;
    uint16_t              idxs[3];
    Dia::Mesh3D::Submesh  sub;
    Dia::Geometry3D::AABB bounds;
    MakeMinimalMesh(&vert, idxs, &sub, bounds);

    ASSERT_TRUE(Dia::Mesh3D::Testing::WriteMesh3DBinary(path, &vert, 1, idxs, 3, &sub, 1, bounds));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    ASSERT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::OK);
    EXPECT_FLOAT_EQ(result.bounds.GetMin().x, -1.0f);
    EXPECT_FLOAT_EQ(result.bounds.GetMin().y, -2.0f);
    EXPECT_FLOAT_EQ(result.bounds.GetMin().z, -3.0f);
    EXPECT_FLOAT_EQ(result.bounds.GetMax().x,  1.0f);
    EXPECT_FLOAT_EQ(result.bounds.GetMax().y,  2.0f);
    EXPECT_FLOAT_EQ(result.bounds.GetMax().z,  3.0f);
}

TEST(Mesh3DBinaryReaderTest, BadMagic_ReturnsBadMagic)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_badmagic.mesh3d");

    // Valid version and zero counts; bad magic 'XXXX'
    ASSERT_TRUE(WriteRawHeader(path, 'X', 'X', 'X', 'X', 1, 0, 0, 0));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    EXPECT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::BadMagic);
}

TEST(Mesh3DBinaryReaderTest, BadVersion_ReturnsBadVersion)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_badversion.mesh3d");

    // Correct magic 'MESH', wrong version (99), zero counts
    ASSERT_TRUE(WriteRawHeader(path, 'M', 'E', 'S', 'H', 99, 0, 0, 0));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    EXPECT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::BadVersion);
}

TEST(Mesh3DBinaryReaderTest, CountOverflow_Vertices_ReturnsCountOverflow)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "mesh3d_test_overflow.mesh3d");

    // vertexCount = 65536, which exceeds kMaxVertices (65535)
    const uint32_t overflowCount = 65536u;
    ASSERT_TRUE(WriteRawHeader(path, 'M', 'E', 'S', 'H', 1, overflowCount, 0, 0));

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(path);

    EXPECT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::CountOverflow);
}

TEST(Mesh3DBinaryReaderTest, MissingFile_ReturnsReadError)
{
    const char* nonExistentPath = "C:/this/path/does/not/exist/missing.mesh3d";

    Dia::Mesh3D::ReadResult result = Dia::Mesh3D::ReadMesh3DFile(nonExistentPath);

    EXPECT_EQ(result.status, Dia::Mesh3D::ReadResult::Status::ReadError);
}
