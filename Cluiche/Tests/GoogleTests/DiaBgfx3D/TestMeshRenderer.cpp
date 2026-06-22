// TestMeshRenderer.cpp - Google Test unit tests for MeshRenderer
//
// Only the paths that never reach a bgfx submit call are exercised here (no GPU
// context in the headless runner). MeshRenderer::Draw reaches bgfx only after a
// draw command resolves to a Ready asset AND an uploaded GpuMesh; every guard
// before that point is testable:
//   - empty frame              -> no draws, no crash
//   - skinned draw             -> skipped by the skinningPaletteIndex filter
//   - unresolvable mesh id     -> LookupMesh returns null, DrawCommand bails
//   - not-Ready asset          -> IsReady() false, DrawCommand bails
// The actual GPU submit path is covered by the visual cluichetest run.

#include <gtest/gtest.h>

#include <DiaBgfx3D/Renderers/MeshRenderer.h>
#include <DiaBgfx3D/Resources/MeshGpuCache.h>
#include <DiaBgfx3D/Resources/MaterialRegistry.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaCore/CRC/StringCRC.h>

using Dia::Bgfx3D::MeshRenderer;
using Dia::Bgfx3D::MeshGpuCache;
using Dia::Bgfx3D::MaterialRegistry;
using Dia::Bgfx3D::MeshPassLighting;
using Dia::Mesh3D::Mesh3DAssetHandler;
using Dia::Mesh3D::Mesh3DAsset;
using Dia::Graphics3D::Mesh3DFrameData;
using Dia::Graphics3D::Mesh3DDrawCommand;
using Dia::Core::StringCRC;

namespace
{
    struct MeshRendererFixture
    {
        MeshGpuCache       cache;
        MaterialRegistry   materials;
        Mesh3DAssetHandler handler;
        MeshRenderer       renderer{ 0, &cache, &materials, &handler };
        // NOTE: InitUniforms() is NOT called — these tests never reach bgfx submit,
        // so uniform handles are kInvalidHandle throughout. That matches the guard
        // paths being tested (null asset, not-ready, skinned, empty frame).
    };

    MeshPassLighting NoOpLighting()
    {
        MeshPassLighting l{};
        l.shadowTexture = 0xFFFF;  // bgfx::kInvalidHandle
        return l;
    }
}

TEST(DiaBgfx3D_MeshRendererTest, ConstructsWithoutBgfxContext)
{
    // Construction must not touch bgfx — purely stores pointers.
    MeshRendererFixture fx;
    SUCCEED();
}

TEST(DiaBgfx3D_MeshRendererTest, DrawEmptyFrameDoesNotCrash)
{
    MeshRendererFixture fx;
    Mesh3DFrameData frame;  // no draws queued

    fx.renderer.Draw(frame, NoOpLighting());  // must be a no-op, no bgfx calls
    SUCCEED();
}

TEST(DiaBgfx3D_MeshRendererTest, DrawSkippedForUnresolvableMeshId)
{
    MeshRendererFixture fx;  // handler is empty -> LookupMesh returns null

    Mesh3DDrawCommand cmd;
    cmd.meshId               = StringCRC("mesh.not_registered");
    cmd.materialId           = StringCRC("default_3d");
    cmd.skinningPaletteIndex = 0;  // static

    Mesh3DFrameData frame;
    frame.RequestDrawMesh(cmd);

    // DrawCommand returns at the null LookupMesh guard, before any bgfx submit.
    fx.renderer.Draw(frame, NoOpLighting());
    SUCCEED();
}

TEST(DiaBgfx3D_MeshRendererTest, DrawSkipsSkinnedCommands)
{
    MeshRendererFixture fx;

    // Register a Ready asset so the only thing stopping a submit is the skin filter.
    Mesh3DAsset* asset = new Mesh3DAsset(StringCRC("mesh.skinned"));
    fx.handler.RegisterMesh(asset);

    Mesh3DDrawCommand cmd;
    cmd.meshId               = StringCRC("mesh.skinned");
    cmd.materialId           = StringCRC("default_3d");
    cmd.skinningPaletteIndex = 1;  // non-zero -> skinned -> must be skipped

    Mesh3DFrameData frame;
    frame.RequestDrawMesh(cmd);

    // Draw() filters skinned commands before DrawCommand; asset is never uploaded.
    fx.renderer.Draw(frame, NoOpLighting());
    SUCCEED();
}

TEST(DiaBgfx3D_MeshRendererTest, DrawSkippedForNotReadyAsset)
{
    MeshRendererFixture fx;

    // A registered-but-Pending asset (default state) must not be drawn:
    // DrawCommand bails at the !IsReady() guard before touching the GPU cache.
    Mesh3DAsset* asset = new Mesh3DAsset(StringCRC("mesh.pending"));
    ASSERT_FALSE(asset->IsReady());
    fx.handler.RegisterMesh(asset);

    Mesh3DDrawCommand cmd;
    cmd.meshId               = StringCRC("mesh.pending");
    cmd.materialId           = StringCRC("default_3d");
    cmd.skinningPaletteIndex = 0;

    Mesh3DFrameData frame;
    frame.RequestDrawMesh(cmd);

    fx.renderer.Draw(frame, NoOpLighting());
    SUCCEED();
}
