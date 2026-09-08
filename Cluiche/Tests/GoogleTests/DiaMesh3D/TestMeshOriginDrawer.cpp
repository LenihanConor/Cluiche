////////////////////////////////////////////////////////////////////////////////
// Filename: TestMeshOriginDrawer.cpp
// Tests for Dia::Mesh3D::MeshOriginDrawer
////////////////////////////////////////////////////////////////////////////////

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <gtest/gtest.h>

#include <DiaMesh3DVisualDebugger/MeshOriginDrawer.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Dia::Graphics3D::Mesh3DDrawCommand MakeCommand(int16_t layer = 0,
                                                   uint32_t skinningPaletteIndex = 0)
    {
        Dia::Graphics3D::Mesh3DDrawCommand cmd;
        cmd.meshId               = Dia::Core::StringCRC("mesh.origin");
        cmd.materialId           = Dia::Core::StringCRC("mat.default");
        cmd.transform            = Dia::Maths::Matrix44::Identity();
        cmd.skinningPaletteIndex = skinningPaletteIndex;
        cmd.layer                = layer;
        return cmd;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// MeshOriginDrawerTest
// ---------------------------------------------------------------------------

TEST(MeshOriginDrawerTest, LayerName_IsMesh3DOrigins)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::MeshOriginDrawer    drawer(frameData);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kMesh3DOrigins);
}

TEST(MeshOriginDrawerTest, Draw_OneCommand_Emits3Rays)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand());

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 3u);
}

TEST(MeshOriginDrawerTest, Draw_Layer0_IsKActive)
{
    // Palette[0] = kActive (layer 0 -> idx = (0 % 5 + 5) % 5 = 0)
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/0));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kActive);
}

TEST(MeshOriginDrawerTest, Draw_Layer1_IsKGoal)
{
    // Palette[1] = kGoal (layer 1 -> idx = 1)
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/1));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kGoal);
}

TEST(MeshOriginDrawerTest, Draw_NegativeLayer_CyclesPalette)
{
    // layer = -1: idx = ((-1 % 5) + 5) % 5 = (-1 + 5) % 5 = 4
    // Palette[4] = kHealthy
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/-1));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(MeshOriginDrawerTest, Draw_ArmLength_IsConstant)
{
    // Arm length is a fixed world-space constant (0.2m) regardless of any external scale.
    Dia::Graphics3D::Mesh3DFrameData frameData;
    frameData.RequestDrawMesh(MakeCommand());

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_FLOAT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.length, 0.2f);
}

TEST(MeshOriginDrawerTest, Draw_EmptyFrameData_NoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 0u);
}

TEST(MeshOriginDrawerTest, Draw_Layer2_IsKWarning)
{
    // Palette[2] = kWarning (layer 2 -> idx = 2)
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/2));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kWarning);
}

TEST(MeshOriginDrawerTest, Draw_Layer3_IsKCapped)
{
    // Palette[3] = kCapped (layer 3 -> idx = 3)
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/3));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kCapped);
}

TEST(MeshOriginDrawerTest, Draw_Layer4_IsKHealthy)
{
    // Palette[4] = kHealthy (layer 4 -> idx = 4)
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand(/*layer=*/4));

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.colour,
              Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(MeshOriginDrawerTest, Draw_TwoCommands_Emits6Rays)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;

    frameData.RequestDrawMesh(MakeCommand());
    frameData.RequestDrawMesh(MakeCommand());

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 6u);
}

TEST(MeshOriginDrawerTest, Draw_WorldPosition_IsAtTranslation)
{
    // Ray origins must match the command's world translation (5, 0, 0).
    Dia::Graphics3D::Mesh3DFrameData frameData;

    Dia::Graphics3D::Mesh3DDrawCommand cmd = MakeCommand();
    cmd.transform = Dia::Maths::Matrix44::FromTranslation(
        Dia::Maths::Vector3D(5.0f, 0.0f, 0.0f));
    frameData.RequestDrawMesh(cmd);

    Dia::Mesh3D::MeshOriginDrawer drawer(frameData);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_FLOAT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.origin.X(), 5.0f);
    EXPECT_FLOAT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.origin.Y(), 0.0f);
    EXPECT_FLOAT_EQ(dbg.GetDebug3DPrimitive(0).ray3D.origin.Z(), 0.0f);
}
