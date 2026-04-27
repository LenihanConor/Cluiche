#include <gtest/gtest.h>

#include <DiaRigidBody2D/Response/ResolveCollisions.h>
#include <DiaRigidBody2D/Detection/Contact.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

static constexpr float kDt = 1.0f / 60.0f;

// Build a Contact with two PointBody2D*, normal pointing from B toward A (+x)
static Contact MakePPContact(PointBody2D* a, PointBody2D* b,
                             const Vector2D& normal, float depth,
                             const Vector2D& point = Vector2D(0.0f, 0.0f))
{
    Contact c;
    c.bodyA = a;
    c.bodyB = b;
    c.normal = normal;
    c.depth  = depth;
    c.point  = point;
    return c;
}

// ---------------------------------------------------------------------------
// Test 1 — Head-on elastic collision (e=1): equal-mass bodies exchange velocities
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, HeadOn_Elastic_EqualMass_VelocitiesExchanged)
{
    Dia::Geometry2D::Transform tA, tB;
    tA.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tB.SetLocalPosition(Vector2D( 1.0f, 0.0f));

    PointBodyDef defA, defB;
    defA.transform = &tA; defA.mass = 1.0f; defA.restitution = 1.0f; defA.friction = 0.0f;
    defB.transform = &tB; defB.mass = 1.0f; defB.restitution = 1.0f; defB.friction = 0.0f;
    PointBody2D bodyA(defA), bodyB(defB);

    bodyA.SetVelocity(Vector2D( 2.0f, 0.0f));
    bodyB.SetVelocity(Vector2D(-2.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(MakePPContact(&bodyA, &bodyB, Vector2D(1.0f, 0.0f), 0.1f));

    ResponseConfig cfg;
    cfg.baumgarteFactor = 0.0f;  // disable positional correction for this test
    ResolveCollisions(contacts, cfg, kDt);

    // After elastic head-on collision equal masses swap velocities
    EXPECT_NEAR(bodyA.GetVelocity().x, -2.0f, 1e-4f);
    EXPECT_NEAR(bodyB.GetVelocity().x,  2.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Test 2 — Perfectly inelastic (e=0): relative velocity at contact = 0 after
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, HeadOn_Inelastic_RelativeVelocityZero)
{
    Dia::Geometry2D::Transform tA, tB;
    tA.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tB.SetLocalPosition(Vector2D( 1.0f, 0.0f));

    PointBodyDef defA, defB;
    defA.transform = &tA; defA.mass = 1.0f; defA.restitution = 0.0f; defA.friction = 0.0f;
    defB.transform = &tB; defB.mass = 1.0f; defB.restitution = 0.0f; defB.friction = 0.0f;
    PointBody2D bodyA(defA), bodyB(defB);

    bodyA.SetVelocity(Vector2D( 2.0f, 0.0f));
    bodyB.SetVelocity(Vector2D(-2.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(MakePPContact(&bodyA, &bodyB, Vector2D(1.0f, 0.0f), 0.1f));

    ResponseConfig cfg;
    cfg.baumgarteFactor         = 0.0f;
    cfg.restitutionVelocitySlop = 0.0f;  // never suppress restitution
    ResolveCollisions(contacts, cfg, kDt);

    // Relative normal velocity should be 0 (perfectly inelastic)
    float relVelX = bodyB.GetVelocity().x - bodyA.GetVelocity().x;
    EXPECT_NEAR(relVelX, 0.0f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Test 3 — Body vs static wall (e=0.5): bounce at half speed
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, BounceOffStaticWall_HalfRestitution)
{
    Dia::Geometry2D::Transform tA, tWall;
    tA.SetLocalPosition(Vector2D(0.0f, 0.0f));
    tWall.SetLocalPosition(Vector2D(2.0f, 0.0f));

    PointBodyDef defA, defWall;
    defA.transform    = &tA;    defA.mass    = 1.0f; defA.restitution    = 0.5f; defA.friction = 0.0f;
    defWall.transform = &tWall; defWall.mass = 0.0f; defWall.type = BodyType::kStatic;
    defWall.restitution = 0.5f; defWall.friction = 0.0f;
    PointBody2D bodyA(defA), wall(defWall);

    bodyA.SetVelocity(Vector2D(4.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(MakePPContact(&bodyA, &wall, Vector2D(1.0f, 0.0f), 0.05f));

    ResponseConfig cfg;
    cfg.baumgarteFactor         = 0.0f;
    cfg.restitutionVelocitySlop = 0.0f;
    ResolveCollisions(contacts, cfg, kDt);

    // Wall is static: invMass=0, only bodyA moves
    // new vel = -e * old vel = -0.5*4 = -2
    EXPECT_NEAR(bodyA.GetVelocity().x, -2.0f, 1e-4f);
    EXPECT_NEAR(wall.GetVelocity().x,   0.0f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Test 4 — 100:1 mass ratio: heavy body barely moves
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, MassRatio_HeavyBodyBarelyMoves)
{
    Dia::Geometry2D::Transform tLight, tHeavy;
    tLight.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tHeavy.SetLocalPosition(Vector2D( 1.0f, 0.0f));

    PointBodyDef defLight, defHeavy;
    defLight.transform = &tLight; defLight.mass = 1.0f;   defLight.restitution = 0.0f; defLight.friction = 0.0f;
    defHeavy.transform = &tHeavy; defHeavy.mass = 100.0f; defHeavy.restitution = 0.0f; defHeavy.friction = 0.0f;
    PointBody2D light(defLight), heavy(defHeavy);

    light.SetVelocity(Vector2D(10.0f, 0.0f));
    heavy.SetVelocity(Vector2D( 0.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    // normal from B(heavy) toward A(light) = +x
    contacts.Add(MakePPContact(&light, &heavy, Vector2D(1.0f, 0.0f), 0.1f));

    ResponseConfig cfg;
    cfg.baumgarteFactor         = 0.0f;
    cfg.restitutionVelocitySlop = 0.0f;
    ResolveCollisions(contacts, cfg, kDt);

    // With 100:1 mass ratio and e=0, both bodies end up near the shared momentum velocity ~0.099
    // The heavy body's velocity *change* is tiny (0 → ~0.099) compared to light body's change (10 → ~0.099)
    float deltaLight = std::abs(light.GetVelocity().x - 10.0f);
    float deltaHeavy = std::abs(heavy.GetVelocity().x - 0.0f);
    // Heavy body gains ~0.1x of the light body's change (mass ratio 1:100)
    EXPECT_LT(deltaHeavy, deltaLight * 0.02f + 0.01f);
}

// ---------------------------------------------------------------------------
// Test 5 — Static body not moved by impulse
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, StaticBody_NotMovedByImpulse)
{
    Dia::Geometry2D::Transform tDyn, tStatic;
    tDyn.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tStatic.SetLocalPosition(Vector2D(1.0f, 0.0f));

    PointBodyDef defDyn, defStatic;
    defDyn.transform    = &tDyn;    defDyn.mass    = 1.0f; defDyn.restitution    = 0.0f; defDyn.friction = 0.0f;
    defStatic.transform = &tStatic; defStatic.type = BodyType::kStatic; defStatic.mass = 0.0f;
    defStatic.restitution = 0.0f; defStatic.friction = 0.0f;
    PointBody2D dyn(defDyn), stat(defStatic);

    dyn.SetVelocity(Vector2D(5.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(MakePPContact(&dyn, &stat, Vector2D(1.0f, 0.0f), 0.05f));

    ResponseConfig cfg;
    cfg.baumgarteFactor         = 0.0f;
    cfg.restitutionVelocitySlop = 0.0f;
    ResolveCollisions(contacts, cfg, kDt);

    EXPECT_FLOAT_EQ(stat.GetVelocity().x, 0.0f);
    EXPECT_FLOAT_EQ(stat.GetVelocity().y, 0.0f);
}

// ---------------------------------------------------------------------------
// Test 6 — Kinematic body not moved by impulse; dynamic receives full impulse
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, KinematicBody_NotMovedByImpulse)
{
    Dia::Geometry2D::Transform tDyn, tKin;
    tDyn.SetLocalPosition(Vector2D(-1.0f, 0.0f));
    tKin.SetLocalPosition(Vector2D( 1.0f, 0.0f));

    PointBodyDef defDyn, defKin;
    defDyn.transform = &tDyn; defDyn.mass = 1.0f;
    defDyn.restitution = 0.0f; defDyn.friction = 0.0f;
    defKin.transform = &tKin; defKin.type = BodyType::kKinematic; defKin.mass = 1.0f;
    defKin.restitution = 0.0f; defKin.friction = 0.0f;
    PointBody2D dyn(defDyn), kin(defKin);

    dyn.SetVelocity(Vector2D(4.0f, 0.0f));
    kin.SetVelocity(Vector2D(0.0f, 0.0f));

    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
    contacts.Add(MakePPContact(&dyn, &kin, Vector2D(1.0f, 0.0f), 0.05f));

    ResponseConfig cfg;
    cfg.baumgarteFactor         = 0.0f;
    cfg.restitutionVelocitySlop = 0.0f;
    ResolveCollisions(contacts, cfg, kDt);

    // Kinematic body should not change velocity (ApplyImpulse returns early for kKinematic)
    EXPECT_FLOAT_EQ(kin.GetVelocity().x, 0.0f);
    // Dynamic body's velocity should have decreased (it absorbed the full impulse)
    EXPECT_LT(dyn.GetVelocity().x, 4.0f);
}

// ---------------------------------------------------------------------------
// Test 7 — Positional correction: overlapping bodies separate over ~5 steps
// ---------------------------------------------------------------------------

TEST(RigidBody2D_CollisionResponse, PositionalCorrection_ReducesPenetration)
{
    Dia::Geometry2D::Transform tA, tB;
    tA.SetLocalPosition(Vector2D(0.0f, 0.0f));
    tB.SetLocalPosition(Vector2D(1.5f, 0.0f));  // overlapping circles r=1

    PointBodyDef defA, defB;
    defA.transform = &tA; defA.mass = 1.0f; defA.restitution = 0.0f; defA.friction = 0.0f;
    defB.transform = &tB; defB.mass = 1.0f; defB.restitution = 0.0f; defB.friction = 0.0f;
    PointBody2D bodyA(defA), bodyB(defB);

    const float initialDepth = 0.5f;

    ResponseConfig cfg;
    cfg.baumgarteSlop           = 0.0f;
    cfg.baumgarteFactor         = 0.2f;
    cfg.restitutionVelocitySlop = 0.0f;

    for (int step = 0; step < 5; ++step)
    {
        float depth = initialDepth;  // simplification: recompute per step would require re-detection
        Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts> contacts;
        contacts.Add(MakePPContact(&bodyA, &bodyB, Vector2D(1.0f, 0.0f), depth));
        ResolveCollisions(contacts, cfg, kDt);
    }

    // After 5 steps of correction, A should have moved left and B right
    float separation = bodyB.GetTransform()->GetLocalPosition().x - bodyA.GetTransform()->GetLocalPosition().x;
    EXPECT_GT(separation, 1.5f);  // started at 1.5, should have increased
}
