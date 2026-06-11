#include <gtest/gtest.h>

#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace
{
    Dia::Mesh3D::Vertex3D MakeVertex(float x, float y, float z)
    {
        Dia::Mesh3D::Vertex3D v{};
        v.position = Dia::Maths::Vector3D(x, y, z);
        v.colour   = 0xFFFFFFFF;
        return v;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Mesh3DAssetTest
// ---------------------------------------------------------------------------

TEST(Mesh3DAssetTest, Construction_InitialState_IsPending)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("test.mesh"));

    EXPECT_EQ(asset.GetState(), Dia::Mesh3D::Mesh3DAsset::State::Pending);
    EXPECT_FALSE(asset.IsReady());
}

TEST(Mesh3DAssetTest, Construction_AssetIdIsPreserved)
{
    Dia::Core::StringCRC id("my.mesh.asset");
    Dia::Mesh3D::Mesh3DAsset asset(id);

    EXPECT_EQ(asset.GetAssetId(), id);
}

TEST(Mesh3DAssetTest, Populate_SetsStateToReady)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("populate.ready"));

    Dia::Mesh3D::Vertex3D verts[3] = { MakeVertex(0,0,0), MakeVertex(1,0,0), MakeVertex(0,1,0) };
    uint16_t              idxs[3]  = { 0, 1, 2 };
    Dia::Mesh3D::Submesh  sub;
    sub.indexStart = 0;
    sub.indexCount = 3;
    sub.materialId = Dia::Core::StringCRC("mat.default");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-1.0f, -1.0f, -1.0f),
        Dia::Maths::Vector3D( 1.0f,  1.0f,  1.0f));

    asset.Populate(verts, 3, idxs, 3, &sub, 1, bounds);

    EXPECT_EQ(asset.GetState(), Dia::Mesh3D::Mesh3DAsset::State::Ready);
    EXPECT_TRUE(asset.IsReady());
}

TEST(Mesh3DAssetTest, Populate_VertexCountMatches)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("populate.vertcount"));

    Dia::Mesh3D::Vertex3D verts[4] = {
        MakeVertex(0,0,0), MakeVertex(1,0,0),
        MakeVertex(1,1,0), MakeVertex(0,1,0)
    };
    uint16_t             idxs[6]  = { 0,1,2, 0,2,3 };
    Dia::Mesh3D::Submesh sub;
    sub.indexStart = 0;
    sub.indexCount = 6;
    sub.materialId = Dia::Core::StringCRC("mat.default");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(0,0,0),
        Dia::Maths::Vector3D(1,1,0));

    asset.Populate(verts, 4, idxs, 6, &sub, 1, bounds);

    EXPECT_EQ(asset.GetVertices().Size(), 4u);
}

TEST(Mesh3DAssetTest, Populate_IndexCountMatches)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("populate.idxcount"));

    Dia::Mesh3D::Vertex3D verts[3] = { MakeVertex(0,0,0), MakeVertex(1,0,0), MakeVertex(0,1,0) };
    uint16_t              idxs[3]  = { 0, 1, 2 };
    Dia::Mesh3D::Submesh  sub;
    sub.indexStart = 0;
    sub.indexCount = 3;
    sub.materialId = Dia::Core::StringCRC("mat.default");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(0,0,0),
        Dia::Maths::Vector3D(1,1,0));

    asset.Populate(verts, 3, idxs, 3, &sub, 1, bounds);

    EXPECT_EQ(asset.GetIndices().Size(), 3u);
}

TEST(Mesh3DAssetTest, Populate_SubmeshCountMatches)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("populate.subcount"));

    Dia::Mesh3D::Vertex3D verts[3] = { MakeVertex(0,0,0), MakeVertex(1,0,0), MakeVertex(0,1,0) };
    uint16_t              idxs[3]  = { 0, 1, 2 };

    Dia::Mesh3D::Submesh subs[2];
    subs[0].indexStart = 0; subs[0].indexCount = 3; subs[0].materialId = Dia::Core::StringCRC("mat.a");
    subs[1].indexStart = 0; subs[1].indexCount = 0; subs[1].materialId = Dia::Core::StringCRC("mat.b");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(0,0,0),
        Dia::Maths::Vector3D(1,1,0));

    asset.Populate(verts, 3, idxs, 3, subs, 2, bounds);

    EXPECT_EQ(asset.GetSubmeshes().Size(), 2u);
}

TEST(Mesh3DAssetTest, Populate_BoundsAreSet)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("populate.bounds"));

    Dia::Mesh3D::Vertex3D verts[3] = { MakeVertex(0,0,0), MakeVertex(1,0,0), MakeVertex(0,1,0) };
    uint16_t              idxs[3]  = { 0, 1, 2 };
    Dia::Mesh3D::Submesh  sub;
    sub.indexStart = 0;
    sub.indexCount = 3;
    sub.materialId = Dia::Core::StringCRC("mat.default");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-2.0f, -3.0f, -4.0f),
        Dia::Maths::Vector3D( 2.0f,  3.0f,  4.0f));

    asset.Populate(verts, 3, idxs, 3, &sub, 1, bounds);

    EXPECT_FLOAT_EQ(asset.GetBounds().GetMin().x, -2.0f);
    EXPECT_FLOAT_EQ(asset.GetBounds().GetMin().y, -3.0f);
    EXPECT_FLOAT_EQ(asset.GetBounds().GetMin().z, -4.0f);
    EXPECT_FLOAT_EQ(asset.GetBounds().GetMax().x,  2.0f);
    EXPECT_FLOAT_EQ(asset.GetBounds().GetMax().y,  3.0f);
    EXPECT_FLOAT_EQ(asset.GetBounds().GetMax().z,  4.0f);
}

TEST(Mesh3DAssetTest, MarkFailed_SetsStateToFailed)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("markfailed.state"));

    asset.MarkFailed("test failure");

    EXPECT_EQ(asset.GetState(), Dia::Mesh3D::Mesh3DAsset::State::Failed);
    EXPECT_FALSE(asset.IsReady());
}

TEST(Mesh3DAssetTest, MarkFailed_AfterPopulate_StillFailed)
{
    Dia::Mesh3D::Mesh3DAsset asset(Dia::Core::StringCRC("markfailed.afterpopulate"));

    Dia::Mesh3D::Vertex3D verts[3] = { MakeVertex(0,0,0), MakeVertex(1,0,0), MakeVertex(0,1,0) };
    uint16_t              idxs[3]  = { 0, 1, 2 };
    Dia::Mesh3D::Submesh  sub;
    sub.indexStart = 0;
    sub.indexCount = 3;
    sub.materialId = Dia::Core::StringCRC("mat.default");

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(0,0,0),
        Dia::Maths::Vector3D(1,1,0));

    asset.Populate(verts, 3, idxs, 3, &sub, 1, bounds);
    ASSERT_EQ(asset.GetState(), Dia::Mesh3D::Mesh3DAsset::State::Ready);

    asset.MarkFailed("post-populate failure");

    EXPECT_EQ(asset.GetState(), Dia::Mesh3D::Mesh3DAsset::State::Failed);
    EXPECT_FALSE(asset.IsReady());
}
