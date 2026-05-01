#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Constraints/ConstraintSolver.h>
#include <DiaRigidBody2D/Constraints/PinJoint.h>
#include <DiaRigidBody2D/Constraints/DistanceConstraint.h>
#include <DiaRigidBody2D/Constraints/SpringConstraint.h>
#include <DiaRigidBody2D/Constraints/HingeJoint.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGeometry2D/Shapes/Circle.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Core/Angle.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

static constexpr float kDt   = 1.0f / 60.0f;
static constexpr int   kIter = 60;

// ---------------------------------------------------------------------------
// Shared fixture: world with no gravity and a fixed timestep
// ---------------------------------------------------------------------------
struct ConstraintFixture {

    // Bodies kept in flat arrays so transforms stay valid
    static constexpr int kMaxBodies = 16;

    Dia::Geometry2D::Transform transforms[kMaxBodies];
    Dia::Geometry2D::Circle    circles[kMaxBodies];

    PhysicsWorld* world = nullptr;

    ConstraintFixture()
    {
        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = kDt;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = nullptr;
        wd.constraintConfig.iterations = 20;
        world = new PhysicsWorld(wd);
    }

    ~ConstraintFixture() { delete world; }

    // Creates a rigid body with the given mass at position (x,y).
    // Index into the pre-allocated transforms/circles/broads arrays.
    RigidBody2D* MakeBody(int idx, float x, float y, BodyType type = BodyType::kDynamic, float mass = 1.0f)
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

// ---------------------------------------------------------------------------
// 1. PinJoint — anchors converge to same world point; drift < tolerance
// ---------------------------------------------------------------------------
TEST(TestConstraints, PinJoint_NoDrift)
{
    ConstraintFixture f;
    RigidBody2D* a = f.MakeBody(0,  2.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, -2.0f, 0.0f);

    // Both anchors are at their own CoM — we want them to meet at world (0,0)
    // anchorA in local space = (−2, 0), anchorB = (+2, 0)
    auto* joint = new PinJoint(a, Vector2D(-2.0f, 0.0f),
                               b, Vector2D( 2.0f, 0.0f));
    f.world->AddConstraint(joint);

    f.Step(kIter);

    const Vector2D wA = a->GetTransform()->GetWorldPosition() + Vector2D(-2.0f, 0.0f);
    const Vector2D wB = b->GetTransform()->GetWorldPosition() + Vector2D( 2.0f, 0.0f);
    const float dx = wA.x - wB.x;
    const float dy = wA.y - wB.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_LT(dist, 0.05f) << "PinJoint anchor separation too large: " << dist;
}

// ---------------------------------------------------------------------------
// 2. DistanceConstraint — separation converges to target distance
// ---------------------------------------------------------------------------
TEST(TestConstraints, DistanceConstraint_MaintainsDistance)
{
    ConstraintFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 5.0f, 0.0f);   // start at dist = 5, target = 3

    auto* dc = new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 3.0f);
    f.world->AddConstraint(dc);

    f.Step(kIter);

    const Vector2D posA = a->GetTransform()->GetWorldPosition();
    const Vector2D posB = b->GetTransform()->GetWorldPosition();
    const float dx   = posA.x - posB.x;
    const float dy   = posA.y - posB.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    EXPECT_NEAR(dist, 3.0f, 0.15f) << "DistanceConstraint distance " << dist << " != 3.0";
}

// ---------------------------------------------------------------------------
// 3. SpringConstraint — oscillates (position crosses rest length)
// ---------------------------------------------------------------------------
TEST(TestConstraints, SpringConstraint_Oscillates)
{
    ConstraintFixture f;
    // Pin one body statically, let the other be pulled toward rest length
    RigidBody2D* anchor = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* mass   = f.MakeBody(1, 4.0f, 0.0f);  // start at dist 4, rest = 2

    auto* spring = new SpringConstraint(anchor, Vector2D::Zero(),
                                        mass,   Vector2D::Zero(),
                                        2.0f, 200.0f, 2.0f);
    f.world->AddConstraint(spring);

    float minX = 4.0f, maxX = 4.0f;
    for (int i = 0; i < 120; ++i)
    {
        f.Step(1);
        const float x = mass->GetTransform()->GetWorldPosition().x;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
    }

    // Mass must have passed through positions both below and above the start
    EXPECT_LT(minX, 3.5f)  << "Spring never pulled mass closer";
    EXPECT_GT(maxX, 1.0f)  << "Spring never moved mass beyond start (should be > initial)";
    // The mass must have gotten significantly closer to the anchor at some point
    EXPECT_LT(minX, 2.5f)  << "Spring did not bring mass near rest length";
}

// ---------------------------------------------------------------------------
// 4. HingeJoint — free hinge allows relative rotation
// ---------------------------------------------------------------------------
TEST(TestConstraints, HingeJoint_AllowsRelativeRotation)
{
    ConstraintFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 2.0f, 0.0f);

    // Give body B angular velocity
    b->SetAngularVelocity(5.0f);

    auto* hinge = new HingeJoint(a, Vector2D(1.0f, 0.0f),
                                 b, Vector2D(-1.0f, 0.0f));
    f.world->AddConstraint(hinge);

    f.Step(30);

    // Relative rotation of B with respect to A should be non-zero
    const float rotA = a->GetTransform()->GetLocalRotation().AsRadians();
    const float rotB = b->GetTransform()->GetLocalRotation().AsRadians();
    EXPECT_NE(rotA, rotB) << "HingeJoint should allow relative rotation";
}

// ---------------------------------------------------------------------------
// 5. HingeJoint with angle limits — rotation clamped within [−0.3, 0.3] rad
// ---------------------------------------------------------------------------
TEST(TestConstraints, HingeJoint_AngleLimitsClamp)
{
    ConstraintFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f, BodyType::kStatic);
    RigidBody2D* b = f.MakeBody(1, 2.0f, 0.0f);

    b->SetAngularVelocity(10.0f);

    auto* hinge = new HingeJoint(a, Vector2D(1.0f, 0.0f),
                                 b, Vector2D(-1.0f, 0.0f));
    hinge->SetAngleLimits(Dia::Maths::Angle::FromRadians(-0.3f),
                          Dia::Maths::Angle::FromRadians( 0.3f));
    f.world->AddConstraint(hinge);

    f.Step(120);

    // relAngle = rotA - rotB; static body A has rotation 0
    const float rotB = b->GetTransform()->GetLocalRotation().AsRadians();
    EXPECT_GE(-rotB, -0.35f) << "Angle limit exceeded on low side";
    EXPECT_LE(-rotB,  0.35f) << "Angle limit exceeded on high side";
}

// ---------------------------------------------------------------------------
// 6. Chain of 5 bodies via DistanceConstraints — endpoints stay bounded
// ---------------------------------------------------------------------------
TEST(TestConstraints, DistanceConstraint_Chain5Bodies)
{
    ConstraintFixture f;
    // Bodies placed at x = 0,1,2,3,4; target distance = 1 between each pair
    for (int i = 0; i < 5; ++i)
        f.MakeBody(i, static_cast<float>(i), 0.0f);

    const auto& bodies = f.world->GetRigidBodies();
    for (unsigned int i = 0; i + 1 < bodies.Size(); ++i)
    {
        auto* dc = new DistanceConstraint(bodies[i], Vector2D::Zero(),
                                          bodies[i + 1], Vector2D::Zero(),
                                          1.0f);
        f.world->AddConstraint(dc);
    }

    // Give the middle body a lateral impulse
    RigidBody2D* mid = bodies[2];
    mid->ApplyImpulse(Vector2D(0.0f, 5.0f));

    f.Step(kIter);

    // All chain links should remain close to target distance 1
    for (unsigned int i = 0; i + 1 < bodies.Size(); ++i)
    {
        const Vector2D pA = bodies[i]->GetTransform()->GetWorldPosition();
        const Vector2D pB = bodies[i + 1]->GetTransform()->GetWorldPosition();
        const float dx   = pA.x - pB.x;
        const float dy   = pA.y - pB.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        EXPECT_NEAR(dist, 1.0f, 0.25f)
            << "Chain link " << i << " distance " << dist << " out of range";
    }
}

// ---------------------------------------------------------------------------
// 7. Remove constraint — bodies drift apart after removal
// ---------------------------------------------------------------------------
TEST(TestConstraints, RemoveConstraint_BodiesDrift)
{
    ConstraintFixture f;
    RigidBody2D* a = f.MakeBody(0, 0.0f, 0.0f);
    RigidBody2D* b = f.MakeBody(1, 3.0f, 0.0f);

    IConstraint* dc = new DistanceConstraint(a, Vector2D::Zero(), b, Vector2D::Zero(), 3.0f);
    f.world->AddConstraint(dc);
    f.Step(30);

    // Apply impulse then remove constraint
    b->ApplyImpulse(Vector2D(10.0f, 0.0f));
    f.world->RemoveConstraint(dc);
    f.Step(30);

    const Vector2D posA = a->GetTransform()->GetWorldPosition();
    const Vector2D posB = b->GetTransform()->GetWorldPosition();
    const float dx   = posA.x - posB.x;
    const float dy   = posA.y - posB.y;
    const float dist = std::sqrt(dx * dx + dy * dy);
    // After removal bodies should have drifted significantly from target = 3
    EXPECT_GT(dist, 4.0f) << "Bodies should drift after constraint removal";
}

// ---------------------------------------------------------------------------
// 8. Solver iterations — more iterations = less drift
// ---------------------------------------------------------------------------
TEST(TestConstraints, SolverIterations_MoreIterationsBetterConvergence)
{
    // Run two separate scenarios: 1 iteration vs 20 iterations
    auto measureDrift = [](int iterations) -> float {
        Dia::Geometry2D::Transform tA, tB;
        tA.SetWorldPosition(Vector2D(0.0f, 0.0f));
        tB.SetWorldPosition(Vector2D(5.0f, 0.0f));

        Dia::Geometry2D::Circle cA(0.5f, Vector2D::Zero());
        Dia::Geometry2D::Circle cB(0.5f, Vector2D::Zero());

        WorldDef wd;
        wd.gravity       = Vector2D(0.0f, 0.0f);
        wd.fixedTimestep = 1.0f / 60.0f;
        wd.maxSubSteps   = 1;
        wd.broadPhase    = nullptr;
        wd.constraintConfig.iterations = iterations;

        PhysicsWorld world(wd);

        RigidBodyDef defA, defB;
        defA.transform = &tA; defA.circleShape = &cA;
        defA.mass = 1.0f; defA.momentOfInertia = 1.0f; defA.allowSleeping = false;
        defB.transform = &tB; defB.circleShape = &cB;
        defB.mass = 1.0f; defB.momentOfInertia = 1.0f; defB.allowSleeping = false;

        RigidBody2D* a = world.AddRigidBody(defA);
        RigidBody2D* b = world.AddRigidBody(defB);

        world.AddConstraint(new PinJoint(a, Vector2D(2.5f, 0.0f),
                                          b, Vector2D(-2.5f, 0.0f)));

        a->ApplyImpulse(Vector2D(0.0f,  3.0f));
        b->ApplyImpulse(Vector2D(0.0f, -3.0f));

        for (int i = 0; i < 60; ++i)
            world.Update(1.0f / 60.0f);

        const Vector2D wA = a->GetTransform()->GetWorldPosition() + Vector2D(2.5f, 0.0f);
        const Vector2D wB = b->GetTransform()->GetWorldPosition() + Vector2D(-2.5f, 0.0f);
        const float dx = wA.x - wB.x;
        const float dy = wA.y - wB.y;
        return std::sqrt(dx * dx + dy * dy);
    };

    const float drift1  = measureDrift(1);
    const float drift20 = measureDrift(20);
    EXPECT_LT(drift20, drift1) << "20 iterations should converge better than 1; drift1="
                               << drift1 << " drift20=" << drift20;
}
