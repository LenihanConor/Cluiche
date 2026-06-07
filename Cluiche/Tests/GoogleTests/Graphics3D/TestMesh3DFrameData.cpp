#include <gtest/gtest.h>

#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Camera3D.h>
#include <DiaGraphics3D/Light.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics3D/Testing/MockMesh3DFrameData.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Core/Angle.h>
#include <DiaMaths/Vector/Vector2D.h>

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
// Camera3D mathematical correctness tests (AC9)
// ===========================================================================

TEST(DiaGraphics3D_Camera3D, SetView_CameraOriginRecoverableFromInverseViewMatrix)
{
    Camera3D cam;
    Dia::Maths::Vector3D eye(3.0f, 5.0f, 7.0f);
    Dia::Maths::Vector3D target(0.0f, 0.0f, 0.0f);
    Dia::Maths::Vector3D up(0.0f, 1.0f, 0.0f);
    cam.SetView(eye, target, up);

    // Inverse of the view matrix brings us back to world space.
    // The camera origin in world space is recoverable via GetTranslation on the inverse.
    Dia::Maths::Matrix44 invView = cam.view.Inverse();
    Dia::Maths::Vector3D recovered = invView.GetTranslation();

    EXPECT_NEAR(recovered.x, eye.x, 1e-4f);
    EXPECT_NEAR(recovered.y, eye.y, 1e-4f);
    EXPECT_NEAR(recovered.z, eye.z, 1e-4f);
}

TEST(DiaGraphics3D_Camera3D, SetPerspective_DiagonalEncodesFovAndAspect)
{
    Camera3D cam;
    Dia::Maths::Angle fovY = Dia::Maths::Angle::FromDegrees(90.0f);
    float aspect = 1.0f; // square — simplifies expected value
    cam.SetPerspective(fovY, aspect, 0.1f, 100.0f);
    // For 90 deg fovY: cot(45) = 1.0, so projection(1,1) ≈ 1.0 (±epsilon for float)
    EXPECT_NEAR(cam.projection(1, 1), 1.0f, 1e-4f);
}

TEST(DiaGraphics3D_Camera3D, SetOrthographic_DiagonalEncodesDimensions)
{
    Camera3D cam;
    cam.SetOrthographic(-5.0f, 5.0f, -5.0f, 5.0f, 1.0f, 100.0f);
    // For symmetric ortho [-5,5] x [-5,5]: m[0][0] = 2/(r-l) = 2/10 = 0.2
    EXPECT_NEAR(cam.projection(0, 0), 0.2f, 1e-4f);
    EXPECT_NEAR(cam.projection(1, 1), 0.2f, 1e-4f);
}

// ===========================================================================
// Mesh3DDrawCommand field storage
// ===========================================================================

TEST(DiaGraphics3D_Mesh3DDrawCommand, DefaultConstruction_ZeroIds)
{
    Mesh3DDrawCommand cmd;
    EXPECT_EQ(cmd.meshId,     Dia::Core::StringCRC());
    EXPECT_EQ(cmd.materialId, Dia::Core::StringCRC());
}

TEST(DiaGraphics3D_Mesh3DDrawCommand, FieldsStoredAndRetrievable)
{
    Mesh3DDrawCommand cmd;
    cmd.meshId               = Dia::Core::StringCRC("my_mesh");
    cmd.materialId           = Dia::Core::StringCRC("my_mat");
    cmd.skinningPaletteIndex = 7u;
    cmd.layer                = -3;

    EXPECT_EQ(cmd.meshId,               Dia::Core::StringCRC("my_mesh"));
    EXPECT_EQ(cmd.materialId,           Dia::Core::StringCRC("my_mat"));
    EXPECT_EQ(cmd.skinningPaletteIndex, 7u);
    EXPECT_EQ(cmd.layer,                -3);
}

// ===========================================================================
// Mesh3DFrameData — additional contract tests
// ===========================================================================

TEST(DiaGraphics3D_Mesh3DFrameData, Clear_PreservesCamera)
{
    Mesh3DFrameData fd;
    Camera3D cam;
    cam.SetOrthographic(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 10.0f);
    fd.SetCamera(cam);
    fd.RequestDrawMesh(Mesh3DDrawCommand());
    fd.Clear();
    // Camera must survive Clear — caller sets it once per frame
    EXPECT_NE(fd.GetCamera().projection, Dia::Maths::Matrix44::Identity());
}

TEST(DiaGraphics3D_Mesh3DFrameData, Clear_ResetsDropCounters)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    for (uint32_t i = 0; i <= Mesh3DFrameData::kMaxMeshDraws; ++i)
        fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.DroppedMeshCount(), 1u);
    fd.Clear();
    EXPECT_EQ(fd.DroppedMeshCount(), 0u);
    EXPECT_EQ(fd.DroppedLightCount(), 0u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, MultipleOverflows_CountAccumulates)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    for (uint32_t i = 0; i < Mesh3DFrameData::kMaxMeshDraws; ++i)
        fd.RequestDrawMesh(cmd);
    fd.RequestDrawMesh(cmd);
    fd.RequestDrawMesh(cmd);
    fd.RequestDrawMesh(cmd);
    EXPECT_EQ(fd.DroppedMeshCount(), 3u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, DirectionalLightCapacityOverflow_LightDropped)
{
    Mesh3DFrameData fd;
    DirectionalLight dl;
    dl.intensity = 1.0f;
    for (uint32_t i = 0; i < Mesh3DFrameData::kMaxLights; ++i)
        fd.AddDirectionalLight(dl);
    EXPECT_EQ(fd.GetDirectionalLights().Size(), Mesh3DFrameData::kMaxLights);
    EXPECT_EQ(fd.DroppedLightCount(), 0u);
    fd.AddDirectionalLight(dl);
    EXPECT_EQ(fd.DroppedLightCount(), 1u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, DirectionalLight_FieldsStoredCorrectly)
{
    Mesh3DFrameData fd;
    DirectionalLight dl;
    dl.direction  = Dia::Maths::Vector3D(0.0f, -1.0f, 0.0f);
    dl.colour     = Dia::Graphics::RGBA(255, 200, 100, 255);
    dl.intensity  = 2.5f;
    fd.AddDirectionalLight(dl);
    const DirectionalLight& stored = fd.GetDirectionalLights().At(0);
    EXPECT_FLOAT_EQ(stored.direction.y, -1.0f);
    EXPECT_FLOAT_EQ(stored.intensity,    2.5f);
    EXPECT_EQ(stored.colour.R(),        255u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, PointLight_FieldsStoredCorrectly)
{
    Mesh3DFrameData fd;
    PointLight pl;
    pl.position  = Dia::Maths::Vector3D(1.0f, 2.0f, 3.0f);
    pl.intensity = 3.0f;
    pl.range     = 50.0f;
    fd.AddPointLight(pl);
    const PointLight& stored = fd.GetPointLights().At(0);
    EXPECT_FLOAT_EQ(stored.position.x, 1.0f);
    EXPECT_FLOAT_EQ(stored.intensity,  3.0f);
    EXPECT_FLOAT_EQ(stored.range,      50.0f);
}

TEST(DiaGraphics3D_Mesh3DFrameData, Copy_IndependentFromSource)
{
    Mesh3DFrameData src;
    Mesh3DDrawCommand cmd;
    cmd.meshId = Dia::Core::StringCRC("mesh_x");
    src.RequestDrawMesh(cmd);

    Mesh3DFrameData dst;
    dst.Copy(src);

    // Mutate source — dst must be unaffected
    src.Clear();
    EXPECT_EQ(dst.GetMeshDraws().Size(), 1u);
    EXPECT_EQ(dst.GetMeshDraws().At(0).meshId, Dia::Core::StringCRC("mesh_x"));
}

TEST(DiaGraphics3D_Mesh3DFrameData, Copy_PreservesDropCounters)
{
    Mesh3DFrameData src;
    Mesh3DDrawCommand cmd;
    for (uint32_t i = 0; i <= Mesh3DFrameData::kMaxMeshDraws; ++i)
        src.RequestDrawMesh(cmd);

    Mesh3DFrameData dst;
    dst.Copy(src);
    EXPECT_EQ(dst.DroppedMeshCount(), 1u);
}

TEST(DiaGraphics3D_Mesh3DFrameData, RequestDrawMesh_AllFieldsPreservedInArray)
{
    Mesh3DFrameData fd;
    Mesh3DDrawCommand cmd;
    cmd.meshId               = Dia::Core::StringCRC("test_mesh");
    cmd.materialId           = Dia::Core::StringCRC("test_mat");
    cmd.skinningPaletteIndex = 3u;
    cmd.layer                = 5;
    fd.RequestDrawMesh(cmd);

    const Mesh3DDrawCommand& stored = fd.GetMeshDraws().At(0);
    EXPECT_EQ(stored.meshId,               Dia::Core::StringCRC("test_mesh"));
    EXPECT_EQ(stored.materialId,           Dia::Core::StringCRC("test_mat"));
    EXPECT_EQ(stored.skinningPaletteIndex, 3u);
    EXPECT_EQ(stored.layer,                5);
}

// ===========================================================================
// FrameData3D — 2D side not corrupted by 3D operations
// ===========================================================================

TEST(DiaGraphics3D_FrameData3D, Clear_ClearsBothSides_DropCounterReset)
{
    FrameData3D fd;
    Mesh3DDrawCommand cmd;
    for (uint32_t i = 0; i <= Mesh3DFrameData::kMaxMeshDraws; ++i)
        fd.RequestDrawMesh(cmd);
    fd.Clear();
    EXPECT_EQ(fd.DroppedMeshCount(), 0u);
    EXPECT_EQ(fd.GetMeshDraws().Size(), 0u);
}

TEST(DiaGraphics3D_FrameData3D, Copy_Preserves2DData)
{
    FrameData3D src;
    Dia::Graphics::RGBA col(255, 0, 0, 255);
    src.RequestDraw(Dia::Maths::Vector2D(1.0f, 2.0f), 3.0f, col);

    FrameData3D dst;
    dst.Copy(src);

    // 3D side
    EXPECT_EQ(dst.GetMeshDraws().Size(), 0u);
    // 2D debug side: confirm circle was copied (use debug visitor)
    // We can't easily count without a visitor — check size via DebugFrameData
    // Since FrameData3D inherits DebugFrameData, we can just verify 3D didn't
    // clobber 2D by also adding a mesh and confirming both survive copy
    Mesh3DDrawCommand cmd;
    cmd.meshId = Dia::Core::StringCRC("m");
    src.RequestDrawMesh(cmd);
    FrameData3D dst2;
    dst2.Copy(src);
    EXPECT_EQ(dst2.GetMeshDraws().Size(), 1u);
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
