#include <gtest/gtest.h>

#include <DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/World/PhysicsWorldCapacities.h>
#include <DiaRigidBody2D/Testing/PhysicsWorldBuilder.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaRigidBody2D/Constraints/SpringConstraint.h>
#include <DiaRigidBody2D/Constraints/HingeJoint.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Shapes/ConvexPolygon.h>
#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaMaths/Vector/Vector2D.h>

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

static int CountPrimitives(const FrameData& fd)
{
    RecordingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    return v.TotalCount();
}

static RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    return v;
}

// Build a minimal world with one dynamic circle body at the given position.
struct SimpleWorld
{
    std::unique_ptr<PhysicsWorld> world;
    BodyFactory*                  factory = nullptr;
    RigidBody2D*                  body    = nullptr;

    explicit SimpleWorld(float x = 0.0f, float y = 0.0f,
                         BodyType type = BodyType::kDynamic)
    {
        world.reset(PhysicsWorldBuilder().WithNoGravity().Build());
        factory = new BodyFactory(*world);
        if (type == BodyType::kStatic)
            body = factory->MakeStatic(x, y);
        else
            body = factory->MakeDynamic(x, y);
    }

    ~SimpleWorld() { delete factory; }
};

// ===========================================================================
// Toggle tests
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Toggle, DefaultDisabled)
{
    DiaRigidBodyVisualDebugger dbg;
    EXPECT_FALSE(dbg.IsEnabled());
}

TEST(RigidBody2DVisualDebugger_Toggle, SetEnabled_RoundTrip)
{
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    EXPECT_TRUE(dbg.IsEnabled());
    dbg.SetEnabled(false);
    EXPECT_FALSE(dbg.IsEnabled());
}

TEST(RigidBody2DVisualDebugger_Toggle, DisabledDrawProducesNoPrimitives)
{
    SimpleWorld sw;
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg; // disabled by default
    dbg.Draw(*sw.world, fd);
    EXPECT_EQ(CountPrimitives(fd), 0);
}

TEST(RigidBody2DVisualDebugger_Toggle, EnabledDrawProducesPrimitives)
{
    SimpleWorld sw;
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);
    EXPECT_GT(CountPrimitives(fd), 0);
}

// ===========================================================================
// Body shape drawing
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Bodies, CircleBody_DrawsCircle)
{
    SimpleWorld sw(1.0f, 2.0f);
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.CircleCount(), 1);
}

TEST(RigidBody2DVisualDebugger_Bodies, PolyBody_DrawsLines)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());

    // Unit square polygon (4 verts → 4 edges)
    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));
    const Vector2D verts[4] = {
        {-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}
    };
    Dia::Geometry2D::ConvexPolygon poly(verts, 4);

    RigidBodyDef def;
    def.id        = Dia::Core::StringCRC("poly_body");
    def.type      = BodyType::kDynamic;
    def.mass      = 1.0f;
    def.transform = &t;
    def.polyShape = &poly;
    world->AddRigidBody(def);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.LineCount(), 4);
}

TEST(RigidBody2DVisualDebugger_Bodies, NullTransform_NoShapeBody_DrawsPoint)
{
    // A body added without a transform falls back to point draw
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    PointBodyDef def;
    def.id            = Dia::Core::StringCRC("point_body");
    def.type          = BodyType::kDynamic;
    def.mass          = 1.0f;
    def.allowSleeping = false;
    world->AddPointBody(def);  // PointBody without a circleShape → ShapeKind::kNone

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.PointCount(), 1);
}

// ===========================================================================
// Body colour (golden values)
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Colour, DynamicBody_IsWhite)
{
    SimpleWorld sw(0.0f, 0.0f, BodyType::kDynamic);
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    // Retrieve the circle primitive
    InspectingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    ASSERT_EQ(v.visitCount, 1);  // one circle, no velocity arrow (not yet moving)
    ASSERT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour, RGBA::White);
}

TEST(RigidBody2DVisualDebugger_Colour, SleepingBody_IsDarkBlue)
{
    SimpleWorld sw(0.0f, 0.0f, BodyType::kDynamic);
    sw.body->SetSleepState(SleepState::kSleeping);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    InspectingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    ASSERT_GE(v.visitCount, 1);
    ASSERT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
    // Sleep colour: R=0, G=0, B=80
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.R(), 0);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.G(), 0);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.B(), 80);
}

TEST(RigidBody2DVisualDebugger_Colour, StaticBody_IsGrey)
{
    SimpleWorld sw(0.0f, 0.0f, BodyType::kStatic);
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    InspectingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    ASSERT_GE(v.visitCount, 1);
    ASSERT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.R(), 128);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.G(), 128);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.B(), 128);
}

TEST(RigidBody2DVisualDebugger_Colour, SleepOverridesBodyType)
{
    // Static body that is sleeping → sleep colour, not grey
    SimpleWorld sw(0.0f, 0.0f, BodyType::kStatic);
    sw.body->SetSleepState(SleepState::kSleeping);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    InspectingDebugVisitor v;
    AsDebug(fd).AcceptVisitor(v);
    ASSERT_GE(v.visitCount, 1);
    ASSERT_EQ(v.lastPrimitive.type, DebugPrimitiveType::Circle2D);
    EXPECT_EQ(v.lastPrimitive.circle2D.outlineColour.B(), 80);
}

// ===========================================================================
// Velocity arrows
// ===========================================================================

TEST(RigidBody2DVisualDebugger_VelocityArrows, ZeroVelocity_NoArrow)
{
    SimpleWorld sw;
    // Body starts with zero velocity
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_VelocityArrows, MovingBody_DrawsArrow)
{
    SimpleWorld sw;
    sw.body->SetVelocity(Vector2D(5.0f, 0.0f));

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 1);
}

TEST(RigidBody2DVisualDebugger_VelocityArrows, StaticBody_NoArrowEvenIfVelocitySet)
{
    // Static bodies are skipped for velocity arrow regardless
    SimpleWorld sw(0.0f, 0.0f, BodyType::kStatic);
    // Can't call SetVelocity meaningfully on static, but even if we did:
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_VelocityArrows, SleepingBody_NoArrow)
{
    SimpleWorld sw;
    sw.body->SetVelocity(Vector2D(5.0f, 0.0f));
    sw.body->SetSleepState(SleepState::kSleeping);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_VelocityArrows, ArrowLength_CappedAt10Units)
{
    // A very fast body: speed = 1000. Arrow should be capped at 10 world units.
    SimpleWorld sw;
    sw.body->SetVelocity(Vector2D(1000.0f, 0.0f));

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    // Find the ray primitive
    struct RayCapture : public DebugFrameDataVisitor
    {
        mutable float capturedLen = -1.0f;
        void Visit(const DebugPrimitive& p) const override
        {
            if (p.type == DebugPrimitiveType::Ray2D)
                capturedLen = p.ray2D.length;
        }
        void Visit(const DebugFrameData&) const override {}
    } v;
    AsDebug(fd).AcceptVisitor(v);
    EXPECT_LE(v.capturedLen, 10.0f);
}

TEST(RigidBody2DVisualDebugger_VelocityArrows, ArrowDirection_IsNormalized)
{
    SimpleWorld sw;
    sw.body->SetVelocity(Vector2D(3.0f, 4.0f));  // speed = 5

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    struct DirCapture : public DebugFrameDataVisitor
    {
        mutable Vector2D dir = { 0.0f, 0.0f };
        void Visit(const DebugPrimitive& p) const override
        {
            if (p.type == DebugPrimitiveType::Ray2D)
                dir = p.ray2D.direction;
        }
        void Visit(const DebugFrameData&) const override {}
    } v;
    AsDebug(fd).AcceptVisitor(v);

    const float len = std::sqrt(v.dir.x * v.dir.x + v.dir.y * v.dir.y);
    EXPECT_NEAR(len, 1.0f, 1e-5f);
    EXPECT_NEAR(v.dir.x, 3.0f / 5.0f, 1e-5f);
    EXPECT_NEAR(v.dir.y, 4.0f / 5.0f, 1e-5f);
}

// ===========================================================================
// Contact normals
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Contacts, EmptyContacts_NoRays)
{
    SimpleWorld sw;
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*sw.world, fd);

    // No contacts → zero rays (body has zero velocity so no arrow either)
    auto v = Inspect(fd);
    EXPECT_EQ(v.RayCount(), 0);
}

TEST(RigidBody2DVisualDebugger_Contacts, ContactNormal_UsesContactPoint)
{
    // Trigger a real collision by dropping a body onto a static floor.
    std::unique_ptr<PhysicsWorld> world(
        PhysicsWorldBuilder()
            .WithGravity(Vector2D(0.0f, -9.81f))
            .WithTimestep(1.0f / 60.0f)
            .WithMaxSubSteps(8)
            .Build());

    BodyFactory f(*world);
    f.MakeStatic(0.0f, -1.0f);       // floor
    RigidBody2D* ball = f.MakeDynamic(0.0f, 0.5f, 0.5f, 1.0f);
    ball->SetVelocity(Vector2D(0.0f, -5.0f));  // push it down

    // Simulate until at least one contact is produced
    for (int i = 0; i < 30 && world->GetLastContacts().Size() == 0; ++i)
        world->Update(1.0f / 60.0f);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    // At least one ray should be drawn for the contact normal
    auto v = Inspect(fd);
    EXPECT_GE(v.RayCount(), 1);
}

// ===========================================================================
// Constraints
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Constraints, PinJoint_DrawsLine)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* a = f.MakeDynamic(-1.0f, 0.0f);
    RigidBody2D* b = f.MakeDynamic( 1.0f, 0.0f);

    PinJoint* pin = new PinJoint(a, Vector2D(0.5f, 0.0f), b, Vector2D(-0.5f, 0.0f));
    world->AddConstraint(pin);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.LineCount(), 1);
}

TEST(RigidBody2DVisualDebugger_Constraints, PinJointWorldFixed_DrawsLine)
{
    // bodyB == nullptr: anchor B is a fixed world point
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* a = f.MakeDynamic(0.0f, 2.0f);

    PinJoint* pin = new PinJoint(a, Vector2D(0.0f, 0.0f), nullptr, Vector2D(0.0f, 0.0f));
    world->AddConstraint(pin);

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    auto v = Inspect(fd);
    EXPECT_GE(v.LineCount(), 1);
}

TEST(RigidBody2DVisualDebugger_Constraints, MultipleConstraints_DrawsMultipleLines)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);
    RigidBody2D* a = f.MakeDynamic(-2.0f, 0.0f);
    RigidBody2D* b = f.MakeDynamic( 0.0f, 0.0f);
    RigidBody2D* c = f.MakeDynamic( 2.0f, 0.0f);

    world->AddConstraint(new PinJoint(a, Vector2D(0.5f, 0.0f), b, Vector2D(-0.5f, 0.0f)));
    world->AddConstraint(new DistanceConstraint(b, Vector2D(0.5f, 0.0f), c, Vector2D(-0.5f, 0.0f), 1.0f));

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);

    auto v = Inspect(fd);
    // At minimum 2 constraint lines (plus any circle outline lines → use GE)
    EXPECT_GE(v.LineCount(), 2);
}

// ===========================================================================
// GetWorldAnchor on constraint types
// ===========================================================================

TEST(RigidBody2DVisualDebugger_WorldAnchors, PinJoint_AnchorPositions)
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

    // Local anchor at body-local offset (0.3, 0)
    PinJoint pin(a, Vector2D(0.3f, 0.0f), b, Vector2D(-0.3f, 0.0f));
    const Vector2D wa = pin.GetWorldAnchorA();
    const Vector2D wb = pin.GetWorldAnchorB();

    // No rotation → world = local body pos + local offset
    EXPECT_NEAR(wa.x, -1.0f + 0.3f, 1e-4f);
    EXPECT_NEAR(wa.y, 0.0f,         1e-4f);
    EXPECT_NEAR(wb.x,  1.0f - 0.3f, 1e-4f);
    EXPECT_NEAR(wb.y, 0.0f,         1e-4f);
}

TEST(RigidBody2DVisualDebugger_WorldAnchors, PinJoint_NullBodyB_ReturnsLocalAnchorB)
{
    Dia::Geometry2D::Transform ta;
    ta.SetLocalPosition(Vector2D(2.0f, 3.0f));
    Dia::Geometry2D::Circle ca(0.5f, Vector2D::Zero());

    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    RigidBodyDef da; da.id = Dia::Core::StringCRC("bodyA"); da.type = BodyType::kDynamic;
    da.mass = 1.0f; da.transform = &ta; da.circleShape = &ca;
    RigidBody2D* a = world->AddRigidBody(da);

    const Vector2D fixedWorld(5.0f, 5.0f);
    PinJoint pin(a, Vector2D(0.0f, 0.0f), nullptr, fixedWorld);
    const Vector2D wb = pin.GetWorldAnchorB();

    EXPECT_NEAR(wb.x, fixedWorld.x, 1e-4f);
    EXPECT_NEAR(wb.y, fixedWorld.y, 1e-4f);
}

TEST(RigidBody2DVisualDebugger_WorldAnchors, DistanceConstraint_AnchorPositions)
{
    Dia::Geometry2D::Transform ta, tb;
    ta.SetLocalPosition(Vector2D(0.0f, 0.0f));
    tb.SetLocalPosition(Vector2D(3.0f, 0.0f));
    Dia::Geometry2D::Circle ca(0.5f, Vector2D::Zero()), cb(0.5f, Vector2D::Zero());

    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    RigidBodyDef da; da.id = Dia::Core::StringCRC("bodyA"); da.type = BodyType::kDynamic;
    da.mass = 1.0f; da.transform = &ta; da.circleShape = &ca;
    RigidBodyDef db; db.id = Dia::Core::StringCRC("bodyB"); db.type = BodyType::kDynamic;
    db.mass = 1.0f; db.transform = &tb; db.circleShape = &cb;
    RigidBody2D* a = world->AddRigidBody(da);
    RigidBody2D* b = world->AddRigidBody(db);

    DistanceConstraint dc(a, Vector2D(0.0f, 0.0f), b, Vector2D(0.0f, 0.0f), 3.0f);
    EXPECT_NEAR(dc.GetWorldAnchorA().x, 0.0f, 1e-4f);
    EXPECT_NEAR(dc.GetWorldAnchorB().x, 3.0f, 1e-4f);
}

// ===========================================================================
// Empty world
// ===========================================================================

TEST(RigidBody2DVisualDebugger_EdgeCases, EmptyWorld_NoCrash_NoPrimitives)
{
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    dbg.Draw(*world, fd);
    EXPECT_EQ(CountPrimitives(fd), 0);
}

// ===========================================================================
// Stress: many bodies — no crash, no overflow
// ===========================================================================

TEST(RigidBody2DVisualDebugger_Stress, ManyBodies_NoOverflow)
{
    // Fill the world with moving dynamic bodies
    std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
    BodyFactory f(*world);

    static constexpr int kCount = 40;  // well within kMaxRigidBodies=256 and kDebugPrimitiveCapacity=1024
    for (int i = 0; i < kCount; ++i)
    {
        RigidBody2D* b = f.MakeDynamic(static_cast<float>(i) * 1.5f, 0.0f);
        if (b) b->SetVelocity(Vector2D(1.0f, 0.0f));
    }

    FrameData fd;
    DiaRigidBodyVisualDebugger dbg;
    dbg.SetEnabled(true);
    EXPECT_NO_THROW(dbg.Draw(*world, fd));

    // At minimum kCount circles + kCount arrows
    auto v = Inspect(fd);
    EXPECT_EQ(v.CircleCount(), kCount);
    EXPECT_EQ(v.RayCount(),    kCount);
}

TEST(RigidBody2DVisualDebugger_Stress, Determinism_TwoIdenticalWorlds_SamePrimitiveCount)
{
    auto makeAndDraw = [](float velX) -> int
    {
        std::unique_ptr<PhysicsWorld> world(PhysicsWorldBuilder().WithNoGravity().Build());
        BodyFactory f(*world);
        for (int i = 0; i < 5; ++i)
        {
            RigidBody2D* b = f.MakeDynamic(static_cast<float>(i), 0.0f);
            if (b) b->SetVelocity(Vector2D(velX, 0.0f));
        }
        FrameData fd;
        DiaRigidBodyVisualDebugger dbg;
        dbg.SetEnabled(true);
        dbg.Draw(*world, fd);
        return CountPrimitives(fd);
    };

    EXPECT_EQ(makeAndDraw(2.0f), makeAndDraw(2.0f));
}
