#include <gtest/gtest.h>
#include <cmath>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaRigidBody2D/Constraints/SpringConstraint.h>
#include <DiaRigidBody2D/Constraints/HingeJoint.h>
#include <DiaRigidBody2D/Constraints/ConstraintSolver.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaGeometry2D/Spatial/SpatialGrid.h>
#include <DiaMaths/Vector/Vector2D.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// ===========================================================================
// Shared fixture — no gravity, no broad-phase (constraint/body-only tests)
// ===========================================================================

static constexpr float kDt = 1.0f / 60.0f;

struct RobustnessFixture {
    static constexpr int kMax = 16;

    Dia::Geometry2D::Transform transforms[kMax];
    Dia::Geometry2D::Circle    circles[kMax];
    Dia::Geometry2D::AARect    broads[kMax];

    PhysicsWorld* world = nullptr;

    RobustnessFixture()
    {
        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = kDt;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = nullptr;
        wd.constraintConfig.iterations = 20;
        world = new PhysicsWorld(wd);
    }

    ~RobustnessFixture() { delete world; }

    RigidBody2D* MakeBody(int idx, float x, float y,
                          BodyType type = BodyType::kDynamic, float mass = 1.0f)
    {
        transforms[idx] = Dia::Geometry2D::Transform();
        transforms[idx].SetWorldPosition(Vector2D(x, y));
        circles[idx] = Dia::Geometry2D::Circle(0.5f, Vector2D::Zero());
        RigidBodyDef def;
        def.transform       = &transforms[idx];
        def.circleShape     = &circles[idx];
        def.type            = type;
        def.mass            = mass;
        def.momentOfInertia = 1.0f;
        def.allowSleeping   = false;
        return world->AddRigidBody(def);
    }

    void Step(int n = 1) { for (int i = 0; i < n; ++i) world->Update(kDt); }
};

// ===========================================================================
// 1. Remove body with active constraint — no crash
// ===========================================================================

TEST(RigidBody2D_Robustness, RemoveBody_WithActiveConstraint_NoCrash)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 3.0f, 0.0f);

    f.world->AddConstraint(new PinJoint(a, Vector2D::Zero(), b, Vector2D::Zero()));
    f.Step(5);

    // Remove body b — constraint referencing b should be auto-removed
    f.world->RemoveRigidBody(b);

    // Should not crash when stepping after removal
    f.Step(10);

    EXPECT_EQ(f.world->GetConstraints().Size(), 0u);
    EXPECT_EQ(f.world->GetRigidBodyCount(), 1);
}

// ===========================================================================
// 2. Both constraint bodies static — no divide-by-zero, no crash
// ===========================================================================

TEST(RigidBody2D_Robustness, Constraint_BothBodiesStatic_NoCrash)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* b = f.MakeBody(1, 3.0f, 0.0f, BodyType::kStatic);

    f.world->AddConstraint(
        new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 3.0f));

    // Both static: invMass=0, invInertia=0, effMass=0 — should be a no-op
    f.Step(30);

    // Positions should not have moved
    EXPECT_NEAR(a->GetTransform()->GetWorldPosition().x, 0.0f, 1e-5f);
    EXPECT_NEAR(b->GetTransform()->GetWorldPosition().x, 3.0f, 1e-5f);
}

// ===========================================================================
// 3. Multiple constraint types on same body
// ===========================================================================

TEST(RigidBody2D_Robustness, MultipleConstraintTypes_SameBody)
{
    RobustnessFixture f;
    RigidBody2D* anchor = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* mid    = f.MakeBody(1, 3.0f, 0.0f);
    RigidBody2D* end    = f.MakeBody(2, 6.0f, 0.0f);

    // Pin mid to anchor, distance from mid to end
    f.world->AddConstraint(new PinJoint(anchor, Vector2D::Zero(), mid, Vector2D::Zero()));
    f.world->AddConstraint(
        new DistanceConstraint(mid, Vector2D::Zero(), end, Vector2D::Zero(), 3.0f));

    end->ApplyImpulse(Vector2D(0.0f, 5.0f));
    f.Step(60);

    // Mid should stay near anchor (pin), end should stay ~3 from mid (distance)
    const Vector2D pMid = mid->GetTransform()->GetWorldPosition();
    const Vector2D pEnd = end->GetTransform()->GetWorldPosition();
    float distMidToAnchor = std::sqrt(pMid.x * pMid.x + pMid.y * pMid.y);
    float dx = pEnd.x - pMid.x;
    float dy = pEnd.y - pMid.y;
    float distMidToEnd = std::sqrt(dx * dx + dy * dy);

    EXPECT_LT(distMidToAnchor, 0.5f) << "Pin should keep mid near anchor";
    EXPECT_NEAR(distMidToEnd, 3.0f, 0.4f) << "Distance constraint should hold";
}

// ===========================================================================
// 4. Spring with very high stiffness — no explosion
// ===========================================================================

TEST(RigidBody2D_Robustness, Spring_ModerateStiffness_Converges)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* b = f.MakeBody(1, 4.0f, 0.0f);

    // k=50, damping=5 — moderate spring, well within stable SI solver limits
    f.world->AddConstraint(
        new SpringConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(),
                             2.0f, 50.0f, 5.0f));

    f.Step(60);

    const float x = b->GetTransform()->GetWorldPosition().x;
    EXPECT_GT(x, -20.0f) << "Spring diverged to negative";
    EXPECT_LT(x,  20.0f) << "Spring diverged to positive";
}

// ===========================================================================
// 5. Overlapping bodies at spawn — separation via collision response
// ===========================================================================

TEST(RigidBody2D_Robustness, OverlappingBodiesAtSpawn_Separate)
{
    using Grid = Dia::Geometry2D::SpatialGrid<Body2DBase*>;

    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(0.5f, 0.0f));  // circles radius 1, overlap = 1.5

    Dia::Geometry2D::Circle cA(1.0f, Vector2D::Zero());
    Dia::Geometry2D::Circle cB(1.0f, Vector2D::Zero());

    Grid::Def gd;
    gd.worldBounds = Dia::Geometry2D::AARect(Vector2D(-50.0f, -50.0f), Vector2D(50.0f, 50.0f));
    gd.cellSize = 10.0f;
    Grid* grid = new Grid(gd);

    WorldDef wd;
    wd.gravity       = Vector2D(0.0f, 0.0f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps   = 1;
    wd.broadPhase    = grid;
    PhysicsWorld world(wd);

    RigidBodyDef defA, defB;
    defA.transform = &tA; defA.circleShape = &cA;
    defA.mass = 1.0f; defA.momentOfInertia = 1.0f; defA.allowSleeping = false;
    defB.transform = &tB; defB.circleShape = &cB;
    defB.mass = 1.0f; defB.momentOfInertia = 1.0f; defB.allowSleeping = false;

    RigidBody2D* bodyA = world.AddRigidBody(defA);
    RigidBody2D* bodyB = world.AddRigidBody(defB);

    for (int i = 0; i < 60; ++i)
        world.Update(kDt);

    // Bodies should have pushed apart (at minimum they shouldn't be at the same spot)
    float dx = bodyB->GetTransform()->GetWorldPosition().x - bodyA->GetTransform()->GetWorldPosition().x;
    float dy = bodyB->GetTransform()->GetWorldPosition().y - bodyA->GetTransform()->GetWorldPosition().y;
    float sep = std::sqrt(dx * dx + dy * dy);
    EXPECT_GT(sep, 0.5f) << "Overlapping bodies should have separated somewhat";

    delete grid;
}

// ===========================================================================
// 6. Constraint with one body static, one dynamic — dynamic body moves
// ===========================================================================

TEST(RigidBody2D_Robustness, Constraint_StaticDynamic_DynamicMoves)
{
    RobustnessFixture f;
    RigidBody2D* stat = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* dyn  = f.MakeBody(1, 5.0f, 0.0f);

    f.world->AddConstraint(
        new DistanceConstraint(stat, Vector2D::Zero(), dyn, Vector2D::Zero(), 2.0f));

    f.Step(60);

    // Dynamic body should have been pulled toward target distance 2 from static
    const float dist = std::abs(dyn->GetTransform()->GetWorldPosition().x);
    EXPECT_NEAR(dist, 2.0f, 0.3f) << "Dynamic should converge to target distance from static";
    // Static body should not have moved
    EXPECT_NEAR(stat->GetTransform()->GetWorldPosition().x, 0.0f, 1e-5f);
}

// ===========================================================================
// 7. ApplyForceAtPoint on body without transform — no crash
// ===========================================================================

TEST(RigidBody2D_Robustness, ApplyForceAtPoint_NoTransformInDef_UsesDefault)
{
    RigidBodyDef def;
    def.transform = nullptr;
    def.mass = 1.0f;
    def.momentOfInertia = 1.0f;

    RigidBody2D body(def);
    // Body gets default transform at origin; force applies normally
    body.ApplyForceAtPoint(Vector2D(10.0f, 0.0f), Vector2D(1.0f, 0.0f));

    EXPECT_NEAR(body.GetForceAccum().x, 10.0f, 1e-5f);
    EXPECT_NEAR(body.GetTorqueAccum(), 0.0f, 1e-5f);
}

// ===========================================================================
// 8. Remove constraint mid-simulation — bodies drift, no crash
// ===========================================================================

TEST(RigidBody2D_Robustness, RemoveConstraint_MidSimulation_BodiesDrift)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 3.0f, 0.0f);

    IConstraint* c = new PinJoint(a, Vector2D(1.5f, 0.0f), b, Vector2D(-1.5f, 0.0f));
    f.world->AddConstraint(c);
    f.Step(30);

    // Give diverging impulses and remove constraint
    a->ApplyImpulse(Vector2D(-5.0f, 0.0f));
    b->ApplyImpulse(Vector2D( 5.0f, 0.0f));
    f.world->RemoveConstraint(c);

    EXPECT_EQ(f.world->GetConstraints().Size(), 0u);

    f.Step(30);

    // Bodies should have moved far apart
    float sep = b->GetTransform()->GetWorldPosition().x - a->GetTransform()->GetWorldPosition().x;
    EXPECT_GT(sep, 5.0f) << "Bodies should drift apart after constraint removal";
}

// ===========================================================================
// 9. Kinematic body in constraint — dynamic side moves, kinematic stays
// ===========================================================================

TEST(RigidBody2D_Robustness, Constraint_KinematicDynamic_KinematicUnmoved)
{
    RobustnessFixture f;
    RigidBody2D* kin = f.MakeBody(0, 0.0f, 0.0f, BodyType::kKinematic, 1.0f);
    RigidBody2D* dyn = f.MakeBody(1, 5.0f, 0.0f);

    f.world->AddConstraint(
        new DistanceConstraint(kin, Vector2D::Zero(), dyn, Vector2D::Zero(), 2.0f));

    f.Step(60);

    // Kinematic should not have moved (ApplyImpulse returns early for kinematic)
    EXPECT_NEAR(kin->GetTransform()->GetWorldPosition().x, 0.0f, 1e-5f);
    EXPECT_NEAR(kin->GetTransform()->GetWorldPosition().y, 0.0f, 1e-5f);
}

// ===========================================================================
// 10. Remove body cleans up multiple constraints
// ===========================================================================

TEST(RigidBody2D_Robustness, RemoveBody_CleansUpMultipleConstraints)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 3.0f, 0.0f);
    RigidBody2D* c = f.MakeBody(2, 6.0f, 0.0f);

    // b is connected to both a and c
    f.world->AddConstraint(new PinJoint(a, Vector2D::Zero(), b, Vector2D::Zero()));
    f.world->AddConstraint(new PinJoint(b, Vector2D::Zero(), c, Vector2D::Zero()));
    f.world->AddConstraint(
        new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 3.0f));

    EXPECT_EQ(f.world->GetConstraints().Size(), 3u);

    // Removing b should clean up all 3 constraints (all involve b)
    f.world->RemoveRigidBody(b);

    EXPECT_EQ(f.world->GetConstraints().Size(), 0u);
    EXPECT_EQ(f.world->GetRigidBodyCount(), 2);

    // Should not crash
    f.Step(10);
}

// ===========================================================================
// 11. Sleeping body wakes when constraint impulse applied
// ===========================================================================

TEST(RigidBody2D_Robustness, SleepingBody_WakesOnConstraintImpulse)
{
    Dia::Geometry2D::Transform tA, tB;
    tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
    tB.SetWorldPosition(Vector2D(5.0f, 0.0f));
    Dia::Geometry2D::Circle cA(0.5f, Vector2D::Zero());
    Dia::Geometry2D::Circle cB(0.5f, Vector2D::Zero());

    WorldDef wd;
    wd.gravity = Vector2D(0.0f, 0.0f);
    wd.fixedTimestep = kDt;
    wd.maxSubSteps = 1;
    wd.broadPhase = nullptr;
    wd.constraintConfig.iterations = 10;
    PhysicsWorld world(wd);

    RigidBodyDef defA;
    defA.transform = &tA; defA.circleShape = &cA;
    defA.mass = 1.0f; defA.momentOfInertia = 1.0f; defA.allowSleeping = true;
    RigidBodyDef defB;
    defB.transform = &tB; defB.circleShape = &cB;
    defB.mass = 1.0f; defB.momentOfInertia = 1.0f; defB.allowSleeping = true;

    RigidBody2D* a = world.AddRigidBody(defA);
    RigidBody2D* b = world.AddRigidBody(defB);

    b->Sleep();
    EXPECT_FALSE(b->IsAwake());

    world.AddConstraint(
        new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 2.0f));

    world.Update(kDt);

    // After one step the constraint solver calls ApplyImpulse on b, which calls Wake()
    EXPECT_TRUE(b->IsAwake());
}

// ===========================================================================
// 12. Pin + Hinge on same body pair — no crash, reasonable behavior
// ===========================================================================

TEST(RigidBody2D_Robustness, PinAndHinge_SameBodyPair_NoCrash)
{
    RobustnessFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* b = f.MakeBody(1, 2.0f, 0.0f);

    b->SetAngularVelocity(3.0f);

    f.world->AddConstraint(new PinJoint(a, Vector2D(1.0f, 0.0f), b, Vector2D(-1.0f, 0.0f)));
    f.world->AddConstraint(
        new HingeJoint(a, Vector2D(1.0f, 0.0f), b, Vector2D(-1.0f, 0.0f)));

    // Should not crash or explode
    f.Step(60);

    const Vector2D pos = b->GetTransform()->GetWorldPosition();
    EXPECT_GT(pos.x, -20.0f);
    EXPECT_LT(pos.x,  20.0f);
    EXPECT_GT(pos.y, -20.0f);
    EXPECT_LT(pos.y,  20.0f);
}
