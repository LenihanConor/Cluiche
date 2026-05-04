#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h>
#include <DiaRigidBody2DVisualDebugger/PhysicsAABBDrawer.h>
#include <DiaRigidBody2DVisualDebugger/VelocityArrowsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ContactNormalsDrawer.h>
#include <DiaRigidBody2DVisualDebugger/ConstraintLinesDrawer.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/World/PhysicsWorldCapacities.h>
#include <DiaRigidBody2D/Testing/PhysicsWorldBuilder.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugColourPalette.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>

#include <DiaCore/CRC/StringCRC.h>
#include <cmath>
#include <memory>

using namespace Dia::RigidBody2D;
using namespace Dia::RigidBody2D::Testing;
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

// Captures all visited primitives (up to N) for inspecting specific ones
struct PrimitiveCapture : public DebugFrameDataVisitor
{
    static constexpr int kMax = 256;
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
    AsDebug(fd).AcceptVisitor(c);
    return c;
}

// ===========================================================================
// PhysicsShapesDrawer
// ===========================================================================

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_DynamicCircle_DrawsCircle)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeDynamic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.CircleCount(), 1);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_DynamicCircle_IsActive)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeDynamic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kActive);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_StaticBody_IsInactive)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeStatic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kInactive);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_KinematicBody_IsGoal)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));
    Dia::Geometry2D::Circle c(0.5f, Vector2D::Zero());

    RigidBodyDef def;
    def.id          = Dia::Core::StringCRC("kinematic_body");
    def.type        = BodyType::kKinematic;
    def.mass        = 1.0f;
    def.transform   = &t;
    def.circleShape = &c;
    world->AddRigidBody(def);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kGoal);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_SleepingBody_IsDeepSleep)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* body = f.MakeDynamic(0.0f, 0.0f);
    body->SetSleepState(SleepState::kSleeping);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(cap.prims[0].circle2D.outlineColour, Dia::Debug::DebugColourPalette::kDeepSleep);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, Draw_Disabled_NoPrimitives)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeDynamic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);
    drawer.SetEnabled(false);

    FrameData fd;
    // IVisualDebugger::Draw is called directly — the layer manager calls it only when enabled.
    // Here we manually guard the same way DebugLayerManager would.
    if (drawer.IsEnabled())
        drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.TotalCount(), 0);
}

TEST(RigidBody2DVisualDebugger_ShapesDrawer, LayerName_IsPhysicsShapes)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::Debug::DebugLayerManager manager;
    PhysicsShapesDrawer drawer(*world, manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kPhysicsShapes);
}

// ===========================================================================
// VelocityArrowsDrawer
// ===========================================================================

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, Draw_StaticBody_NoArrow)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeStatic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    VelocityArrowsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, Draw_SleepingBody_NoArrow)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* body = f.MakeDynamic(0.0f, 0.0f);
    body->SetVelocity(Vector2D(5.0f, 0.0f));
    body->SetSleepState(SleepState::kSleeping);

    Dia::Debug::DebugLayerManager manager;
    VelocityArrowsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, Draw_MovingBody_DrawsRay)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* body = f.MakeDynamic(0.0f, 0.0f);
    body->SetVelocity(Vector2D(5.0f, 0.0f));

    Dia::Debug::DebugLayerManager manager;
    VelocityArrowsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 1);
}

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, Draw_Colour_IsWarning)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* body = f.MakeDynamic(0.0f, 0.0f);
    body->SetVelocity(Vector2D(5.0f, 0.0f));

    Dia::Debug::DebugLayerManager manager;
    VelocityArrowsDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_EQ(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Ray2D);
    EXPECT_EQ(cap.prims[0].ray2D.colour, Dia::Debug::DebugColourPalette::kWarning);
}

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, Draw_Scale_AffectsLength)
{
    // Set up a body with known velocity to get a predictable arrow length
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* body = f.MakeDynamic(0.0f, 0.0f);
    body->SetVelocity(Vector2D(2.0f, 0.0f));  // speed=2, scale=0.1 → len=0.2

    Dia::Debug::DebugLayerManager manager1, manager2;
    manager1.SetDebugScale(1.0f);
    manager2.SetDebugScale(2.0f);

    VelocityArrowsDrawer drawer1(*world, manager1);
    VelocityArrowsDrawer drawer2(*world, manager2);

    FrameData fd1, fd2;
    drawer1.Draw(fd1);
    drawer2.Draw(fd2);

    auto cap1 = Capture(fd1);
    auto cap2 = Capture(fd2);

    ASSERT_EQ(cap1.count, 1);
    ASSERT_EQ(cap2.count, 1);
    ASSERT_EQ(cap1.prims[0].type, DebugPrimitiveType::Ray2D);
    ASSERT_EQ(cap2.prims[0].type, DebugPrimitiveType::Ray2D);

    const float len1 = cap1.prims[0].ray2D.length;
    const float len2 = cap2.prims[0].ray2D.length;
    EXPECT_NEAR(len2, len1 * 2.0f, 1e-4f);
}

TEST(RigidBody2DVisualDebugger_VelocityArrowsDrawer, LayerName_IsPhysicsVelocity)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::Debug::DebugLayerManager manager;
    VelocityArrowsDrawer drawer(*world, manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kPhysicsVelocity);
}

// ===========================================================================
// ContactNormalsDrawer
// ===========================================================================

// ---------------------------------------------------------------------------
// Contact test fixture: holds the world + grid + BodyFactory together so
// body shape/transform slots remain valid for the lifetime of the test.
// A SpatialGrid broadphase is required for collision detection to run.
// ---------------------------------------------------------------------------
struct ContactWorld
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

    std::unique_ptr<Grid>         grid;
    std::unique_ptr<PhysicsWorld> world;
    std::unique_ptr<BodyFactory>  factory;

    ContactWorld()
    {
        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        gd.cellSize    = 10.0f;
        grid.reset(new Grid(gd));

        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, -9.81f);
        wd.fixedTimestep = 1.0f / 60.0f;
        wd.maxSubSteps   = 8;
        wd.broadPhase    = grid.get();
        world.reset(new PhysicsWorld(wd));

        factory.reset(new BodyFactory(*world));

        // Floor center at y=0, radius=0.5 → top at y=0.5
        // Ball center at y=0.8, radius=0.5 → bottom at y=0.3 → overlaps immediately
        factory->MakeStatic(0.0f, 0.0f);
        RigidBody2D* ball = factory->MakeDynamic(0.0f, 0.8f, 0.5f, 1.0f);
        ball->SetVelocity(Vector2D(0.0f, -5.0f));

        // Single step is sufficient to generate a contact from the initial overlap
        world->Update(1.0f / 60.0f);
    }

    bool HasContacts() const { return world->GetLastContacts().Size() > 0; }
};

TEST(RigidBody2DVisualDebugger_ContactNormalsDrawer, Draw_TwoContacts_TwoRays)
{
    ContactWorld cw;
    ASSERT_TRUE(cw.HasContacts()) << "Precondition: simulation must produce at least 1 contact";
    const unsigned int contactCount = cw.world->GetLastContacts().Size();

    Dia::Debug::DebugLayerManager manager;
    ContactNormalsDrawer drawer(*cw.world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), static_cast<int>(contactCount));
}

TEST(RigidBody2DVisualDebugger_ContactNormalsDrawer, Draw_Colour_IsError)
{
    ContactWorld cw;
    ASSERT_TRUE(cw.HasContacts());

    Dia::Debug::DebugLayerManager manager;
    ContactNormalsDrawer drawer(*cw.world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Ray2D);
    EXPECT_EQ(cap.prims[0].ray2D.colour, Dia::Debug::DebugColourPalette::kError);
}

TEST(RigidBody2DVisualDebugger_ContactNormalsDrawer, Draw_Normal_MatchesContactNormal)
{
    ContactWorld cw;
    ASSERT_TRUE(cw.HasContacts());
    const Contact& contact = cw.world->GetLastContacts()[0];

    Dia::Debug::DebugLayerManager manager;
    ContactNormalsDrawer drawer(*cw.world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_GE(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Ray2D);

    // Ray direction must match contact normal
    EXPECT_NEAR(cap.prims[0].ray2D.direction.x, contact.normal.x, 1e-4f);
    EXPECT_NEAR(cap.prims[0].ray2D.direction.y, contact.normal.y, 1e-4f);
}

TEST(RigidBody2DVisualDebugger_ContactNormalsDrawer, LayerName_IsPhysicsContacts)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::Debug::DebugLayerManager manager;
    ContactNormalsDrawer drawer(*world, manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kPhysicsContacts);
}

// ===========================================================================
// ConstraintLinesDrawer
// ===========================================================================

TEST(RigidBody2DVisualDebugger_ConstraintLinesDrawer, Draw_TwoConstraints_TwoLines)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* a = f.MakeDynamic(-2.0f, 0.0f);
    RigidBody2D* b = f.MakeDynamic( 0.0f, 0.0f);
    RigidBody2D* c = f.MakeDynamic( 2.0f, 0.0f);

    world->AddConstraint(new PinJoint(a, Vector2D(0.5f, 0.0f), b, Vector2D(-0.5f, 0.0f)));
    world->AddConstraint(new DistanceConstraint(b, Vector2D(0.5f, 0.0f), c, Vector2D(-0.5f, 0.0f), 1.0f));

    Dia::Debug::DebugLayerManager manager;
    ConstraintLinesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 2);
}

TEST(RigidBody2DVisualDebugger_ConstraintLinesDrawer, Draw_Colour_IsGoal)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* a = f.MakeDynamic(-1.0f, 0.0f);
    RigidBody2D* b = f.MakeDynamic( 1.0f, 0.0f);

    world->AddConstraint(new PinJoint(a, Vector2D(0.5f, 0.0f), b, Vector2D(-0.5f, 0.0f)));

    Dia::Debug::DebugLayerManager manager;
    ConstraintLinesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_EQ(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_EQ(cap.prims[0].line2D.colour, Dia::Debug::DebugColourPalette::kGoal);
}

TEST(RigidBody2DVisualDebugger_ConstraintLinesDrawer, Draw_Endpoints_MatchAnchors)
{
    Dia::Geometry2D::Transform ta, tb;
    ta.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tb.SetLocalPosition(Vector2D( 1.0f, 0.0f));
    Dia::Geometry2D::Circle ca(0.5f, Vector2D::Zero()), cb(0.5f, Vector2D::Zero());

    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    RigidBodyDef da; da.id = Dia::Core::StringCRC("bodyA"); da.type = BodyType::kDynamic;
    da.mass = 1.0f; da.transform = &ta; da.circleShape = &ca;
    RigidBodyDef db; db.id = Dia::Core::StringCRC("bodyB"); db.type = BodyType::kDynamic;
    db.mass = 1.0f; db.transform = &tb; db.circleShape = &cb;
    RigidBody2D* a = world->AddRigidBody(da);
    RigidBody2D* b = world->AddRigidBody(db);

    PinJoint* pin = new PinJoint(a, Vector2D(0.0f, 0.0f), b, Vector2D(0.0f, 0.0f));
    const Vector2D expectedAnchorA = pin->GetWorldAnchorA();
    const Vector2D expectedAnchorB = pin->GetWorldAnchorB();
    world->AddConstraint(pin);

    Dia::Debug::DebugLayerManager manager;
    ConstraintLinesDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_EQ(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Line2D);
    EXPECT_NEAR(cap.prims[0].line2D.start.x, expectedAnchorA.x, 1e-4f);
    EXPECT_NEAR(cap.prims[0].line2D.start.y, expectedAnchorA.y, 1e-4f);
    EXPECT_NEAR(cap.prims[0].line2D.end.x,   expectedAnchorB.x, 1e-4f);
    EXPECT_NEAR(cap.prims[0].line2D.end.y,   expectedAnchorB.y, 1e-4f);
}

TEST(RigidBody2DVisualDebugger_ConstraintLinesDrawer, LayerName_IsPhysicsConstraints)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::Debug::DebugLayerManager manager;
    ConstraintLinesDrawer drawer(*world, manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kPhysicsConstraints);
}

// ===========================================================================
// PhysicsAABBDrawer
// ===========================================================================

TEST(RigidBody2DVisualDebugger_AABBDrawer, Draw_RigidBody_DrawsRect)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeDynamic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsAABBDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RectCount(), 1);
}

TEST(RigidBody2DVisualDebugger_AABBDrawer, Draw_Colour_IsWarning)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    f.MakeDynamic(0.0f, 0.0f);

    Dia::Debug::DebugLayerManager manager;
    PhysicsAABBDrawer drawer(*world, manager);

    FrameData fd;
    drawer.Draw(fd);

    auto cap = Capture(fd);
    ASSERT_EQ(cap.count, 1);
    ASSERT_EQ(cap.prims[0].type, DebugPrimitiveType::Rect2D);
    EXPECT_EQ(cap.prims[0].rect2D.outlineColour, Dia::Debug::DebugColourPalette::kWarning);
}

TEST(RigidBody2DVisualDebugger_AABBDrawer, LayerName_IsPhysicsAABB)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    Dia::Debug::DebugLayerManager manager;
    PhysicsAABBDrawer drawer(*world, manager);

    EXPECT_EQ(drawer.GetLayerName(), Dia::Debug::LayerNames::kPhysicsAABB);
}

// ===========================================================================
// Migration: all five drawers registered together
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Migration, AllFiveDrawers_Registered_DrawCorrectTotalPrimitives)
{
    // Build a world: 1 dynamic circle, 1 static circle, 1 constraint, no contacts
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* dyn = f.MakeDynamic(-1.0f, 0.0f);
    dyn->SetVelocity(Vector2D(3.0f, 0.0f));
    RigidBody2D* stat = f.MakeStatic(1.0f, 0.0f);
    world->AddConstraint(new PinJoint(dyn, Vector2D(0.0f, 0.0f), stat, Vector2D(0.0f, 0.0f)));

    Dia::Debug::DebugLayerManager manager;

    PhysicsShapesDrawer    shapesDrawer   (*world, manager);
    PhysicsAABBDrawer      aabbDrawer     (*world, manager);
    VelocityArrowsDrawer   velDrawer      (*world, manager);
    ContactNormalsDrawer   contactDrawer  (*world, manager);
    ConstraintLinesDrawer  constraintDrawer(*world, manager);

    manager.Register(&shapesDrawer,     10);
    manager.Register(&aabbDrawer,       15);
    manager.Register(&velDrawer,        20);
    manager.Register(&contactDrawer,    20);
    manager.Register(&constraintDrawer, 10);

    FrameData fd;
    manager.Draw(fd);

    auto v = Inspect(fd);
    // Shapes: 2 circles (dynamic + static)
    EXPECT_EQ(v.CircleCount(), 2);
    // AABB: 2 rects (both are rigid bodies)
    EXPECT_EQ(v.RectCount(), 2);
    // Velocity: 1 ray (dynamic body has velocity)
    EXPECT_EQ(v.RayCount(), 1);
    // Contacts: 0 (no simulation step)
    // Constraints: 1 line
    EXPECT_EQ(v.LineCount(), 1);
}

#endif // DIA_DEBUG
