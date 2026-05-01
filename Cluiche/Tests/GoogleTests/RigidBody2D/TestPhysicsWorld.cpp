#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

static PhysicsWorld* MakeWorld(float timestep = 1.0f / 60.0f, int maxSubSteps = 8)
{
    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = timestep;
    def.maxSubSteps   = maxSubSteps;
    def.broadPhase    = nullptr;
    return new PhysicsWorld(def);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PhysicsWorld, Construction_GravityAndTimestep)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    EXPECT_FLOAT_EQ(world->GetGravity().x, 0.0f);
    EXPECT_FLOAT_EQ(world->GetGravity().y, -9.81f);
    EXPECT_FLOAT_EQ(world->GetFixedTimestep(), 1.0f / 60.0f);
}

// ---------------------------------------------------------------------------
// Body management
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PhysicsWorld, AddPointBody_CountIncrements)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    EXPECT_EQ(world->GetPointBodyCount(), 0);

    PointBodyDef def;
    def.id   = Dia::Core::StringCRC("A");
    def.type = BodyType::kDynamic;
    PointBody2D* body = world->AddPointBody(def);

    EXPECT_NE(body, nullptr);
    EXPECT_EQ(world->GetPointBodyCount(), 1);
}

TEST(RigidBody2D_PhysicsWorld, AddRigidBody_CountIncrements)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    EXPECT_EQ(world->GetRigidBodyCount(), 0);

    RigidBodyDef def;
    def.id   = Dia::Core::StringCRC("B");
    def.type = BodyType::kDynamic;
    RigidBody2D* body = world->AddRigidBody(def);

    EXPECT_NE(body, nullptr);
    EXPECT_EQ(world->GetRigidBodyCount(), 1);
}

TEST(RigidBody2D_PhysicsWorld, RemovePointBody_CountDecrements)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    PointBodyDef def;
    def.id = Dia::Core::StringCRC("P");
    PointBody2D* body = world->AddPointBody(def);
    EXPECT_EQ(world->GetPointBodyCount(), 1);

    world->RemovePointBody(body);
    EXPECT_EQ(world->GetPointBodyCount(), 0);
}

TEST(RigidBody2D_PhysicsWorld, RemoveRigidBody_CountDecrements)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    RigidBodyDef def;
    def.id = Dia::Core::StringCRC("R");
    RigidBody2D* body = world->AddRigidBody(def);
    EXPECT_EQ(world->GetRigidBodyCount(), 1);

    world->RemoveRigidBody(body);
    EXPECT_EQ(world->GetRigidBodyCount(), 0);
}

// ---------------------------------------------------------------------------
// Accumulator / step counting
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PhysicsWorld, Update_LessThanTimestep_NoStep)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld(1.0f / 60.0f));
    world->Update(0.001f);
    EXPECT_EQ(world->GetStepCount(), 0);
}

TEST(RigidBody2D_PhysicsWorld, Update_ExactlyOneTimestep_OneStep)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld(1.0f / 60.0f));
    world->Update(1.0f / 60.0f);
    EXPECT_EQ(world->GetStepCount(), 1);
}

TEST(RigidBody2D_PhysicsWorld, Update_ThreeTimesteps_ThreeSteps)
{
    // Use a round timestep to avoid FP accumulation edge cases
    std::unique_ptr<PhysicsWorld> world(MakeWorld(0.1f));
    world->Update(0.35f);  // 3 full steps of 0.1, 0.05 remainder
    EXPECT_EQ(world->GetStepCount(), 3);
}

TEST(RigidBody2D_PhysicsWorld, Update_LargeSpike_CappedAtMaxSubSteps)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld(1.0f / 60.0f, 8));
    world->Update(10.0f);  // Would be 600 steps without cap
    EXPECT_EQ(world->GetStepCount(), 8);
}

TEST(RigidBody2D_PhysicsWorld, Update_AccumulatesAcrossFrames)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld(1.0f / 60.0f));
    world->Update(0.5f / 60.0f);
    EXPECT_EQ(world->GetStepCount(), 0);
    world->Update(0.6f / 60.0f);  // total = 1.1/60 → 1 step
    EXPECT_EQ(world->GetStepCount(), 1);
}

// ---------------------------------------------------------------------------
// Gravity change
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PhysicsWorld, SetGravity_TakesEffect)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    world->SetGravity(Vector2D(0.0f, -20.0f));
    EXPECT_FLOAT_EQ(world->GetGravity().y, -20.0f);
}

TEST(RigidBody2D_PhysicsWorld, SetGravity_AffectsNextStep)
{
    std::unique_ptr<PhysicsWorld> world(MakeWorld(1.0f / 60.0f));
    Dia::Geometry2D::Transform t;
    PointBodyDef def;
    def.type      = BodyType::kDynamic;
    def.mass      = 1.0f;
    def.transform = &t;
    PointBody2D* body = world->AddPointBody(def);

    world->SetGravity(Vector2D(0.0f, 0.0f));  // Zero gravity
    world->Update(1.0f / 60.0f);

    EXPECT_FLOAT_EQ(body->GetTransform()->GetLocalPosition().y, 0.0f);
}

// ---------------------------------------------------------------------------
// CollisionEvents observer
// ---------------------------------------------------------------------------

TEST(RigidBody2D_PhysicsWorld, GetCollisionEvents_CanAttachObserver)
{
    class DummyObserver : public Dia::Core::Observer {
    public:
        void ObserverNotification(const Dia::Core::ObserverSubject*, int) override {}
    };

    std::unique_ptr<PhysicsWorld> world(MakeWorld());
    DummyObserver obs;
    world->GetCollisionEvents().AttachToObserver(&obs);
    world->GetCollisionEvents().DetachFromObserver(&obs);
    // No crash = pass
}
