#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.h>
#include <DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.h>

#include <DiaSoftBody2D/SoftBodyWorld.h>
#include <DiaSoftBody2D/Rope.h>
#include <DiaSoftBody2D/Cloth.h>
#include <DiaSoftBody2D/Particle.h>
#include <DiaSoftBody2D/Constraints/DistanceConstraint.h>
#include <DiaSoftBody2D/Testing/SoftBodyWorldBuilder.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Testing/PhysicsWorldBuilder.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <memory>

using namespace Dia::SoftBody2D;
using namespace Dia::SoftBody2D::Testing;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const Dia::Graphics::DebugFrameData& AsDebug(const FrameData& fd)
{
    return static_cast<const Dia::Graphics::DebugFrameData&>(fd);
}

static RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    return v;
}

// PrimitiveCapture is large (~22KB) — pass by reference to avoid stack copies.
struct PrimitiveCapture : public DebugFrameDataVisitor
{
    static constexpr int kMax = 256;
    mutable DebugPrimitive prims[kMax];
    mutable int            count = 0;

    PrimitiveCapture() : count(0) {}

    void Visit(const DebugPrimitive& p) const override
    {
        if (count < kMax) prims[count++] = p;
    }
    void Visit(const DebugFrameData&) const override {}
};

// Pass by reference — do not return PrimitiveCapture by value (too large for reliable stack return)
static void DoCapture(const FrameData& fd, PrimitiveCapture& cap)
{
    AsDebug(fd).AcceptVisitor(cap);
}

// ---------------------------------------------------------------------------
// Shared world setups
// ---------------------------------------------------------------------------

// 3-particle rope with no anchors
static std::unique_ptr<SoftBodyWorld> MakeRopeWorld(int particleCount = 3)
{
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(2.0f, 0.0f);
    def.particleCount = particleCount;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = nullptr;
    def.endAnchor     = nullptr;
    world->AddRope(def);
    return world;
}

// 2x2 cloth (4 particles total)
static std::unique_ptr<SoftBodyWorld> MakeClothWorld(bool pinTopRow = false)
{
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    ClothDef def;
    def.id            = Dia::Core::StringCRC("cloth");
    def.origin        = Vector2D(0.0f, 0.0f);
    def.width         = 1.0f;
    def.height        = 1.0f;
    def.resX          = 2;
    def.resY          = 2;
    def.mass          = 1.0f;
    def.particleRadius = 0.05f;
    def.pinTopRow     = pinTopRow;
    world->AddCloth(def);
    return world;
}

// ===========================================================================
// SoftParticlesDrawer
// ===========================================================================

TEST(SoftBody2DVisualDebugger_ParticlesDrawer, Draw_ThreeParticles_ThreeCircles)
{
    auto world = MakeRopeWorld(3);
    Dia::Debug::DebugLayerManager manager;
    SoftParticlesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.CircleCount(), 3);
}

TEST(SoftBody2DVisualDebugger_ParticlesDrawer, Draw_PinnedParticle_IsMagenta)
{
    // A rope with a start anchor pins particle 0 (invMass set to 0 by Rope ctor)
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(2.0f, 0.0f);
    def.particleCount = 3;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0x1);
    def.endAnchor     = nullptr;
    world->AddRope(def);

    Dia::Debug::DebugLayerManager manager;
    SoftParticlesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kPinned);
}

TEST(SoftBody2DVisualDebugger_ParticlesDrawer, Draw_DynamicParticle_IsWhite)
{
    auto world = MakeRopeWorld(3);  // No anchors → all dynamic

    Dia::Debug::DebugLayerManager manager;
    SoftParticlesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kActive);
}

TEST(SoftBody2DVisualDebugger_ParticlesDrawer, Draw_Disabled_NoPrimitives)
{
    auto world = MakeRopeWorld(3);

    Dia::Debug::DebugLayerManager manager;
    SoftParticlesDrawer drawer(*world, manager);
    drawer.SetEnabled(false);

    FrameData fd;
    if (drawer.IsEnabled())
        drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.TotalCount(), 0);
}

TEST(SoftBody2DVisualDebugger_ParticlesDrawer, LayerName_IsSoftParticles)
{
    auto world = MakeRopeWorld();
    Dia::Debug::DebugLayerManager manager;
    SoftParticlesDrawer drawer(*world, manager);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kSoftParticles);
}

// ===========================================================================
// SoftConstraintsDrawer
// ===========================================================================

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, Draw_RopeConstraint_IsActive)
{
    auto world = MakeRopeWorld(3);  // Rope constraints are kRope type

    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(cap.prims[0].line2D.colour, Dia::Debug::DebugColourPalette::kActive);
}

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, Draw_StructuralConstraint_IsActive)
{
    auto world = MakeClothWorld(false);

    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    bool foundActive = false;
    for (int i = 0; i < cap.count; ++i)
    {
        if (cap.prims[i].type == DebugPrimitiveType::Line2D &&
            cap.prims[i].line2D.colour == Dia::Debug::DebugColourPalette::kActive)
        {
            foundActive = true;
            break;
        }
    }
    EXPECT_TRUE(foundActive) << "Expected at least one structural (kActive) constraint line";
}

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, Draw_ShearConstraint_IsGoal)
{
    auto world = MakeClothWorld(false);

    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    bool foundGoal = false;
    for (int i = 0; i < cap.count; ++i)
    {
        if (cap.prims[i].type == DebugPrimitiveType::Line2D &&
            cap.prims[i].line2D.colour == Dia::Debug::DebugColourPalette::kGoal)
        {
            foundGoal = true;
            break;
        }
    }
    EXPECT_TRUE(foundGoal) << "Expected at least one shear (kGoal) constraint line";
}

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, Draw_BendConstraint_IsInactive)
{
    // A 3x2 cloth produces bend constraints (skip-one horizontal)
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    ClothDef def;
    def.id            = Dia::Core::StringCRC("cloth");
    def.origin        = Vector2D(0.0f, 0.0f);
    def.width         = 2.0f;
    def.height        = 0.5f;
    def.resX          = 3;
    def.resY          = 2;
    def.mass          = 1.0f;
    def.particleRadius = 0.05f;
    def.pinTopRow     = false;
    world->AddCloth(def);

    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    bool foundInactive = false;
    for (int i = 0; i < cap.count; ++i)
    {
        if (cap.prims[i].type == DebugPrimitiveType::Line2D &&
            cap.prims[i].line2D.colour == Dia::Debug::DebugColourPalette::kInactive)
        {
            foundInactive = true;
            break;
        }
    }
    EXPECT_TRUE(foundInactive) << "Expected at least one bend (kInactive) constraint line";
}

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, Draw_TornConstraint_Skipped)
{
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(2.0f, 0.0f);
    def.particleCount = 3;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.maxStretch    = 0.0f;
    Rope* rope = world->AddRope(def);

    // Deactivate all constraints
    for (int i = 0; i < rope->GetConstraintCount(); ++i)
        rope->GetConstraint(i).active = false;

    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 0);
}

TEST(SoftBody2DVisualDebugger_ConstraintsDrawer, LayerName_IsSoftConstraints)
{
    auto world = MakeRopeWorld();
    Dia::Debug::DebugLayerManager manager;
    SoftConstraintsDrawer drawer(*world, manager);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kSoftConstraints);
}

// ===========================================================================
// SoftAnchorLinksDrawer
// ===========================================================================

TEST(SoftBody2DVisualDebugger_AnchorLinksDrawer, Draw_RopeWithTwoAnchors_TwoLines)
{
    std::unique_ptr<Dia::RigidBody2D::PhysicsWorld> physWorld(
        Dia::RigidBody2D::Testing::PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::RigidBody2D::Testing::BodyFactory bodyFactory(*physWorld);
    Dia::RigidBody2D::Body2DBase* startBody = bodyFactory.MakeStatic(-2.0f, 0.0f);
    Dia::RigidBody2D::Body2DBase* endBody   = bodyFactory.MakeStatic( 2.0f, 0.0f);

    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(-2.0f, 0.0f);
    def.endPoint      = Vector2D( 2.0f, 0.0f);
    def.particleCount = 5;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = startBody;
    def.endAnchor     = endBody;
    world->AddRope(def);

    Dia::Debug::DebugLayerManager manager;
    SoftAnchorLinksDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 2);
}

TEST(SoftBody2DVisualDebugger_AnchorLinksDrawer, Draw_NullAnchor_Skipped)
{
    std::unique_ptr<Dia::RigidBody2D::PhysicsWorld> physWorld(
        Dia::RigidBody2D::Testing::PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::RigidBody2D::Testing::BodyFactory bodyFactory(*physWorld);
    Dia::RigidBody2D::Body2DBase* startBody = bodyFactory.MakeStatic(-2.0f, 0.0f);

    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(-2.0f, 0.0f);
    def.endPoint      = Vector2D( 2.0f, 0.0f);
    def.particleCount = 5;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = startBody;
    def.endAnchor     = nullptr;
    world->AddRope(def);

    Dia::Debug::DebugLayerManager manager;
    SoftAnchorLinksDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 1);
}

TEST(SoftBody2DVisualDebugger_AnchorLinksDrawer, Draw_Colour_IsWarning)
{
    std::unique_ptr<Dia::RigidBody2D::PhysicsWorld> physWorld(
        Dia::RigidBody2D::Testing::PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::RigidBody2D::Testing::BodyFactory bodyFactory(*physWorld);
    Dia::RigidBody2D::Body2DBase* startBody = bodyFactory.MakeStatic(-2.0f, 0.0f);

    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(-2.0f, 0.0f);
    def.endPoint      = Vector2D( 2.0f, 0.0f);
    def.particleCount = 5;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = startBody;
    def.endAnchor     = nullptr;
    world->AddRope(def);

    Dia::Debug::DebugLayerManager manager;
    SoftAnchorLinksDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(cap.prims[0].line2D.colour, Dia::Debug::DebugColourPalette::kWarning);
}

TEST(SoftBody2DVisualDebugger_AnchorLinksDrawer, LayerName_IsSoftAnchors)
{
    auto world = MakeRopeWorld();
    Dia::Debug::DebugLayerManager manager;
    SoftAnchorLinksDrawer drawer(*world, manager);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kSoftAnchors);
}

// ===========================================================================
// SoftVelocityDrawer
// ===========================================================================

TEST(SoftBody2DVisualDebugger_VelocityDrawer, Draw_MovingParticle_DrawsLine)
{
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(2.0f, 0.0f);
    def.particleCount = 3;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    Rope* rope = world->AddRope(def);

    // Give particle 1 some Verlet velocity (delta > epsilon)
    rope->GetParticle(1).prevPosition = rope->GetParticle(1).position - Vector2D(0.5f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    SoftVelocityDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.LineCount(), 1);
}

TEST(SoftBody2DVisualDebugger_VelocityDrawer, Draw_StillParticle_Skipped)
{
    auto world = MakeRopeWorld(3);  // All particles at rest (prevPosition == position after construction)

    Dia::Debug::DebugLayerManager manager;
    SoftVelocityDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 0);
}

TEST(SoftBody2DVisualDebugger_VelocityDrawer, Draw_PinnedParticle_Skipped)
{
    // 2-particle rope with start anchor (particle 0 pinned, invMass == 0)
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(1.0f, 0.0f);
    def.particleCount = 2;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    def.startAnchor   = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0x1);
    def.endAnchor     = reinterpret_cast<Dia::RigidBody2D::Body2DBase*>(0x2);  // Both pinned
    Rope* rope = world->AddRope(def);

    // Give both particles a non-zero delta — but both are pinned so neither should draw
    rope->GetParticle(0).prevPosition = rope->GetParticle(0).position - Vector2D(1.0f, 0.0f);
    rope->GetParticle(1).prevPosition = rope->GetParticle(1).position - Vector2D(1.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    SoftVelocityDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 0);
}

TEST(SoftBody2DVisualDebugger_VelocityDrawer, Draw_Colour_IsHealthy)
{
    std::unique_ptr<SoftBodyWorld> world(SoftBodyWorldBuilder().WithNoGravity().Build());
    RopeDef def;
    def.id            = Dia::Core::StringCRC("rope");
    def.startPoint    = Vector2D(0.0f, 0.0f);
    def.endPoint      = Vector2D(2.0f, 0.0f);
    def.particleCount = 3;
    def.mass          = 1.0f;
    def.stiffness     = 1.0f;
    def.particleRadius = 0.1f;
    Rope* rope = world->AddRope(def);

    // Give particle 1 some Verlet velocity
    rope->GetParticle(1).prevPosition = rope->GetParticle(1).position - Vector2D(0.3f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    SoftVelocityDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    PrimitiveCapture cap;
    DoCapture(fd, cap);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(cap.prims[0].line2D.colour, Dia::Debug::DebugColourPalette::kHealthy);
}

TEST(SoftBody2DVisualDebugger_VelocityDrawer, LayerName_IsSoftVelocity)
{
    auto world = MakeRopeWorld();
    Dia::Debug::DebugLayerManager manager;
    SoftVelocityDrawer drawer(*world, manager);
    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kSoftVelocity);
}

#endif // DIA_DEBUG
