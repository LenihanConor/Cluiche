#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <gtest/gtest.h>
#include <DiaMesh3DVisualDebugger/MeshStatsDrawer.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaMaths/Matrix/Matrix44.h>
#include <DiaCore/CRC/StringCRC.h>

TEST(MeshStatsDrawerTest, LayerName_IsMesh3DStats)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler assetHandler;
    Dia::Debug::DebugLayerManager layerManager;

    Dia::Mesh3D::MeshStatsDrawer drawer(frameData, assetHandler, layerManager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kMesh3DStats);
}

TEST(MeshStatsDrawerTest, Draw_EmitsNoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler assetHandler;
    Dia::Debug::DebugLayerManager layerManager;

    Dia::Mesh3D::MeshStatsDrawer drawer(frameData, assetHandler, layerManager);

    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 0);
    EXPECT_EQ(dbg.GetDebugPrimitiveCount(), 0);
}

TEST(MeshStatsDrawerTest, Draw_WithCommands_EmitsNoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler assetHandler;
    Dia::Debug::DebugLayerManager layerManager;

    Dia::Graphics3D::Mesh3DDrawCommand cmd;
    cmd.transform = Dia::Maths::Matrix44::Identity();
    frameData.RequestDrawMesh(cmd);

    Dia::Mesh3D::MeshStatsDrawer drawer(frameData, assetHandler, layerManager);

    Dia::Graphics::FrameData dbg;
    drawer.Draw(dbg);

    EXPECT_EQ(dbg.GetDebug3DPrimitiveCount(), 0);
    EXPECT_EQ(dbg.GetDebugPrimitiveCount(), 0);
}
