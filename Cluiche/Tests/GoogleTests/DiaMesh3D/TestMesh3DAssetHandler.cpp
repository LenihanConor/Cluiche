#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaMesh3D/Testing/MeshBuilder3D.h>
#include <DiaAsset/IAssetTypeHandler.h>
#include <DiaThreading/JobSystem.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Strings/String512.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>

#include <string.h>
#include <stdio.h>

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

    struct TestCallback : public Dia::AssetRuntime::IAssetLoadCallback
    {
        bool loadComplete = false;
        bool loadFailed   = false;
        Dia::Core::StringCRC lastId;
        char failReason[128] = {};

        void OnLoadComplete(const Dia::Core::StringCRC& assetId) override
        {
            loadComplete = true;
            lastId = assetId;
        }

        void OnLoadFailed(const Dia::Core::StringCRC& assetId, const char* reason) override
        {
            loadFailed = true;
            lastId = assetId;
            if (reason)
            {
                strncpy(failReason, reason, sizeof(failReason) - 1);
                failReason[sizeof(failReason) - 1] = '\0';
            }
        }
    };

    bool WriteUnitCube(const char* path)
    {
        Dia::Mesh3D::Vertex3D verts[8];
        memset(verts, 0, sizeof(verts));

        const float p = 0.5f;
        const float n = -0.5f;
        verts[0].position = Dia::Maths::Vector3D(n, n, n); verts[0].colour = 0xFFFFFFFF;
        verts[1].position = Dia::Maths::Vector3D(p, n, n); verts[1].colour = 0xFFFFFFFF;
        verts[2].position = Dia::Maths::Vector3D(p, p, n); verts[2].colour = 0xFFFFFFFF;
        verts[3].position = Dia::Maths::Vector3D(n, p, n); verts[3].colour = 0xFFFFFFFF;
        verts[4].position = Dia::Maths::Vector3D(n, n, p); verts[4].colour = 0xFFFFFFFF;
        verts[5].position = Dia::Maths::Vector3D(p, n, p); verts[5].colour = 0xFFFFFFFF;
        verts[6].position = Dia::Maths::Vector3D(p, p, p); verts[6].colour = 0xFFFFFFFF;
        verts[7].position = Dia::Maths::Vector3D(n, p, p); verts[7].colour = 0xFFFFFFFF;

        uint16_t indices[36] = {
            0,1,2, 0,2,3,
            4,5,6, 4,6,7,
            0,1,5, 0,5,4,
            2,3,7, 2,7,6,
            0,3,7, 0,7,4,
            1,2,6, 1,6,5
        };

        Dia::Mesh3D::Submesh sub;
        sub.indexStart = 0;
        sub.indexCount = 36;
        sub.materialId = Dia::Core::StringCRC("mat.default");

        Dia::Geometry3D::AABB bounds(
            Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f),
            Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f));

        return Dia::Mesh3D::Testing::WriteMesh3DBinary(
            path, verts, 8, indices, 36, &sub, 1, bounds);
    }

    bool WriteRawHeaderOnly(
        const char* path,
        uint32_t vertexCount,
        uint32_t indexCount,
        uint32_t submeshCount)
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
        buf[0] = 'M'; buf[1] = 'E'; buf[2] = 'S'; buf[3] = 'H';
        buf[4] = 1;
        memcpy(buf + 5,  &vertexCount,  4);
        memcpy(buf + 9,  &indexCount,   4);
        memcpy(buf + 13, &submeshCount, 4);

        fwrite(buf, 1, 41, fp);
        fclose(fp);
        return true;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Mesh3DAssetHandlerTest
// ---------------------------------------------------------------------------

class Mesh3DAssetHandlerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mJobSystem.Initialize(2);
        mHandler.SetJobSystem(&mJobSystem);
    }

    void TearDown() override
    {
        mJobSystem.Shutdown();
    }

    Dia::Core::JobSystem          mJobSystem;
    Dia::Mesh3D::Mesh3DAssetHandler mHandler;
};

TEST_F(Mesh3DAssetHandlerTest, Load_ValidFile_AssetBecomesReady)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_valid.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.handler_test");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    EXPECT_TRUE(callback.loadComplete);
    EXPECT_FALSE(callback.loadFailed);

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    EXPECT_TRUE(asset->IsReady());
}

TEST_F(Mesh3DAssetHandlerTest, Load_ValidFile_AABBIsCorrect)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_aabb.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.handler_aabb");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->IsReady());

    EXPECT_FLOAT_EQ(asset->GetBounds().GetMin().x, -0.5f);
    EXPECT_FLOAT_EQ(asset->GetBounds().GetMin().y, -0.5f);
    EXPECT_FLOAT_EQ(asset->GetBounds().GetMin().z, -0.5f);
    EXPECT_FLOAT_EQ(asset->GetBounds().GetMax().x,  0.5f);
    EXPECT_FLOAT_EQ(asset->GetBounds().GetMax().y,  0.5f);
    EXPECT_FLOAT_EQ(asset->GetBounds().GetMax().z,  0.5f);
}

TEST_F(Mesh3DAssetHandlerTest, Load_ValidFile_VertexAndIndexCounts)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_counts.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.handler_counts");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    ASSERT_TRUE(asset->IsReady());
    EXPECT_EQ(asset->GetVertices().Size(), 8u);
    EXPECT_EQ(asset->GetIndices().Size(), 36u);
    EXPECT_EQ(asset->GetSubmeshes().Size(), 1u);
}

TEST_F(Mesh3DAssetHandlerTest, Unload_RemovesAsset)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_unload.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.handler_unload");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();
    ASSERT_NE(mHandler.LookupMesh(assetId), nullptr);
    ASSERT_EQ(mHandler.GetLoadedCount(), 1u);

    mHandler.Unload(assetId);

    EXPECT_EQ(mHandler.LookupMesh(assetId), nullptr);
    EXPECT_EQ(mHandler.GetLoadedCount(), 0u);
}

TEST_F(Mesh3DAssetHandlerTest, Load_DoubleLoad_DoesNotCrash)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_double.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.handler_double");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback1;
    TestCallback callback2;

    mHandler.Load(assetId, resolvedPath, &callback1);
    mHandler.Tick();
    ASSERT_TRUE(callback1.loadComplete);

    // Second Load of same asset — should fire callback immediately (already ready)
    mHandler.Load(assetId, resolvedPath, &callback2);
    EXPECT_TRUE(callback2.loadComplete);
    EXPECT_EQ(mHandler.GetLoadedCount(), 1u);
}

TEST_F(Mesh3DAssetHandlerTest, Load_BadFile_FiresOnLoadFailed)
{
    const char* fakePath = "C:/nonexistent/path/bad.mesh3d";
    Dia::Core::StringCRC assetId("mesh3d.handler_badfile");
    Dia::Core::Containers::String512 resolvedPath(fakePath);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    EXPECT_FALSE(callback.loadComplete);
    EXPECT_TRUE(callback.loadFailed);

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetState(), Dia::Mesh3D::Mesh3DAsset::State::Failed);
}

TEST_F(Mesh3DAssetHandlerTest, Load_IndexCountOverflow_FiresOnLoadFailed)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_idxoverflow.mesh3d");
    // Write header with index count exceeding kMaxIndices (196608)
    ASSERT_TRUE(WriteRawHeaderOnly(path, 0, 200000, 0));

    Dia::Core::StringCRC assetId("mesh3d.handler_idxoverflow");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    EXPECT_FALSE(callback.loadComplete);
    EXPECT_TRUE(callback.loadFailed);

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetState(), Dia::Mesh3D::Mesh3DAsset::State::Failed);
}

TEST_F(Mesh3DAssetHandlerTest, Load_SubmeshCountOverflow_FiresOnLoadFailed)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_suboverflow.mesh3d");
    // Write header with submesh count exceeding kMaxSubmeshes (32)
    ASSERT_TRUE(WriteRawHeaderOnly(path, 0, 0, 64));

    Dia::Core::StringCRC assetId("mesh3d.handler_suboverflow");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    mHandler.Load(assetId, resolvedPath, &callback);
    mHandler.Tick();

    EXPECT_FALSE(callback.loadComplete);
    EXPECT_TRUE(callback.loadFailed);

    Dia::Mesh3D::Mesh3DAsset* asset = mHandler.LookupMesh(assetId);
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetState(), Dia::Mesh3D::Mesh3DAsset::State::Failed);
}

// ---------------------------------------------------------------------------
// RegisterMesh — inject a pre-built asset (no disk load, no JobSystem)
// ---------------------------------------------------------------------------

TEST_F(Mesh3DAssetHandlerTest, RegisterMesh_LookupReturnsAsset)
{
    Dia::Core::StringCRC assetId("mesh3d.register_basic");
    Dia::Mesh3D::Mesh3DAsset* asset = new Dia::Mesh3D::Mesh3DAsset(assetId);

    mHandler.RegisterMesh(asset);

    EXPECT_EQ(mHandler.LookupMesh(assetId), asset);
    EXPECT_EQ(mHandler.GetLoadedCount(), 1u);
    // Handler owns the asset now — freed in TearDown via the handler destructor.
}

TEST_F(Mesh3DAssetHandlerTest, RegisterMesh_DuplicateIdReplacesPrevious)
{
    Dia::Core::StringCRC assetId("mesh3d.register_dup");
    Dia::Mesh3D::Mesh3DAsset* first  = new Dia::Mesh3D::Mesh3DAsset(assetId);
    Dia::Mesh3D::Mesh3DAsset* second = new Dia::Mesh3D::Mesh3DAsset(assetId);

    mHandler.RegisterMesh(first);   // handler takes ownership of 'first'
    mHandler.RegisterMesh(second);  // replaces + frees 'first', now owns 'second'

    // Lookup must return the most recently registered asset, count unchanged.
    EXPECT_EQ(mHandler.LookupMesh(assetId), second);
    EXPECT_EQ(mHandler.GetLoadedCount(), 1u);
}

TEST_F(Mesh3DAssetHandlerTest, RegisterMesh_DistinctIdsCoexist)
{
    Dia::Core::StringCRC idA("mesh3d.register_a");
    Dia::Core::StringCRC idB("mesh3d.register_b");
    Dia::Mesh3D::Mesh3DAsset* a = new Dia::Mesh3D::Mesh3DAsset(idA);
    Dia::Mesh3D::Mesh3DAsset* b = new Dia::Mesh3D::Mesh3DAsset(idB);

    mHandler.RegisterMesh(a);
    mHandler.RegisterMesh(b);

    EXPECT_EQ(mHandler.LookupMesh(idA), a);
    EXPECT_EQ(mHandler.LookupMesh(idB), b);
    EXPECT_EQ(mHandler.GetLoadedCount(), 2u);
}

TEST_F(Mesh3DAssetHandlerTest, RegisterMesh_ThenUnloadRemoves)
{
    Dia::Core::StringCRC assetId("mesh3d.register_unload");
    mHandler.RegisterMesh(new Dia::Mesh3D::Mesh3DAsset(assetId));
    ASSERT_EQ(mHandler.GetLoadedCount(), 1u);

    mHandler.Unload(assetId);

    EXPECT_EQ(mHandler.LookupMesh(assetId), nullptr);
    EXPECT_EQ(mHandler.GetLoadedCount(), 0u);
}

// Regression: RegisterMesh() replacing an asset whose Load() is still in-flight
// must not leave Tick() dereferencing the freed (replaced) pointer. The pending
// load captured the original asset pointer; RegisterMesh frees it and installs a
// new one. Tick() must detect the swap (live entry != captured pointer), discard
// the stale result, and leave the registered asset intact.
TEST_F(Mesh3DAssetHandlerTest, RegisterMesh_ReplacingInFlightLoad_TickDoesNotUseFreedAsset)
{
    char path[512];
    BuildTempFilePath(path, sizeof(path), "handler_test_register_race.mesh3d");
    ASSERT_TRUE(WriteUnitCube(path));

    Dia::Core::StringCRC assetId("mesh3d.register_race");
    Dia::Core::Containers::String512 resolvedPath(path);
    TestCallback callback;

    // Kick off an async load (creates + maps the original asset, queues a job).
    mHandler.Load(assetId, resolvedPath, &callback);

    // Before Tick() drains the load, replace the same id with a pre-built asset.
    // This frees the original pointer the pending load still holds.
    Dia::Mesh3D::Mesh3DAsset* replacement = new Dia::Mesh3D::Mesh3DAsset(assetId);
    mHandler.RegisterMesh(replacement);

    // Tick() must NOT touch the freed original — it discards the stale result.
    mHandler.Tick();

    // The registered replacement survives and is what lookups return.
    EXPECT_EQ(mHandler.LookupMesh(assetId), replacement);
    EXPECT_EQ(mHandler.GetLoadedCount(), 1u);
}
