#include <gtest/gtest.h>

#include <DiaMesh3D/Primitives/UnitCube.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// UnitCubeTest
// ---------------------------------------------------------------------------

TEST(UnitCubeTest, CreateUnitCube_ReturnsNonNull)
{
    Dia::Mesh3D::Mesh3DAsset* asset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("test.unit_cube"));
    EXPECT_NE(asset, nullptr);
    delete asset;
}

TEST(UnitCubeTest, CreateUnitCube_VertexCountIs24)
{
    Dia::Mesh3D::Mesh3DAsset* asset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("test.unit_cube.verts"));
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetVertices().Size(), 24u);
    delete asset;
}

TEST(UnitCubeTest, CreateUnitCube_IndexCountIs36)
{
    Dia::Mesh3D::Mesh3DAsset* asset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("test.unit_cube.indices"));
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetIndices().Size(), 36u);
    delete asset;
}

TEST(UnitCubeTest, CreateUnitCube_StateIsReady)
{
    Dia::Mesh3D::Mesh3DAsset* asset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("test.unit_cube.state"));
    ASSERT_NE(asset, nullptr);
    EXPECT_EQ(asset->GetState(), Dia::Mesh3D::Mesh3DAsset::State::Ready);
    EXPECT_TRUE(asset->IsReady());
    delete asset;
}

TEST(UnitCubeTest, CreateUnitCube_FirstFaceNormalsAreAxisAligned)
{
    Dia::Mesh3D::Mesh3DAsset* asset = Dia::Mesh3D::Primitives::CreateUnitCube(Dia::Core::StringCRC("test.unit_cube.normals"));
    ASSERT_NE(asset, nullptr);
    ASSERT_GE(asset->GetVertices().Size(), 4u);

    // First 4 vertices belong to the +X face — normals should be (1, 0, 0)
    for (uint32_t i = 0; i < 4; ++i)
    {
        const Dia::Mesh3D::Vertex3D& v = asset->GetVertices()[i];
        EXPECT_FLOAT_EQ(v.normal.x,  1.0f) << "vertex " << i << " normal.x";
        EXPECT_FLOAT_EQ(v.normal.y,  0.0f) << "vertex " << i << " normal.y";
        EXPECT_FLOAT_EQ(v.normal.z,  0.0f) << "vertex " << i << " normal.z";
    }

    delete asset;
}
