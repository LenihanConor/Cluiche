#include <gtest/gtest.h>

#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics3D/Testing/MockMesh3DFrameData.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::Graphics3D;

// ===========================================================================
// Camera3D tests
// ===========================================================================

TEST(DiaGraphics3D_Camera3D, DefaultConstruction_IdentityMatrices)
{
    Camera3D cam;
    // Both view and projection default to Identity — first element should be 1.0f
    EXPECT_FLOAT_EQ(cam.view(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(cam.projection(0, 0), 1.0f);
}

TEST(DiaGraphics3D_Camera3D, SetView_ChangesViewMatrix)
{
    Camera3D cam;
    Dia::Maths::Vector3D eye(0.0f, 0.0f, 5.0f);
    Dia::Maths::Vector3D target(0.0f, 0.0f, 0.0f);
    Dia::Maths::Vector3D up(0.0f, 1.0f, 0.0f);
    cam.SetView(eye, target, up);
    // View matrix should no longer be identity after SetView
    // The translation row/column encodes -eye in view space
    // Check that it differs from identity
    EXPECT_NE(cam.view, Dia::Maths::Matrix44::Identity());
}

TEST(DiaGraphics3D_Camera3D, SetPerspective_ChangesProjectionMatrix)
{
    Camera3D cam;
    Dia::Maths::Angle fovY = Dia::Maths::Angle::FromDegrees(45.0f);
    cam.SetPerspective(fovY, 16.0f / 9.0f, 0.1f, 1000.0f);
    EXPECT_NE(cam.projection, Dia::Maths::Matrix44::Identity());
}

TEST(DiaGraphics3D_Camera3D, SetOrthographic_ChangesProjectionMatrix)
{
    Camera3D cam;
    cam.SetOrthographic(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    EXPECT_NE(cam.projection, Dia::Maths::Matrix44::Identity());
}

// ===========================================================================
// Mesh3DDrawCommand tests
// ===========================================================================

TEST(DiaGraphics3D_Mesh3DDrawCommand, DefaultConstruction_Sane)
{
    Mesh3DDrawCommand cmd;
    EXPECT_EQ(cmd.skinningPaletteIndex, 0u);
    EXPECT_EQ(cmd.layer, 0);
    // transform should be identity
    EXPECT_FLOAT_EQ(cmd.transform(0, 0), 1.0f);
    EXPECT_FLOAT_EQ(cmd.transform(1, 1), 1.0f);
    EXPECT_FLOAT_EQ(cmd.transform(2, 2), 1.0f);
    EXPECT_FLOAT_EQ(cmd.transform(3, 3), 1.0f);
}

// ===========================================================================
// Mesh3DFrameData tests
// ===========================================================================

TEST(DiaGraphics3D_Mesh3DFrameData, DefaultConstruction_Empty)
{
    Mesh3DFrameData fd;
    EXPECT_EQ(fd.GetMeshDraws().Size(), 0u);
    EXPECT_EQ(fd.GetDirectionalLights().Size(), 0u);
    EXPECT_EQ(fd.GetPointLights().Size(), 0u);
    EXPECT_EQ(fd.DroppedMeshCount(), 0u);
    EXPECT_EQ(fd.DroppedLightCount(), 0u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, RequestDrawMesh_IncrementsCount)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.GetMeshDraws().Size(), 1u);
    EXPECT_EQ(fd.DroppedMeshCount(), 0u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, AddDirectionalLight_IncrementsCount)
{
    Mesh3DFrameData fd;
    DirectionalLight light;
    light.intensity = 1.0f;
    fd.AddDirectionalLight(light);
    EXPECT_EQ(fd.GetDirectionalLights().Size(), 1u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, AddPointLight_IncrementsCount)
{
    Mesh3DFrameData fd;
    PointLight light;
    light.intensity = 1.0f;
    light.range = 10.0f;
    fd.AddPointLight(light);
    EXPECT_EQ(fd.GetPointLights().Size(), 1u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, SetCamera_StoresCamera)
{
    Mesh3DFrameData fd;
    Camera3D cam;
    Dia::Maths::Angle fovY = Dia::Maths::Angle::FromDegrees(60.0f);
    cam.SetPerspective(fovY, 1.333f, 0.1f, 500.0f);
    fd.SetCamera(cam);
    EXPECT_NE(fd.GetCamera().projection, Dia::Maths::Matrix44::Identity());
}

TEST(DiaGraphics3D_Mesh3DFrameData, Clear_ResetsAll)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    DirectionalLight dl;
    PointLight pl;
    pl.range = 5.0f;
    fd.RequestDrawMesh(cmd);
    fd.AddDirectionalLight(dl);
    fd.AddPointLight(pl);
    fd.Clear();
    EXPECT_EQ(fd.GetMeshDraws().Size(), 0u);
    EXPECT_EQ(fd.GetDirectionalLights().Size(), 0u);
    EXPECT_EQ(fd.GetPointLights().Size(), 0u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, Copy_PreservesData)
{
    Mesh3DFrameData src;
    Mesh3DDrawCommand cmd;
    Dia::Core::StringCRC id("mesh_a");
    cmd.meshId = id;
    src.RequestDrawMesh(cmd);

    Mesh3DFrameData dst;
    dst.Copy(src);

    EXPECT_EQ(dst.GetMeshDraws().Size(), 1u);
    EXPECT_EQ(dst.GetMeshDraws().At(0).meshId, id);
}

TEST(DiaGraphics3D_Mesh3DFrameData, CapacityOverflow_MeshDropped)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    for (uint32_t i = 0; i < Mesh3DFrameData::kMaxMeshDraws; ++i)
        fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.GetMeshDraws().Size(), Mesh3DFrameData::kMaxMeshDraws);
    EXPECT_EQ(fd.DroppedMeshCount(), 0u);

    // One more should drop
    fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.DroppedMeshCount(), 1u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, LightCapacityOverflow_LightDropped)
{
    Mesh3DFrameData fd;
    PointLight pl;
    pl.range = 1.0f;
    for (uint32_t i = 0; i < Mesh3DFrameData::kMaxLights; ++i)
        fd.AddPointLight(pl);
    EXPECT_EQ(fd.GetPointLights().Size(), Mesh3DFrameData::kMaxLights);
    EXPECT_EQ(fd.DroppedLightCount(), 0u);

    fd.AddPointLight(pl);
    EXPECT_EQ(fd.DroppedLightCount(), 1u);
}

// ===========================================================================
// FrameData3D tests
// ===========================================================================

TEST(DiaGraphics3D_FrameData3D, DefaultConstruction_Empty)
{
    FrameData3D fd;
    EXPECT_EQ(fd.GetMeshDraws().Size(), 0u);
    // 2D side should also be empty
    EXPECT_EQ(fd.GetSprites().Size(), 0u);
}

TEST(DiaGraphics3D_FrameData3D, Clear_ClearsBoth2DAnd3D)
{
    FrameData3D fd;
    Mesh3DDrawCommand cmd;
    fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.GetMeshDraws().Size(), 1u);
    fd.Clear();
    EXPECT_EQ(fd.GetMeshDraws().Size(), 0u);
    EXPECT_EQ(fd.GetSprites().Size(), 0u);
}

TEST(DiaGraphics3D_FrameData3D, Copy_Preserves3DData)
{
    FrameData3D src;
    Mesh3DDrawCommand cmd;
    Dia::Core::StringCRC id("mesh_b");
    cmd.meshId = id;
    src.RequestDrawMesh(cmd);

    FrameData3D dst;
    dst.Copy(src);

    EXPECT_EQ(dst.GetMeshDraws().Size(), 1u);
    EXPECT_EQ(dst.GetMeshDraws().At(0).meshId, id);
}

TEST(DiaGraphics3D_FrameData3D, Assignment_Preserves3DData)
{
    FrameData3D src;
    Mesh3DDrawCommand cmd;
    Dia::Core::StringCRC id("mesh_c");
    cmd.meshId = id;
    src.RequestDrawMesh(cmd);

    FrameData3D dst;
    dst = src;

    EXPECT_EQ(dst.GetMeshDraws().Size(), 1u);
    EXPECT_EQ(dst.GetMeshDraws().At(0).meshId, id);
}

// ===========================================================================
// MockMesh3DFrameData test
// ===========================================================================

TEST(DiaGraphics3D_Mock, MockMesh3DFrameData_ConstructsAndWorks)
{
    Dia::Graphics3D::Testing::MockMesh3DFrameData mock;
    Mesh3DDrawCommand cmd;
    mock.RequestDrawMesh(cmd);
    EXPECT_EQ(mock.GetMeshDraws().Size(), 1u);
}
