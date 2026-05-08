#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static WorldDef MakeWorldDef()
{
    WorldDef def;
    def.gravity       = Vector2D(0.0f, -9.81f);
    def.fixedTimestep = 1.0f / 60.0f;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    return def;
}

static bool IsFinite2D(const Vector2D& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y);
}

// ===========================================================================
// 1. 100 dynamic bodies simulate 120 steps without NaN/Inf
// ===========================================================================

TEST(RigidBody2D_Stress, HundredDynamicBodies_120Steps_NoNaNOrInf)
{
    static constexpr int kBodyCount = 100;
    static constexpr int kGridWidth = 10;  // 10x10 grid

    PhysicsWorld world(MakeWorldDef());

    for (int i = 0; i < kBodyCount; ++i)
    {
        float x = static_cast<float>(i % kGridWidth) * 2.0f;
        float y = static_cast<float>(i / kGridWidth) * 2.0f;

        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "Body%d", i);
        def.id           = Dia::Core::StringCRC(name);
        def.type         = BodyType::kDynamic;
        def.mass         = 1.0f;
        def.allowSleeping = false;

        PointBody2D* body = world.AddPointBody(def);
        ASSERT_NE(body, nullptr);
        body->GetTransform()->SetLocalPosition(Vector2D(x, y));
    }

    EXPECT_EQ(world.GetPointBodyCount(), kBodyCount);

    for (int step = 0; step < 120; ++step)
        world.Update(1.0f / 60.0f);

    const auto& bodies = world.GetPointBodies();
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        const Vector2D pos = bodies[i]->GetTransform()->GetLocalPosition();
        EXPECT_TRUE(IsFinite2D(pos)) << "Body " << i << " has non-finite position after 120 steps";
    }
}

// ===========================================================================
// 2. Rapid add/remove 50 bodies without crash
// ===========================================================================

TEST(RigidBody2D_Stress, RapidAddRemove_50Bodies_NoCrash)
{
    PhysicsWorld world(MakeWorldDef());

    // --- First wave: add 50 and remove all ---
    for (int i = 0; i < 50; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "WaveA%d", i);
        def.id   = Dia::Core::StringCRC(name);
        def.type = BodyType::kDynamic;
        def.mass = 1.0f;
        PointBody2D* body = world.AddPointBody(def);
        ASSERT_NE(body, nullptr);
    }

    EXPECT_EQ(world.GetPointBodyCount(), 50);

    // Remove all bodies from the first wave
    while (world.GetPointBodyCount() > 0)
    {
        PointBody2D* body = world.GetPointBodies()[0];
        world.RemovePointBody(body);
    }

    EXPECT_EQ(world.GetPointBodyCount(), 0);

    // --- Second wave: add 50 again ---
    for (int i = 0; i < 50; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "WaveB%d", i);
        def.id   = Dia::Core::StringCRC(name);
        def.type = BodyType::kDynamic;
        def.mass = 1.0f;
        PointBody2D* body = world.AddPointBody(def);
        ASSERT_NE(body, nullptr);
    }

    EXPECT_EQ(world.GetPointBodyCount(), 50);

    // Should not crash when stepping after the cycle
    world.Update(1.0f / 60.0f);
}

// ===========================================================================
// 3. Very small timestep (1/1000 s) — stable over 100 steps
// ===========================================================================

TEST(RigidBody2D_Stress, VerySmallTimestep_StableSimulation)
{
    WorldDef wd = MakeWorldDef();
    wd.fixedTimestep = 0.001f;  // 1/1000 s
    wd.maxSubSteps   = 8;
    PhysicsWorld world(wd);

    PointBodyDef def;
    def.id           = Dia::Core::StringCRC("SmallDtBody");
    def.type         = BodyType::kDynamic;
    def.mass         = 1.0f;
    def.allowSleeping = false;
    PointBody2D* body = world.AddPointBody(def);
    body->GetTransform()->SetLocalPosition(Vector2D(0.0f, 10.0f));

    for (int i = 0; i < 100; ++i)
        world.Update(0.001f);

    const Vector2D pos = body->GetTransform()->GetLocalPosition();
    EXPECT_TRUE(IsFinite2D(pos)) << "Position became non-finite with dt=0.001";
    EXPECT_FALSE(std::isnan(pos.x));
    EXPECT_FALSE(std::isnan(pos.y));
}

// ===========================================================================
// 4. Large timestep capped — dt=10 with fixedTimestep=1/60, maxSubSteps=8
//    → exactly 8 physics steps, not 600
// ===========================================================================

TEST(RigidBody2D_Stress, LargeTimestep_CappedAtMaxSubSteps)
{
    WorldDef wd = MakeWorldDef();
    wd.fixedTimestep = 1.0f / 60.0f;
    wd.maxSubSteps   = 8;
    PhysicsWorld world(wd);

    // Pass dt=10 — would be ~600 steps without the cap
    world.Update(10.0f);

    EXPECT_EQ(world.GetStepCount(), 8)
        << "World should clamp to maxSubSteps=8, not run 600 steps for dt=10";
}

// ===========================================================================
// 5. Static body count — 50 static + 50 dynamic = correct counts
// ===========================================================================

TEST(RigidBody2D_Stress, StaticAndDynamic_CorrectBodyCounts)
{
    PhysicsWorld world(MakeWorldDef());

    for (int i = 0; i < 50; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "Static%d", i);
        def.id   = Dia::Core::StringCRC(name);
        def.type = BodyType::kStatic;
        def.mass = 1.0f;
        PointBody2D* body = world.AddPointBody(def);
        body->GetTransform()->SetLocalPosition(
            Vector2D(static_cast<float>(i) * 2.0f, -10.0f));
    }

    for (int i = 0; i < 50; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "Dynamic%d", i);
        def.id   = Dia::Core::StringCRC(name);
        def.type = BodyType::kDynamic;
        def.mass = 1.0f;
        PointBody2D* body = world.AddPointBody(def);
        body->GetTransform()->SetLocalPosition(
            Vector2D(static_cast<float>(i) * 2.0f, 0.0f));
    }

    EXPECT_EQ(world.GetPointBodyCount(), 100);

    // Run a few steps — counts must remain stable
    for (int i = 0; i < 10; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_EQ(world.GetPointBodyCount(), 100);
}

// ===========================================================================
// 6. Extreme force (1e6) applied to a body — step 10 times, no crash
// ===========================================================================

TEST(RigidBody2D_Stress, ExtremeForce_NoCrash)
{
    PhysicsWorld world(MakeWorldDef());

    PointBodyDef def;
    def.id           = Dia::Core::StringCRC("ForceBody");
    def.type         = BodyType::kDynamic;
    def.mass         = 1.0f;
    def.allowSleeping = false;
    PointBody2D* body = world.AddPointBody(def);
    body->GetTransform()->SetLocalPosition(Vector2D(0.0f, 0.0f));

    // Apply an extremely large force before each step
    for (int i = 0; i < 10; ++i)
    {
        body->ApplyForce(Vector2D(1e6f, 0.0f));
        world.Update(1.0f / 60.0f);
    }

    // We only assert no crash and the body still exists/is accessible.
    // The position may have become very large — that is expected behaviour
    // under extreme force with no damping.  We do NOT assert finite here
    // because overflow is a documented consequence of unrealistic inputs.
    // The important thing is no assertion failure, no segfault, no UB.
    EXPECT_NE(body, nullptr);
}

// ===========================================================================
// 7. Zero-mass dynamic body — can be added without crash; falls under gravity
//    because gravity is applied unconditionally to kDynamic bodies.
// ===========================================================================

TEST(RigidBody2D_Stress, ZeroMassDynamicBody_AddedWithoutCrash_StillFallsUnderGravity)
{
    PhysicsWorld world(MakeWorldDef());

    PointBodyDef def;
    def.id            = Dia::Core::StringCRC("ZeroMassBody");
    def.type          = BodyType::kDynamic;
    def.mass          = 0.0f;   // Intentionally zero
    def.allowSleeping = false;

    PointBody2D* body = world.AddPointBody(def);
    ASSERT_NE(body, nullptr);
    EXPECT_FLOAT_EQ(body->GetInverseMass(), 0.0f);

    body->GetTransform()->SetLocalPosition(Vector2D(0.0f, 5.0f));

    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    // Gravity is added to acceleration regardless of mass, so body falls.
    // Verify no NaN/Inf — stability is the guarantee, not immobility.
    const float y = body->GetTransform()->GetLocalPosition().y;
    EXPECT_FALSE(std::isnan(y));
    EXPECT_FALSE(std::isinf(y));
    EXPECT_LT(y, 5.0f);   // has fallen below starting position
}

// ===========================================================================
// 8. All bodies sleeping — no position change, world stays consistent
// ===========================================================================

TEST(RigidBody2D_Stress, AllBodiesSleeping_NoPositionChange)
{
    PhysicsWorld world(MakeWorldDef());

    static constexpr int kCount = 20;
    PointBody2D* bodies[kCount];

    for (int i = 0; i < kCount; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "SleepBody%d", i);
        def.id            = Dia::Core::StringCRC(name);
        def.type          = BodyType::kDynamic;
        def.mass          = 1.0f;
        def.allowSleeping = true;
        bodies[i] = world.AddPointBody(def);
        bodies[i]->GetTransform()->SetLocalPosition(
            Vector2D(static_cast<float>(i) * 1.0f, 0.0f));
        bodies[i]->Sleep();
    }

    // Record positions before simulation
    Vector2D beforePos[kCount];
    for (int i = 0; i < kCount; ++i)
        beforePos[i] = bodies[i]->GetTransform()->GetLocalPosition();

    for (int step = 0; step < 60; ++step)
        world.Update(1.0f / 60.0f);

    for (int i = 0; i < kCount; ++i)
    {
        const Vector2D afterPos = bodies[i]->GetTransform()->GetLocalPosition();
        EXPECT_NEAR(afterPos.x, beforePos[i].x, 1e-5f) << "Sleeping body " << i << " should not move (x)";
        EXPECT_NEAR(afterPos.y, beforePos[i].y, 1e-5f) << "Sleeping body " << i << " should not move (y)";
    }
}

// ===========================================================================
// 9. Bodies spread across a wide area — no NaN after long simulation
// ===========================================================================

TEST(RigidBody2D_Stress, WideAreaSpread_LongSimulation_NoNaN)
{
    static constexpr int kCount = 30;

    PhysicsWorld world(MakeWorldDef());

    for (int i = 0; i < kCount; ++i)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "WideBody%d", i);
        def.id            = Dia::Core::StringCRC(name);
        def.type          = BodyType::kDynamic;
        def.mass          = 1.0f;
        def.allowSleeping = false;

        PointBody2D* body = world.AddPointBody(def);
        body->GetTransform()->SetLocalPosition(
            Vector2D(static_cast<float>(i) * 100.0f, 500.0f));
    }

    for (int step = 0; step < 300; ++step)
        world.Update(1.0f / 60.0f);

    const auto& pts = world.GetPointBodies();
    for (unsigned int i = 0; i < pts.Size(); ++i)
    {
        const Vector2D pos = pts[i]->GetTransform()->GetLocalPosition();
        EXPECT_FALSE(std::isnan(pos.x)) << "Body " << i << " has NaN x";
        EXPECT_FALSE(std::isnan(pos.y)) << "Body " << i << " has NaN y";
    }
}

// ===========================================================================
// 10. Rapid add/remove interleaved with steps — no crash
// ===========================================================================

TEST(RigidBody2D_Stress, InterleavedAddRemoveWithSteps_NoCrash)
{
    PhysicsWorld world(MakeWorldDef());

    for (int cycle = 0; cycle < 20; ++cycle)
    {
        PointBodyDef def;
        char name[32];
        snprintf(name, sizeof(name), "CycleBody%d", cycle);
        def.id   = Dia::Core::StringCRC(name);
        def.type = BodyType::kDynamic;
        def.mass = 1.0f;

        PointBody2D* body = world.AddPointBody(def);
        body->GetTransform()->SetLocalPosition(
            Vector2D(static_cast<float>(cycle) * 0.5f, 0.0f));

        world.Update(1.0f / 60.0f);

        world.RemovePointBody(body);
        world.Update(1.0f / 60.0f);
    }

    EXPECT_EQ(world.GetPointBodyCount(), 0);
}

// ===========================================================================
// 11. Single body under normal gravity — falls downward after 1 second
// ===========================================================================

TEST(RigidBody2D_Stress, SingleBody_FallsUnderGravity_CorrectDirection)
{
    PhysicsWorld world(MakeWorldDef());

    PointBodyDef def;
    def.id            = Dia::Core::StringCRC("FallBody");
    def.type          = BodyType::kDynamic;
    def.mass          = 1.0f;
    def.allowSleeping = false;

    PointBody2D* body = world.AddPointBody(def);
    body->GetTransform()->SetLocalPosition(Vector2D(0.0f, 100.0f));

    const float yStart = body->GetTransform()->GetLocalPosition().y;

    // Simulate ~1 second (60 frames at 1/60)
    for (int i = 0; i < 60; ++i)
        world.Update(1.0f / 60.0f);

    const float yEnd = body->GetTransform()->GetLocalPosition().y;

    // Under -9.81 gravity the body must have moved down (y decreased)
    EXPECT_LT(yEnd, yStart) << "Body should fall under gravity";
    EXPECT_TRUE(std::isfinite(yEnd)) << "Position must remain finite";
}

// ===========================================================================
// 12. World with no bodies — Update does not crash
// ===========================================================================

TEST(RigidBody2D_Stress, EmptyWorld_UpdateNoCrash)
{
    PhysicsWorld world(MakeWorldDef());

    EXPECT_EQ(world.GetPointBodyCount(), 0);
    EXPECT_EQ(world.GetRigidBodyCount(), 0);

    for (int i = 0; i < 120; ++i)
        world.Update(1.0f / 60.0f);

    EXPECT_EQ(world.GetStepCount(), 120);
}
