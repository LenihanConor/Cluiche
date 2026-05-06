////////////////////////////////////////////////////////////////////////////////
// TestIK2DVisualDebuggerStack.cpp
// Tests for the 4 focused IVisualDebugger draw classes in DiaIK2DVisualDebugger,
// plus the 6 IKSolver accessor methods added to DiaIK2D.
// Feature spec: docs/specs/features/dia/diavisualdebugger/ik2d-visual-debugger-stack.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifndef DIA_DEBUG
#define DIA_DEBUG
#endif
#include <DiaIK2DVisualDebugger/IKChainBonesDrawer.h>
#include <DiaIK2DVisualDebugger/IKChainJointsDrawer.h>
#include <DiaIK2DVisualDebugger/IKChainArrowsDrawer.h>
#include <DiaIK2DVisualDebugger/IKReachCirclesDrawer.h>

#include <DiaIK2D/IKSolver.h>
#include <DiaIK2D/IKChainDef.h>
#include <DiaIK2D/Testing/IKTestHelpers.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>

#include <DiaRig2D/Skeleton.h>
#include <DiaRig2D/Pose.h>
#include <DiaRig2D/BoneTransform.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Testing/MockVisitors.h>

#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::IK2D;
using namespace Dia::IK2D::Testing;
using namespace Dia::Debug;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

// ============================================================================
// Shared test fixture: 3-bone straight chain, one IK chain registered
// skeleton: bone0 (root), bone1 (parent=0), bone2 (parent=1)
// IK chain: id="arm", startBone="bone0", endBone="bone2"
// ============================================================================

struct IK3BoneFixture
{
    Dia::Rig2D::SkeletonDef skeletonDef;
    Dia::Rig2D::Skeleton    skeleton;
    Dia::Rig2D::Pose        pose;
    IKSolver                solver;
    Dia::Debug::DebugLayerManager manager;

    IK3BoneFixture()
        : skeletonDef(BuildLimbSkeletonDef(3, 1.0f))
        , skeleton(skeletonDef)
        , pose(skeleton)
        , solver(skeleton, pose)
    {
        Dia::Rig2D::BoneTransform identity;
        solver.SetRootTransform(identity);

        IKChainDef chain;
        chain.id          = Dia::Core::StringCRC("arm");
        chain.startBoneId = Dia::Core::StringCRC("bone0");
        chain.endBoneId   = Dia::Core::StringCRC("bone2");
        solver.RegisterChain(chain);
    }
};

// ---------------------------------------------------------------------------
// Helper: returns a RecordingDebugVisitor populated from fd
// ---------------------------------------------------------------------------
static RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

// ---------------------------------------------------------------------------
// Helper: captures all primitives from fd for field-level inspection
// ---------------------------------------------------------------------------
struct PrimitiveCapture : public DebugFrameDataVisitor
{
    static constexpr int kMax = 64;
    mutable DebugPrimitive prims[kMax] = {};
    mutable int            count = 0;

    void Visit(const DebugPrimitive& p) const override
    {
        if (count < kMax) prims[count++] = p;
    }
    void Visit(const DebugFrameData&) const override {}
};

static PrimitiveCapture Capture(const FrameData& fd)
{
    PrimitiveCapture c;
    static_cast<const DebugFrameData&>(fd).AcceptVisitor(c);
    return c;
}

// ============================================================================
// IKSolver accessors — 4 tests
// ============================================================================

TEST(IKSolverAccessors, GetChainCount_AfterRegister_IsOne)
{
    IK3BoneFixture f;
    EXPECT_EQ(f.solver.GetChainCount(), 1);
}

TEST(IKSolverAccessors, GetChainId_ReturnsRegisteredId)
{
    IK3BoneFixture f;
    EXPECT_EQ(f.solver.GetChainId(0), Dia::Core::StringCRC("arm"));
}

TEST(IKSolverAccessors, GetChainStartEndBoneIndex_MatchDef)
{
    IK3BoneFixture f;
    // BuildLimbSkeletonDef(3, 1.0f): bone0=index0, bone1=index1, bone2=index2
    EXPECT_EQ(f.solver.GetChainStartBoneIndex(0), 0);
    EXPECT_EQ(f.solver.GetChainEndBoneIndex(0),   2);
}

TEST(IKSolverAccessors, GetWorldTransforms_SizeMatchesBoneCount)
{
    IK3BoneFixture f;
    EXPECT_EQ(static_cast<int>(f.solver.GetWorldTransforms().Size()),
              f.skeleton.GetBoneCount());
}

// ============================================================================
// IKChainBonesDrawer — 4 tests
// ============================================================================

TEST(IKChainBonesDrawer, Draw_ThreeBoneChain_TwoLines)
{
    IK3BoneFixture f;
    IKChainBonesDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    // 3 bones (index 0,1,2); bone0 has no parent → skipped → 2 lines
    EXPECT_EQ(Inspect(fd).LineCount(), 2);
}

TEST(IKChainBonesDrawer, Draw_Colour_IsGoal)
{
    IK3BoneFixture f;
    IKChainBonesDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    auto c = Capture(fd);
    ASSERT_GE(c.count, 1);
    ASSERT_EQ(c.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(c.prims[0].line2D.colour, DebugColourPalette::kGoal);
}

TEST(IKChainBonesDrawer, Draw_Disabled_NoPrimitives)
{
    IK3BoneFixture f;
    IKChainBonesDrawer drawer(f.solver, f.skeleton, f.manager);

    f.manager.Register(&drawer, 10);
    f.manager.DisableLayer(LayerNames::kIKBones);

    FrameData fd;
    f.manager.Draw(fd);
    EXPECT_EQ(Inspect(fd).LineCount(), 0);
}

TEST(IKChainBonesDrawer, LayerName_IsIKBones)
{
    IK3BoneFixture f;
    IKChainBonesDrawer drawer(f.solver, f.skeleton, f.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kIKBones);
}

// ============================================================================
// IKChainJointsDrawer — 4 tests
// ============================================================================

TEST(IKChainJointsDrawer, Draw_ThreeBones_ThreeCircles)
{
    IK3BoneFixture f;
    IKChainJointsDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(Inspect(fd).CircleCount(), 3);
}

TEST(IKChainJointsDrawer, Draw_EndBone_IsHealthy)
{
    IK3BoneFixture f;
    IKChainJointsDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    // End bone is drawn last (i=endIdx=2 in loop i=startIdx..endIdx) → primitive index 2
    auto c = Capture(fd);
    ASSERT_EQ(c.count, 3);
    ASSERT_EQ(c.prims[2].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(c.prims[2].circle2D.outlineColour, DebugColourPalette::kHealthy);
}

TEST(IKChainJointsDrawer, Draw_StartBone_IsGoal)
{
    IK3BoneFixture f;
    IKChainJointsDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    // Start bone is drawn first (i=startIdx=0 in loop) → primitive index 0
    auto c = Capture(fd);
    ASSERT_GE(c.count, 1);
    ASSERT_EQ(c.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(c.prims[0].circle2D.outlineColour, DebugColourPalette::kGoal);
}

TEST(IKChainJointsDrawer, LayerName_IsIKJoints)
{
    IK3BoneFixture f;
    IKChainJointsDrawer drawer(f.solver, f.skeleton, f.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kIKJoints);
}

// ============================================================================
// IKChainArrowsDrawer — 3 tests
// ============================================================================

TEST(IKChainArrowsDrawer, Draw_ThreeBones_ThreeRays)
{
    IK3BoneFixture f;
    IKChainArrowsDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(Inspect(fd).RayCount(), 3);
}

TEST(IKChainArrowsDrawer, Draw_Colour_IsGoal)
{
    IK3BoneFixture f;
    IKChainArrowsDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    auto c = Capture(fd);
    ASSERT_GE(c.count, 1);
    ASSERT_EQ(c.prims[0].type, DebugPrimitiveType::Ray2D);
    EXPECT_EQ(c.prims[0].ray2D.colour, DebugColourPalette::kGoal);
}

TEST(IKChainArrowsDrawer, LayerName_IsIKArrows)
{
    IK3BoneFixture f;
    IKChainArrowsDrawer drawer(f.solver, f.skeleton, f.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kIKArrows);
}

// ============================================================================
// IKReachCirclesDrawer — 4 tests
// ============================================================================

TEST(IKReachCirclesDrawer, Draw_Chain_DrawsOneCircle)
{
    IK3BoneFixture f;
    IKReachCirclesDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    EXPECT_EQ(Inspect(fd).CircleCount(), 1);
}

TEST(IKReachCirclesDrawer, Draw_Radius_SumOfBoneLengths)
{
    IK3BoneFixture f;
    // BuildLimbSkeletonDef(3, 1.0f): each segment is 1.0f long.
    // Skeleton::ComputeBoneLengths() sets root bone (bone0) length = 0.0f.
    // bone1.length=1.0f, bone2.length=1.0f.
    // Chain: startIdx=0, endIdx=2.
    // Spec formula: sum bone.length for i in [startIdx, endIdx-1) = bone0+bone1 = 0+1 = 1.0f
    // scale=1.0f (default) → drawn radius=1.0f
    IKReachCirclesDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    auto c = Capture(fd);
    ASSERT_EQ(c.count, 1);
    ASSERT_EQ(c.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_NEAR(c.prims[0].circle2D.radius, 1.0f, 1e-4f);
}

TEST(IKReachCirclesDrawer, Draw_Colour_IsInactive)
{
    IK3BoneFixture f;
    IKReachCirclesDrawer drawer(f.solver, f.skeleton, f.manager);

    FrameData fd;
    drawer.Draw(fd);

    auto c = Capture(fd);
    ASSERT_GE(c.count, 1);
    ASSERT_EQ(c.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(c.prims[0].circle2D.outlineColour, DebugColourPalette::kInactive);
}

TEST(IKReachCirclesDrawer, LayerName_IsIKReach)
{
    IK3BoneFixture f;
    IKReachCirclesDrawer drawer(f.solver, f.skeleton, f.manager);
    EXPECT_EQ(drawer.GetLayerName(), LayerNames::kIKReach);
}
