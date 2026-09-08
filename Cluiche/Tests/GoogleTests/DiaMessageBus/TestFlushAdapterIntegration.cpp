// TestFlushAdapterIntegration.cpp
//
// Integration coverage for the flush-adapters feature: PhysicsBusAdapter
// (DiaRigidBody2D) and InputBusAdapter (DiaInput) registered together on one
// Dia::MessageBus::Bus. Confirms:
//   - both adapters are Flush()-ed in registration order (pre-Primary)
//   - messages posted by both adapters are delivered within the SAME
//     bus.Update() tick (no one-tick delay)
//   - GetLastTickLedger() reflects messages from both adapters

#include <gtest/gtest.h>

#include <DiaRigidBody2D/PhysicsBusAdapter.h>
#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <DiaInput/InputBusAdapter.h>
#include <DiaInput/InputSourceManager.h>

#include <DiaMessageBus/Bus.h>

#include "Fixtures/FakeInputSource.h"

#include <vector>

using namespace Dia::Maths;

namespace {

using Grid = Dia::Geometry2D::SpatialGrid<Dia::RigidBody2D::Body2DBase*>;

// Wrap the REAL adapters to record Flush() call order without changing their
// behaviour — each override records, then delegates to the base Flush().
class OrderTrackingPhysicsAdapter : public Dia::RigidBody2D::PhysicsBusAdapter {
public:
    OrderTrackingPhysicsAdapter(Dia::RigidBody2D::PhysicsWorld& world, Dia::MessageBus::Bus& bus, std::vector<int>& order)
        : Dia::RigidBody2D::PhysicsBusAdapter(world, bus), mOrder(order) {}
    void Flush(Dia::MessageBus::Bus& bus) override {
        mOrder.push_back(1);
        Dia::RigidBody2D::PhysicsBusAdapter::Flush(bus);
    }
private:
    std::vector<int>& mOrder;
};

class OrderTrackingInputAdapter : public Dia::Input::InputBusAdapter {
public:
    OrderTrackingInputAdapter(Dia::Input::InputSourceManager& mgr, Dia::MessageBus::Bus& bus, std::vector<int>& order)
        : Dia::Input::InputBusAdapter(mgr, bus), mOrder(order) {}
    void Flush(Dia::MessageBus::Bus& bus) override {
        mOrder.push_back(2);
        Dia::Input::InputBusAdapter::Flush(bus);
    }
private:
    std::vector<int>& mOrder;
};

} // namespace

TEST(FlushAdapterIntegration, PhysicsAndInput_FlushedInRegistrationOrder_DeliveredSameTick_LedgerCoversBoth)
{
    // --- physics world with one real overlapping-body collision ---
    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-100.0f, -100.0f), Vector2D(100.0f, 100.0f));
    gd.cellSize    = 10.0f;
    Grid grid(gd);

    Dia::RigidBody2D::WorldDef wd;
    wd.gravity       = Vector2D(0.0f, 0.0f);
    wd.fixedTimestep = 1.0f / 60.0f;
    wd.maxSubSteps   = 1;
    wd.broadPhase    = &grid;
    Dia::RigidBody2D::PhysicsWorld world(wd);

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(1.5f, 0.0f));
    Dia::Geometry2D::Circle circA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle circB(1.0f, Vector2D::Zero());

    Dia::RigidBody2D::PointBodyDef defA; defA.transform = &tA; defA.circleShape = &circA; defA.type = Dia::RigidBody2D::BodyType::kDynamic; defA.mass = 1.0f;
    Dia::RigidBody2D::PointBodyDef defB; defB.transform = &tB; defB.circleShape = &circB; defB.type = Dia::RigidBody2D::BodyType::kDynamic; defB.mass = 1.0f;
    world.AddPointBody(defA);
    world.AddPointBody(defB);

    // --- input source with one queued key press ---
    Dia::Input::InputSourceManager inputMgr;
    TestFixtures::FakeInputSource  inputSrc;
    inputMgr.AddInputSource(&inputSrc);

    // --- bus + both adapters, registered physics-first ---
    Dia::MessageBus::Bus bus;
    bus.Initialize();

    std::vector<int> flushOrder;
    OrderTrackingPhysicsAdapter physicsAdapter(world, bus, flushOrder);
    OrderTrackingInputAdapter   inputAdapter(inputMgr, bus, flushOrder);
    bus.RegisterFlushAdapter(&physicsAdapter);
    bus.RegisterFlushAdapter(&inputAdapter);

    std::vector<Dia::RigidBody2D::Messages::PhysicsCollisionEvent> physicsReceived;
    auto physicsHandle = bus.Subscribe<Dia::RigidBody2D::Messages::PhysicsCollisionEvent>(
        Dia::Core::StringCRC("TestFlushAdapterIntegration_Physics"),
        [&physicsReceived](const Dia::RigidBody2D::Messages::PhysicsCollisionEvent& e) { physicsReceived.push_back(e); });

    std::vector<Dia::Input::Messages::KeyDownEvent> inputReceived;
    auto inputHandle = bus.Subscribe<Dia::Input::Messages::KeyDownEvent>(
        Dia::Core::StringCRC("TestFlushAdapterIntegration_Input"),
        [&inputReceived](const Dia::Input::Messages::KeyDownEvent& e) { inputReceived.push_back(e); });

    // Drive the physics step (produces a real Enter collision synchronously)
    // and queue an input event, then tick the bus exactly once.
    world.Update(1.0f / 60.0f);
    inputSrc.QueueKeyPress(11);

    bus.Update();

    // Registration order preserved: physics adapter (1) flushed before input adapter (2).
    ASSERT_EQ(flushOrder.size(), 2u);
    EXPECT_EQ(flushOrder[0], 1);
    EXPECT_EQ(flushOrder[1], 2);

    // Both messages delivered within this single tick — no one-tick delay.
    ASSERT_EQ(physicsReceived.size(), 1u);
    EXPECT_EQ(physicsReceived[0].type, Dia::RigidBody2D::CollisionEventType::kEnter);
    ASSERT_EQ(inputReceived.size(), 1u);
    EXPECT_EQ(inputReceived[0].code, 11);

    // Ledger reflects both types for this completed tick.
    const auto& ledger = bus.GetLastTickLedger();
    bool foundPhysics = false, foundInput = false;
    for (unsigned int i = 0; i < ledger.entries.Size(); ++i)
    {
        if (ledger.entries[i].typeId == Dia::RigidBody2D::Messages::PhysicsCollisionEvent::kTypeId)
        {
            foundPhysics = true;
            EXPECT_GE(ledger.entries[i].deliveries, 1u);
        }
        if (ledger.entries[i].typeId == Dia::Input::Messages::KeyDownEvent::kTypeId)
        {
            foundInput = true;
            EXPECT_GE(ledger.entries[i].deliveries, 1u);
        }
    }
    EXPECT_TRUE(foundPhysics);
    EXPECT_TRUE(foundInput);
}
