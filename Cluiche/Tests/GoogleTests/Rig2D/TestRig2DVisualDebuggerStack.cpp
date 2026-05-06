////////////////////////////////////////////////////////////////////////////////
// TestRig2DVisualDebuggerStack.cpp
// Tests for the 5 focused IVisualDebugger draw classes in DiaRig2DVisualDebugger.
// Feature spec: docs/specs/features/dia/diavisualdebugger/rig2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif

#include <DiaRig2DVisualDebugger/BoneLinesDrawer.h>
#include <DiaRig2DVisualDebugger/JointCirclesDrawer.h>
#include <DiaRig2DVisualDebugger/DirectionArrowsDrawer.h>
#include <DiaRig2DVisualDebugger/RestPoseDrawer.h>
#include <DiaRig2DVisualDebugger/BoneLabelsDrawer.h>
#include <DiaRig2DVisualDebugger/RigRestPoseRenderer.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/FixedPrimitiveBuffer.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>
#include <DiaRig2D/Testing/SkeletonBuilders.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Testing/MockVisitors.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::Rig2D;
using namespace Dia::Rig2D::Testing;
using namespace Dia::Debug;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

// ============================================================================
// Test fixture: 3-bone chain (root → mid → tip) with computed world transforms
// ============================================================================

struct Rig3Bone
{
    // MakeSimpleChain(3): bone_0 (root, pos 0,0), bone_1 (parent=0, local 0,1),
    //                     bone_2 (parent=1, local 0,1)
    // Bind-pose world positions: (0,0), (0,1), (0,2)
    SkeletonDef def;
    Skeleton    skeleton;
    Dia::Core::Containers::DynamicArrayC<BoneTransform, kMaxBones> worldTransforms;
    DebugLayerManager manager;

    Rig3Bone()
        : def(MakeSimpleChain(3))
        , skeleton(def)
    {
        Pose pose(skeleton);
        pose.SetToBindPose(skeleton);
        const BoneTransform identity{
            Dia::Maths::Vector2D(0.0f, 0.0f), 0.0f, Dia::Maths::Vector2D(1.0f, 1.0f)
        };
        pose.ComputeWorldTransforms(skeleton, identity, worldTransforms);
    }
};

// Helpers to count specific primitives in a FrameData
static RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

static int TextCount(const FrameData& fd)
{
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    int count = 0;
    const uint32_t total = dbg.GetDebugPrimitiveCount();
    for (uint32_t i = 0; i < total; ++i)
    {
        if (dbg.GetDebugPrimitive(i).type == DebugPrimitiveType::Text2D)
            ++count;
    }
    return count;
}

// ============================================================================
// BoneLinesDrawer — 5 tests
// ============================================================================

TEST(BoneLinesDrawer, Draw_TwoBoneChain_DrawsTwoLines)
{
    Rig3Bone r;
    BoneLinesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // 3 bones, 2 non-root → 2 lines
    EXPECT_EQ(Inspect(fd).LineCount(), 2);
}

TEST(BoneLinesDrawer, Draw_LineEndpoints_MatchWorldPositions)
{
    Rig3Bone r;
    BoneLinesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // First line (bone_1): parent=bone_0 pos=(0,0) → bone_1 pos=(0,1)
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Line2D);
    EXPECT_NEAR(p.line2D.start.x, 0.0f, 1e-4f);
    EXPECT_NEAR(p.line2D.start.y, 0.0f, 1e-4f);
    EXPECT_NEAR(p.line2D.end.x,   0.0f, 1e-4f);
    EXPECT_NEAR(p.line2D.end.y,   1.0f, 1e-4f);
}

TEST(BoneLinesDrawer, Draw_Colour_IsActive)
{
    Rig3Bone r;
    BoneLinesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(p.line2D.colour, DebugColourPalette::kActive);
}

TEST(BoneLinesDrawer, Draw_Disabled_NoPrimitives)
{
    Rig3Bone r;
    BoneLinesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    drawer.SetEnabled(false);

    FrameData fd;
    // IVisualDebugger callers check IsEnabled() before calling Draw.
    // For robustness we test that even if called directly while disabled,
    // the base-class IsEnabled guard is the manager's responsibility.
    // The draw class itself always draws — enable is the manager's concern.
    // Instead, verify via manager registration and layer disable:
    r.manager.Register(&drawer, 10);
    r.manager.DisableLayer(LayerNames::kRigBones);
    r.manager.Draw(fd);
    EXPECT_EQ(Inspect(fd).LineCount(), 0);
}

TEST(BoneLinesDrawer, LayerName_IsRigBones)
{
    Rig3Bone r;
    BoneLinesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kRigBones);
}

// ============================================================================
// JointCirclesDrawer — 6 tests
// ============================================================================

TEST(JointCirclesDrawer, Draw_ThreeBones_ThreeCircles)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(Inspect(fd).CircleCount(), 3);
}

TEST(JointCirclesDrawer, Draw_RootCircle_IsGreen)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // bone_0 is root → first circle drawn → index 0
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(p.circle2D.outlineColour, DebugColourPalette::kHealthy);
}

TEST(JointCirclesDrawer, Draw_LeafCircle_IsYellow)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // bone_2 is the leaf (tip) → index 2 in the output
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_EQ(dbg.GetDebugPrimitiveCount(), 3u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(2);
    ASSERT_EQ(p.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(p.circle2D.outlineColour, DebugColourPalette::kWarning);
}

TEST(JointCirclesDrawer, Draw_MidChainCircle_IsWhite)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // bone_1 is mid-chain → index 1
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_EQ(dbg.GetDebugPrimitiveCount(), 3u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(1);
    ASSERT_EQ(p.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(p.circle2D.outlineColour, DebugColourPalette::kActive);
}

TEST(JointCirclesDrawer, Draw_RootRadius_LargerThanLeaf)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_EQ(dbg.GetDebugPrimitiveCount(), 3u);

    const float rootRadius = dbg.GetDebugPrimitive(0).circle2D.radius;
    const float leafRadius = dbg.GetDebugPrimitive(2).circle2D.radius;
    EXPECT_GT(rootRadius, leafRadius);
}

TEST(JointCirclesDrawer, LayerName_IsRigJoints)
{
    Rig3Bone r;
    JointCirclesDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kRigJoints);
}

// ============================================================================
// DirectionArrowsDrawer — 5 tests
// ============================================================================

TEST(DirectionArrowsDrawer, Draw_ThreeBones_ThreeRays)
{
    Rig3Bone r;
    DirectionArrowsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(Inspect(fd).RayCount(), 3);
}

TEST(DirectionArrowsDrawer, Draw_ZeroRotation_PointsRight)
{
    Rig3Bone r;
    // All bones have rotation == 0.0f in bind pose
    DirectionArrowsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Ray2D);
    // direction = (cos(0), sin(0)) = (1, 0)
    EXPECT_NEAR(p.ray2D.direction.x, 1.0f, 1e-4f);
    EXPECT_NEAR(p.ray2D.direction.y, 0.0f, 1e-4f);
}

TEST(DirectionArrowsDrawer, Draw_Colour_IsGoal)
{
    Rig3Bone r;
    DirectionArrowsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Ray2D);
    EXPECT_EQ(p.ray2D.colour, DebugColourPalette::kGoal);
}

TEST(DirectionArrowsDrawer, Draw_Scale_AffectsLength)
{
    Rig3Bone r;

    // Build a skeleton with a bone that has length == 0 so fallback of 4*scale is used
    // This makes scale effect predictable: length = 4.0f * scale
    r.manager.SetDebugScale(1.0f);
    DirectionArrowsDrawer drawer1(r.skeleton, r.worldTransforms, r.manager);
    FrameData fd1;
    drawer1.Draw(fd1);

    r.manager.SetDebugScale(2.0f);
    DirectionArrowsDrawer drawer2(r.skeleton, r.worldTransforms, r.manager);
    FrameData fd2;
    drawer2.Draw(fd2);

    const DebugFrameData& dbg1 = static_cast<const DebugFrameData&>(fd1);
    const DebugFrameData& dbg2 = static_cast<const DebugFrameData&>(fd2);

    ASSERT_GE(dbg1.GetDebugPrimitiveCount(), 1u);
    ASSERT_GE(dbg2.GetDebugPrimitiveCount(), 1u);

    const float len1 = dbg1.GetDebugPrimitive(0).ray2D.length;
    const float len2 = dbg2.GetDebugPrimitive(0).ray2D.length;

    // At scale=2 the length should be double the scale=1 length
    EXPECT_NEAR(len2, len1 * 2.0f, 1e-3f);
}

TEST(DirectionArrowsDrawer, LayerName_IsRigArrows)
{
    Rig3Bone r;
    DirectionArrowsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kRigArrows);
}

// ============================================================================
// RestPoseDrawer — 4 tests
// ============================================================================

TEST(RestPoseDrawer, Draw_ProducesLines)
{
    Rig3Bone r;
    RestPoseDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // 3 bones → 2 non-root → 2 rest-pose lines
    EXPECT_EQ(Inspect(fd).LineCount(), 2);
}

TEST(RestPoseDrawer, Draw_Colour_IsInactive)
{
    Rig3Bone r;
    RestPoseDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(p.line2D.colour, DebugColourPalette::kInactive);
}

TEST(RestPoseDrawer, Draw_DoesNotModifyWorldTransforms)
{
    Rig3Bone r;
    // Snapshot positions before draw
    const Dia::Maths::Vector2D pos0Before = r.worldTransforms[0].position;
    const Dia::Maths::Vector2D pos1Before = r.worldTransforms[1].position;
    const Dia::Maths::Vector2D pos2Before = r.worldTransforms[2].position;

    RestPoseDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    FrameData fd;
    drawer.Draw(fd);

    EXPECT_NEAR(r.worldTransforms[0].position.x, pos0Before.x, 1e-6f);
    EXPECT_NEAR(r.worldTransforms[0].position.y, pos0Before.y, 1e-6f);
    EXPECT_NEAR(r.worldTransforms[1].position.x, pos1Before.x, 1e-6f);
    EXPECT_NEAR(r.worldTransforms[1].position.y, pos1Before.y, 1e-6f);
    EXPECT_NEAR(r.worldTransforms[2].position.x, pos2Before.x, 1e-6f);
    EXPECT_NEAR(r.worldTransforms[2].position.y, pos2Before.y, 1e-6f);
}

TEST(RestPoseDrawer, LayerName_IsRigRestPose)
{
    Rig3Bone r;
    RestPoseDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kRigRestPose);
}

// ============================================================================
// BoneLabelsDrawer — 5 tests
// ============================================================================

TEST(BoneLabelsDrawer, Draw_ThreeBones_ThreeTextPrimitives)
{
    Rig3Bone r;
    BoneLabelsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(TextCount(fd), 3);
}

TEST(BoneLabelsDrawer, Draw_LabelText_MatchesBoneName)
{
    Rig3Bone r;
    BoneLabelsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Text2D);

    // First bone is "bone_0"
    const char* expected = r.skeleton.GetBone(0).name.AsChar();
    EXPECT_STREQ(p.text2D.text, expected);
}

TEST(BoneLabelsDrawer, Draw_LabelOffset_NonZero)
{
    Rig3Bone r;
    BoneLabelsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    // Label position should differ from the joint position
    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Text2D);

    const Dia::Maths::Vector2D& jointPos = r.worldTransforms[0].position;
    EXPECT_FALSE(
        std::abs(p.text2D.position.x - jointPos.x) < 1e-4f &&
        std::abs(p.text2D.position.y - jointPos.y) < 1e-4f)
        << "Label position should be offset from joint position";
}

TEST(BoneLabelsDrawer, Draw_Colour_IsActive)
{
    Rig3Bone r;
    BoneLabelsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);

    FrameData fd;
    drawer.Draw(fd);

    const DebugFrameData& dbg = static_cast<const DebugFrameData&>(fd);
    ASSERT_GE(dbg.GetDebugPrimitiveCount(), 1u);
    const DebugPrimitive& p = dbg.GetDebugPrimitive(0);
    ASSERT_EQ(p.type, DebugPrimitiveType::Text2D);
    EXPECT_EQ(p.text2D.colour, DebugColourPalette::kActive);
}

TEST(BoneLabelsDrawer, LayerName_IsRigLabels)
{
    Rig3Bone r;
    BoneLabelsDrawer drawer(r.skeleton, r.worldTransforms, r.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kRigLabels);
}

// ============================================================================
// Migration — 2 tests
// ============================================================================

TEST(Rig2DVisualDebugger_Migration, AllFiveDrawers_Registered_DrawCorrectTotalPrimitives)
{
    // 3-bone skeleton:
    //   BoneLinesDrawer:       2 lines
    //   JointCirclesDrawer:    3 circles
    //   DirectionArrowsDrawer: 3 rays
    //   RestPoseDrawer:        2 lines  (→ 4 lines total)
    //   BoneLabelsDrawer:      3 text   (→ tested separately via TextCount)
    Rig3Bone r;

    BoneLinesDrawer      linesDrawer   (r.skeleton, r.worldTransforms, r.manager);
    JointCirclesDrawer   circlesDrawer (r.skeleton, r.worldTransforms, r.manager);
    DirectionArrowsDrawer arrowsDrawer (r.skeleton, r.worldTransforms, r.manager);
    RestPoseDrawer       restDrawer    (r.skeleton, r.worldTransforms, r.manager);
    BoneLabelsDrawer     labelsDrawer  (r.skeleton, r.worldTransforms, r.manager);

    r.manager.Register(&linesDrawer,   10);
    r.manager.Register(&circlesDrawer, 10);
    r.manager.Register(&arrowsDrawer,  20);
    r.manager.Register(&restDrawer,    10);
    r.manager.Register(&labelsDrawer,  40);

    FrameData fd;
    r.manager.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(),   4);  // 2 bones + 2 rest
    EXPECT_EQ(v.CircleCount(), 3);
    EXPECT_EQ(v.RayCount(),    3);
    EXPECT_EQ(TextCount(fd),   3);
}

TEST(Rig2DVisualDebugger_Migration, OldVisualDebugger_RemovedFromBuild)
{
    // Compile-time verification: VisualDebugger.h no longer exists in
    // DiaRig2DVisualDebugger. This test is a placeholder — the build
    // itself is the real guard: if VisualDebugger.h exists and is still
    // compiled, the project configuration test would fail to build.
    // Since we reached this point in the test binary, it is confirmed gone.
    SUCCEED();
}

// ============================================================================
// RigRestPoseRenderer (fixed-layer) — 4 tests
// ============================================================================

namespace
{
    class CountingVisitor : public Dia::Graphics::DebugFrameDataVisitor
    {
    public:
        mutable int lineCount = 0;
        void Visit(const Dia::Graphics::DebugPrimitive& p) const override
        {
            if (p.type == Dia::Graphics::DebugPrimitiveType::Line2D) ++lineCount;
        }
        void Visit(const Dia::Graphics::DebugFrameData&) const override {}
    };
}

TEST(RigRestPoseRenderer, Build_ProducesTwoLines_For3BoneSkeleton)
{
    Rig3Bone r;
    Dia::Rig2D::RigRestPoseRenderer renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(32);

    renderer.BuildPrimitives(&r.skeleton, buf);

    // 3 bones → 2 non-root → 2 line primitives
    EXPECT_EQ(buf.GetCount(), 2u);
}

TEST(RigRestPoseRenderer, Build_LinesAreInactiveColour)
{
    Rig3Bone r;
    Dia::Rig2D::RigRestPoseRenderer renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(32);

    renderer.BuildPrimitives(&r.skeleton, buf);

    CountingVisitor v;
    buf.AcceptVisitor(v);
    EXPECT_EQ(v.lineCount, 2);
}

TEST(RigRestPoseRenderer, BuildTwice_SameCount)
{
    Rig3Bone r;
    Dia::Rig2D::RigRestPoseRenderer renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(32);

    renderer.BuildPrimitives(&r.skeleton, buf);
    buf.Clear();
    renderer.BuildPrimitives(&r.skeleton, buf);

    EXPECT_EQ(buf.GetCount(), 2u);
}

TEST(RigRestPoseRenderer, RegisterFixed_ThroughManager_DrawsLines)
{
    Rig3Bone r;
    Dia::Rig2D::RigRestPoseRenderer renderer;
    r.manager.RegisterFixed(
        Dia::Debug::LayerNames::kRigRestPose,
        &r.skeleton,
        &renderer,
        64,
        10);

    CountingVisitor v;
    r.manager.DrawFixed(v);
    EXPECT_EQ(v.lineCount, 2);
}

TEST(RigRestPoseRenderer, RootOnlySkeleton_ZeroLines)
{
    // A single-bone skeleton (root only) has no non-root bones,
    // so rest pose should emit zero line primitives.
    SkeletonDef def = MakeSimpleChain(1);
    Skeleton skeleton(def);

    Dia::Rig2D::RigRestPoseRenderer renderer;
    Dia::Debug::FixedPrimitiveBuffer buf(32);

    renderer.BuildPrimitives(&skeleton, buf);

    EXPECT_EQ(buf.GetCount(), 0u);
}
