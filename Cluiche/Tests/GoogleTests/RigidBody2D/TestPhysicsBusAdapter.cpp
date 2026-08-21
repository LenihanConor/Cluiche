// TestPhysicsBusAdapter.cpp
//
// PhysicsBusAdapter bridges PhysicsWorld's collision-event ObserverSubject
// onto Dia::MessageBus::Bus as snapshot-safe Messages::PhysicsCollisionEvent
// values (identity via Body2DBase::GetUniqueId()/GetId(), never a raw
// Body2DBase*). Covers: real collision -> real notification -> buffered ->
// Flush -> Bus delivery, same-tick delivery, and ledger coverage.

#include <gtest/gtest.h>

#include <DiaRigidBody2D/PhysicsBusAdapter.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMessageBus/Bus.h>

#include <vector>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

namespace {

// Broad-phase backed by a SpatialGrid covering a 200x200 world — mirrors
// Cluiche/Tests/GoogleTests/RigidBody2D/TestCollisionDetection.cpp's WorldFixture.
using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

struct WorldFixture {
    Grid*         grid  = nullptr;
    PhysicsWorld* world = nullptr;

    WorldFixture()
    {
        Grid::Def gd;
        gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
        gd.cellSize    = 10.0f;
        grid = new Grid(gd);

        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = 1.0f / 60.0f;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = grid;
        world = new PhysicsWorld(wd);
    }

    ~WorldFixture() { delete world; delete grid; }

    PointBody2D* AddCircle(Dia::Geometry2D::Transform* t, const Dia::Geometry2D::Circle* circle)
    {
        PointBodyDef def;
        def.transform   = t;
        def.circleShape = circle;
        def.type        = BodyType::kDynamic;
        def.mass        = 1.0f;
        return world->AddPointBody(def);
    }
};

} // namespace

// ---------------------------------------------------------------------------
// Real collision -> ObserverNotification -> buffered -> Flush -> Bus, same tick
// ---------------------------------------------------------------------------

TEST(PhysicsBusAdapter, RealCollision_DeliversPhysicsCollisionEvent_SameTick)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(1.5f, 0.0f));
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    PointBody2D* bodyA = f.AddCircle(&tA, &circA);
    PointBody2D* bodyB = f.AddCircle(&tB, &circB);
    ASSERT_NE(bodyA, nullptr);
    ASSERT_NE(bodyB, nullptr);

    Dia::MessageBus::Bus bus;
    bus.Initialize();

    PhysicsBusAdapter adapter(*f.world, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::PhysicsCollisionEvent> received;
    auto handle = bus.Subscribe<Messages::PhysicsCollisionEvent>(
        Dia::Core::StringCRC("TestPhysicsBusAdapter"),
        [&received](const Messages::PhysicsCollisionEvent& e) { received.push_back(e); });

    // Real physics step — narrow-phase produces a contact, EmitCollisionEvents
    // notifies the adapter's ObserverNotification synchronously.
    f.world->Update(1.0f / 60.0f);

    // Bus tick: pre-Primary Flush() drains the adapter's buffer, Primary pass
    // delivers to the subscriber above — all within this single Update().
    bus.Update();

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].type, CollisionEventType::kEnter);

    const uint32_t uidA = bodyA->GetUniqueId();
    const uint32_t uidB = bodyB->GetUniqueId();
    EXPECT_TRUE((received[0].bodyAUniqueId == uidA && received[0].bodyBUniqueId == uidB) ||
                (received[0].bodyAUniqueId == uidB && received[0].bodyBUniqueId == uidA));
    EXPECT_NE(received[0].bodyAUniqueId, received[0].bodyBUniqueId);
}

// ---------------------------------------------------------------------------
// Continued overlap across ticks -> kStay, not a second kEnter
// ---------------------------------------------------------------------------

TEST(PhysicsBusAdapter, ContinuedOverlap_SecondTick_DeliversStay)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(1.5f, 0.0f));
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());
    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    PhysicsBusAdapter adapter(*f.world, bus);
    bus.RegisterFlushAdapter(&adapter);

    std::vector<Messages::PhysicsCollisionEvent> received;
    auto handle = bus.Subscribe<Messages::PhysicsCollisionEvent>(
        Dia::Core::StringCRC("TestPhysicsBusAdapter"),
        [&received](const Messages::PhysicsCollisionEvent& e) { received.push_back(e); });

    f.world->Update(1.0f / 60.0f); // Enter
    bus.Update();
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].type, CollisionEventType::kEnter);
    received.clear();

    f.world->Update(1.0f / 60.0f); // still overlapping -> Stay
    bus.Update();
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0].type, CollisionEventType::kStay);
}

// ---------------------------------------------------------------------------
// GetLastTickLedger reflects the adapter's broadcasted message
// ---------------------------------------------------------------------------

TEST(PhysicsBusAdapter, RealCollision_LedgerReflectsBroadcast)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(1.5f, 0.0f));
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());
    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    PhysicsBusAdapter adapter(*f.world, bus);
    bus.RegisterFlushAdapter(&adapter);

    auto handle = bus.Subscribe<Messages::PhysicsCollisionEvent>(
        Dia::Core::StringCRC("TestPhysicsBusAdapter"),
        [](const Messages::PhysicsCollisionEvent&) {});

    f.world->Update(1.0f / 60.0f);
    bus.Update();

    const auto& ledger = bus.GetLastTickLedger();
    bool found = false;
    for (unsigned int i = 0; i < ledger.entries.Size(); ++i)
    {
        if (ledger.entries[i].typeId == Messages::PhysicsCollisionEvent::kTypeId)
        {
            found = true;
            EXPECT_GE(ledger.entries[i].count, 1u);
            EXPECT_GE(ledger.entries[i].deliveries, 1u);
        }
    }
    EXPECT_TRUE(found);
}

// ---------------------------------------------------------------------------
// No collision -> adapter forwards nothing
// ---------------------------------------------------------------------------

TEST(PhysicsBusAdapter, NoCollision_NoMessageDelivered)
{
    WorldFixture f;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(50.0f, 50.0f)); // far apart — never overlap
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());
    f.AddCircle(&tA, &circA);
    f.AddCircle(&tB, &circB);

    Dia::MessageBus::Bus bus;
    bus.Initialize();
    PhysicsBusAdapter adapter(*f.world, bus);
    bus.RegisterFlushAdapter(&adapter);

    int count = 0;
    auto handle = bus.Subscribe<Messages::PhysicsCollisionEvent>(
        Dia::Core::StringCRC("TestPhysicsBusAdapter"),
        [&count](const Messages::PhysicsCollisionEvent&) { ++count; });

    f.world->Update(1.0f / 60.0f);
    bus.Update();

    EXPECT_EQ(count, 0);
}
