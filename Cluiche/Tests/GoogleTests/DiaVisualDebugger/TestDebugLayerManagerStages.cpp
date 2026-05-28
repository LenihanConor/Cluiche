////////////////////////////////////////////////////////////////////////////////
// TestDebugLayerManagerStages.cpp
// Tests for DebugLayerManager stage-tagging: SetStageActive, IsStageActive,
// GetStageTags, Register idempotency, Draw skipping inactive layers.
// Feature spec: docs/specs/features/dia/diavisualdebugger/shared-debug-console.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Debug;
using namespace Dia::Core;

// ============================================================================
// MockLayer — minimal IVisualDebugger for stage tests.
// Tracks whether Draw() was called.
// ============================================================================

struct MockLayer : public IVisualDebugger
{
    StringCRC mName;
    mutable bool drawCalled = false;

    explicit MockLayer(const char* n) : mName(n) {}

    StringCRC GetLayerName() const override { return mName; }

    void Draw(Dia::Graphics::FrameData& /*frameData*/) override
    {
        drawCalled = true;
    }
};

// ============================================================================
// Stage registration suite
// ============================================================================

TEST(DebugLayerManagerStages, Register_WithStageTag_AppearsByTag)
{
    DebugLayerManager mgr;
    MockLayer layer("physics.shapes");
    mgr.Register(&layer, 0, StringCRC("RigidBody2DTestStage"));

    Containers::DynamicArrayC<StringCRC, 16> tags;
    mgr.GetStageTags(tags);

    ASSERT_EQ(tags.Size(), 1u);
    EXPECT_EQ(tags[0], StringCRC("RigidBody2DTestStage"));
}

TEST(DebugLayerManagerStages, SetStageActive_False_DeactivatesLayer)
{
    DebugLayerManager mgr;
    MockLayer layer("stage.layer.a");
    mgr.Register(&layer, 0, StringCRC("stage1"));

    mgr.SetStageActive(StringCRC("stage1"), false);

    EXPECT_FALSE(mgr.IsStageActive(StringCRC("stage1")));
}

TEST(DebugLayerManagerStages, SetStageActive_True_ReactivatesLayer)
{
    DebugLayerManager mgr;
    MockLayer layer("stage.layer.b");
    mgr.Register(&layer, 0, StringCRC("stage1"));

    mgr.SetStageActive(StringCRC("stage1"), false);
    ASSERT_FALSE(mgr.IsStageActive(StringCRC("stage1")));

    mgr.SetStageActive(StringCRC("stage1"), true);
    EXPECT_TRUE(mgr.IsStageActive(StringCRC("stage1")));
}

TEST(DebugLayerManagerStages, Draw_SkipsInactiveLayers)
{
    DebugLayerManager mgr;
    MockLayer layer("stage.draw.skip");
    mgr.Register(&layer, 0, StringCRC("testStage"));

    mgr.SetStageActive(StringCRC("testStage"), false);

    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_FALSE(layer.drawCalled);

    mgr.SetStageActive(StringCRC("testStage"), true);
    layer.drawCalled = false;
    mgr.Draw(fd);
    EXPECT_TRUE(layer.drawCalled);
}

TEST(DebugLayerManagerStages, Register_WithoutStageTag_UnaffectedBySetStageActive)
{
    DebugLayerManager mgr;
    MockLayer global("global.layer");
    mgr.Register(&global, 0);  // no stageTag — global layer

    mgr.SetStageActive(StringCRC("someStage"), false);

    // Global layer (empty tag) is always active — Draw() must still be called
    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_TRUE(global.drawCalled);
}

TEST(DebugLayerManagerStages, Register_SamePointerReentry_IsIdempotent)
{
    DebugLayerManager mgr;
    MockLayer layer("stage.idempotent");
    mgr.Register(&layer, 0, StringCRC("stage1"));

    // Deactivate, then re-register with same pointer — should reactivate
    mgr.SetStageActive(StringCRC("stage1"), false);
    ASSERT_FALSE(mgr.IsStageActive(StringCRC("stage1")));

    mgr.Register(&layer, 0, StringCRC("stage1"));
    EXPECT_TRUE(mgr.IsStageActive(StringCRC("stage1")));
}

TEST(DebugLayerManagerStages, GetStageTags_ReturnsUniqueStages)
{
    DebugLayerManager mgr;
    MockLayer a("stage.unique.a"), b("stage.unique.b"), c("stage.unique.c");
    mgr.Register(&a, 0, StringCRC("stage1"));
    mgr.Register(&b, 0, StringCRC("stage1"));
    mgr.Register(&c, 0, StringCRC("stage2"));

    Containers::DynamicArrayC<StringCRC, 16> tags;
    mgr.GetStageTags(tags);

    EXPECT_EQ(tags.Size(), 2u);

    bool hasStage1 = false;
    bool hasStage2 = false;
    for (unsigned int i = 0; i < tags.Size(); ++i)
    {
        if (tags[i] == StringCRC("stage1")) hasStage1 = true;
        if (tags[i] == StringCRC("stage2")) hasStage2 = true;
    }
    EXPECT_TRUE(hasStage1);
    EXPECT_TRUE(hasStage2);
}
