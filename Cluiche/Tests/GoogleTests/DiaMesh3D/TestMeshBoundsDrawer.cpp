////////////////////////////////////////////////////////////////////////////////
// Filename: TestMeshBoundsDrawer.cpp
// Tests for Dia::Mesh3D::MeshBoundsDrawer
////////////////////////////////////////////////////////////////////////////////

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <gtest/gtest.h>

#include <DiaMesh3DVisualDebugger/MeshBoundsDrawer.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMesh3D/Vertex3D.h>
#include <DiaMesh3D/Submesh.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/CRC/CRC.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Dia::Mesh3D::Vertex3D MakeVertex(float x, float y, float z)
    {
        Dia::Mesh3D::Vertex3D v{};
        v.position = Dia::Maths::Vector3D(x, y, z);
        v.colour   = 0xFFFFFFFF;
        return v;
    }

    // Build a ready asset with the given bounds and register it.
    // The raw pointer is owned by handler after RegisterMesh().
    void RegisterReadyAsset(Dia::Mesh3D::Mesh3DAssetHandler& handler,
                            const char*                      id,
                            const Dia::Geometry3D::AABB&     bounds)
    {
        auto* asset = new Dia::Mesh3D::Mesh3DAsset(Dia::Core::StringCRC(id));

        Dia::Mesh3D::Vertex3D verts[3] = {
            MakeVertex(0, 0, 0), MakeVertex(1, 0, 0), MakeVertex(0, 1, 0)
        };
        uint16_t idxs[3] = { 0, 1, 2 };
        Dia::Mesh3D::Submesh sub;
        sub.indexStart = 0;
        sub.indexCount = 3;
        sub.materialId = Dia::Core::CRC("mat.default");

        asset->Populate(verts, 3, idxs, 3, &sub, 1, bounds);
        handler.RegisterMesh(asset);
    }

    Dia::Graphics3D::Mesh3DDrawCommand MakeCommand(const char* meshId,
                                                   const Dia::Maths::Matrix44& transform = Dia::Maths::Matrix44::Identity())
    {
        Dia::Graphics3D::Mesh3DDrawCommand cmd;
        cmd.meshId               = Dia::Core::StringCRC(meshId);
        cmd.materialId           = Dia::Core::StringCRC("mat.default");
        cmd.transform            = transform;
        cmd.skinningPaletteIndex = 0;
        cmd.layer                = 0;
        return cmd;
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// MeshBoundsDrawerTest
// ---------------------------------------------------------------------------

TEST(MeshBoundsDrawerTest, LayerName_IsMesh3DBounds)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Mesh3D::MeshBoundsDrawer    drawer(frameData, handler);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kMesh3DBounds);
}

TEST(MeshBoundsDrawerTest, Draw_ReadyAsset_Emits12Lines)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-1.0f, -1.0f, -1.0f),
        Dia::Maths::Vector3D( 1.0f,  1.0f,  1.0f));
    RegisterReadyAsset(handler, "cube", bounds);

    frameData.RequestDrawMesh(MakeCommand("cube"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 12u);
}

TEST(MeshBoundsDrawerTest, Draw_ReadyAsset_Colour_IsHealthy)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-1.0f, -1.0f, -1.0f),
        Dia::Maths::Vector3D( 1.0f,  1.0f,  1.0f));
    RegisterReadyAsset(handler, "cube.healthy", bounds);

    frameData.RequestDrawMesh(MakeCommand("cube.healthy"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).line3D.colour,
              Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(MeshBoundsDrawerTest, Draw_PendingAsset_Colour_IsWarning)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    // Register a pending (not populated) asset
    auto* asset = new Dia::Mesh3D::Mesh3DAsset(Dia::Core::StringCRC("pending.mesh"));
    handler.RegisterMesh(asset);

    frameData.RequestDrawMesh(MakeCommand("pending.mesh"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).line3D.colour,
              Dia::Debug::DebugColourPalette::kWarning);
}

TEST(MeshBoundsDrawerTest, Draw_FailedAsset_Colour_IsError)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    auto* asset = new Dia::Mesh3D::Mesh3DAsset(Dia::Core::StringCRC("failed.mesh"));
    asset->MarkFailed("test failure");
    handler.RegisterMesh(asset);

    frameData.RequestDrawMesh(MakeCommand("failed.mesh"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).line3D.colour,
              Dia::Debug::DebugColourPalette::kError);
}

TEST(MeshBoundsDrawerTest, Draw_UnknownMeshId_Colour_IsInactive)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    // No asset registered — lookup returns nullptr

    frameData.RequestDrawMesh(MakeCommand("unknown.mesh"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    // Still emits 12 lines for the unit-cube placeholder
    ASSERT_EQ(dbg.GetDebug3DPrimitiveCount(), 12u);
    EXPECT_EQ(dbg.GetDebug3DPrimitive(0).line3D.colour,
              Dia::Debug::DebugColourPalette::kInactive);
}

TEST(MeshBoundsDrawerTest, Draw_TranslationOnly_BoxMatchesWorldPosition)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-1.0f, -1.0f, -1.0f),
        Dia::Maths::Vector3D( 1.0f,  1.0f,  1.0f));
    RegisterReadyAsset(handler, "cube.offset", bounds);

    // Translate to (5, 0, 0): all box X coords must lie in [4, 6]
    Dia::Maths::Matrix44 transform = Dia::Maths::Matrix44::FromTranslation(
        Dia::Maths::Vector3D(5.0f, 0.0f, 0.0f));
    frameData.RequestDrawMesh(MakeCommand("cube.offset", transform));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_GE(dbg.GetDebug3DPrimitiveCount(), 1u);
    const float fromX = dbg.GetDebug3DPrimitive(0).line3D.from.X();
    EXPECT_GE(fromX, 4.0f);
    EXPECT_LE(fromX, 6.0f);
}

TEST(MeshBoundsDrawerTest, Draw_UniformScale_BoxScalesWithTransform)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    // Unit [-0.5, 0.5] bounds; scale 10 -> world box spans [-5, 5] on all axes
    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f),
        Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f));
    RegisterReadyAsset(handler, "cube.scaled", bounds);

    Dia::Maths::Matrix44 transform = Dia::Maths::Matrix44::FromScale(10.0f);
    frameData.RequestDrawMesh(MakeCommand("cube.scaled", transform));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    ASSERT_EQ(dbg.GetDebug3DPrimitiveCount(), 12u);

    // Every corner X must lie in [-5, 5]
    for (uint32_t i = 0; i < dbg.GetDebug3DPrimitiveCount(); ++i)
    {
        EXPECT_GE(dbg.GetDebug3DPrimitive(i).line3D.from.X(), -5.0f);
        EXPECT_LE(dbg.GetDebug3DPrimitive(i).line3D.from.X(),  5.0f);
        EXPECT_GE(dbg.GetDebug3DPrimitive(i).line3D.to.X(),   -5.0f);
        EXPECT_LE(dbg.GetDebug3DPrimitive(i).line3D.to.X(),    5.0f);
    }
}

TEST(MeshBoundsDrawerTest, Draw_TwoCommands_Emits24Lines)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f),
        Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f));
    RegisterReadyAsset(handler, "cube.a", bounds);
    RegisterReadyAsset(handler, "cube.b", bounds);

    frameData.RequestDrawMesh(MakeCommand("cube.a"));
    frameData.RequestDrawMesh(MakeCommand("cube.b"));

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 24u);
}

TEST(MeshBoundsDrawerTest, Draw_EmptyFrameData_NoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    // No draw commands submitted

    Dia::Mesh3D::MeshBoundsDrawer drawer(frameData, handler);
    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 0u);
}
