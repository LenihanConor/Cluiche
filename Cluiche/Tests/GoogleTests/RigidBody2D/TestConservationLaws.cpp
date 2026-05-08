#include <gtest/gtest.h>

#include <DiaRigidBody2D/World/PhysicsWorld.h>
#include <DiaRigidBody2D/Bodies/PointBody2D.h>
#include <DiaRigidBody2D/Bodies/RigidBody2D.h>
#include <DiaGeometry2D/Transform/Transform.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cmath>

using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

static WorldDef MakeZeroGravDef(float dt = 1.0f / 60.0f)
{
    WorldDef def;
    def.gravity       = Vector2D(0.0f, 0.0f);  // Zero gravity for conservation tests
    def.fixedTimestep = dt;
    def.maxSubSteps   = 8;
    def.broadPhase    = nullptr;
    return def;
}

// ---------------------------------------------------------------------------
// TEST 1 — Conservation of momentum: two free bodies, no external forces.
//
// With zero gravity and no forces applied, total momentum must remain equal
// to the initial momentum at every step (p = m*v for each body).
//
// The engine has no collision response between bodies, so bodies move
// independently. Total momentum = sum(m_i * v_i) = constant.
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Conservation, TotalLinearMomentum_NoExternalForce_Conserved)
{
    const float dt = 1.0f / 60.0f;

    WorldDef def = MakeZeroGravDef(dt);
    PhysicsWorld world(def);

    const float massA = 2.0f;
    const float massB = 3.0f;
    const Vector2D velA(4.0f, 1.0f);
    const Vector2D velB(-2.0f, 3.0f);

    PointBodyDef pd0;
    pd0.id            = Dia::Core::StringCRC("PA");
    pd0.type          = BodyType::kDynamic;
    pd0.mass          = massA;
    pd0.linearDamping = 0.0f;
    pd0.allowSleeping = false;
    PointBody2D* bodyA = world.AddPointBody(pd0);
    bodyA->GetTransform()->SetLocalPosition(Vector2D(0.0f, 0.0f));
    bodyA->SetVelocity(velA);

    PointBodyDef pd1;
    pd1.id            = Dia::Core::StringCRC("PB");
    pd1.type          = BodyType::kDynamic;
    pd1.mass          = massB;
    pd1.linearDamping = 0.0f;
    pd1.allowSleeping = false;
    PointBody2D* bodyB = world.AddPointBody(pd1);
    bodyB->GetTransform()->SetLocalPosition(Vector2D(10.0f, 0.0f));
    bodyB->SetVelocity(velB);

    // Initial total momentum
    float invMassA = bodyA->GetInverseMass();
    float invMassB = bodyB->GetInverseMass();
    float mA = (invMassA > 1e-10f) ? 1.0f / invMassA : 0.0f;
    float mB = (invMassB > 1e-10f) ? 1.0f / invMassB : 0.0f;

    Vector2D p0 = bodyA->GetVelocity() * mA + bodyB->GetVelocity() * mB;

    for (int s = 0; s < 120; ++s)
    {
        world.Update(dt);

        Vector2D p = bodyA->GetVelocity() * mA + bodyB->GetVelocity() * mB;
        EXPECT_NEAR(p.x, p0.x, 1e-3f) << "Momentum x changed at step " << s;
        EXPECT_NEAR(p.y, p0.y, 1e-3f) << "Momentum y changed at step " << s;
    }
}

// ---------------------------------------------------------------------------
// TEST 2 — Center of mass moves at constant velocity in a closed system.
//
// CoM velocity = total_momentum / total_mass. With no external forces and
// no damping, this must remain constant.
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Conservation, CenterOfMass_ConstantVelocity_NoExternalForce)
{
    const float dt = 1.0f / 60.0f;

    WorldDef def = MakeZeroGravDef(dt);
    PhysicsWorld world(def);

    struct BodySetup { float mass; Vector2D pos; Vector2D vel; };
    BodySetup setups[] = {
        { 1.0f, Vector2D(0.0f, 0.0f),   Vector2D(1.0f,  0.0f) },
        { 2.0f, Vector2D(5.0f, 0.0f),   Vector2D(-1.0f, 2.0f) },
        { 3.0f, Vector2D(-3.0f, 4.0f),  Vector2D(0.5f, -1.0f) },
    };
    static constexpr int kN = 3;

    PointBody2D* bodies[kN];
    float masses[kN];

    for (int i = 0; i < kN; ++i)
    {
        PointBodyDef pd;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "CoM%d", i);
        pd.id            = Dia::Core::StringCRC(buf);
        pd.type          = BodyType::kDynamic;
        pd.mass          = setups[i].mass;
        pd.linearDamping = 0.0f;
        pd.allowSleeping = false;
        bodies[i] = world.AddPointBody(pd);
        bodies[i]->GetTransform()->SetLocalPosition(setups[i].pos);
        bodies[i]->SetVelocity(setups[i].vel);

        float inv = bodies[i]->GetInverseMass();
        masses[i] = (inv > 1e-10f) ? 1.0f / inv : 0.0f;
    }

    // Compute initial CoM velocity
    float totalMass = 0.0f;
    Vector2D comVel0(0.0f, 0.0f);
    for (int i = 0; i < kN; ++i)
    {
        totalMass += masses[i];
        comVel0 = comVel0 + bodies[i]->GetVelocity() * masses[i];
    }
    comVel0 = comVel0 * (1.0f / totalMass);

    for (int s = 0; s < 60; ++s)
    {
        world.Update(dt);

        Vector2D comVel(0.0f, 0.0f);
        for (int i = 0; i < kN; ++i)
            comVel = comVel + bodies[i]->GetVelocity() * masses[i];
        comVel = comVel * (1.0f / totalMass);

        EXPECT_NEAR(comVel.x, comVel0.x, 1e-3f) << "CoM vx changed at step " << s;
        EXPECT_NEAR(comVel.y, comVel0.y, 1e-3f) << "CoM vy changed at step " << s;
    }
}

// ---------------------------------------------------------------------------
// TEST 3 — Kinetic energy conserved for isolated bodies with no damping.
//
// KE = 0.5 * m * v^2. With zero gravity and zero damping, each body's KE
// is constant (no forces act, velocity never changes for a free body).
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Conservation, KineticEnergy_FreeBody_Conserved)
{
    const float dt = 1.0f / 60.0f;

    WorldDef def = MakeZeroGravDef(dt);
    PhysicsWorld world(def);

    PointBodyDef pd;
    pd.id            = Dia::Core::StringCRC("KE");
    pd.type          = BodyType::kDynamic;
    pd.mass          = 2.0f;
    pd.linearDamping = 0.0f;
    pd.allowSleeping = false;
    PointBody2D* body = world.AddPointBody(pd);
    body->GetTransform()->SetLocalPosition(Vector2D(0.0f, 0.0f));
    body->SetVelocity(Vector2D(3.0f, 4.0f));

    float invM = body->GetInverseMass();
    float m    = (invM > 1e-10f) ? 1.0f / invM : 0.0f;

    Vector2D v0 = body->GetVelocity();
    float ke0 = 0.5f * m * (v0.x * v0.x + v0.y * v0.y);

    for (int s = 0; s < 60; ++s)
    {
        world.Update(dt);
        Vector2D v = body->GetVelocity();
        float ke = 0.5f * m * (v.x * v.x + v.y * v.y);
        EXPECT_NEAR(ke, ke0, 1e-3f) << "KE changed at step " << s;
    }
}

// ---------------------------------------------------------------------------
// TEST 4 — Angular momentum conserved for free-spinning rigid body.
//
// L = I * omega. With zero gravity and zero angular damping, omega is
// constant for a free RigidBody2D (no torques applied).
// ---------------------------------------------------------------------------

TEST(RigidBody2D_Conservation, AngularMomentum_FreeSpinner_Conserved)
{
    const float dt = 1.0f / 60.0f;

    WorldDef def = MakeZeroGravDef(dt);
    PhysicsWorld world(def);

    Dia::Geometry2D::Transform t;
    t.SetLocalPosition(Vector2D(0.0f, 0.0f));

    RigidBodyDef rd;
    rd.id              = Dia::Core::StringCRC("Spinner");
    rd.type            = BodyType::kDynamic;
    rd.mass            = 1.0f;
    rd.allowSleeping   = false;
    rd.momentOfInertia = 2.0f;
    rd.transform       = &t;
    RigidBody2D* rb    = world.AddRigidBody(rd);
    rb->SetAngularVelocity(5.0f);
    rb->SetVelocity(Vector2D(0.0f, 0.0f));

    float I     = 1.0f / rb->GetInverseInertia();
    float omega0 = rb->GetAngularVelocity();
    float L0     = I * omega0;

    for (int s = 0; s < 60; ++s)
    {
        world.Update(dt);
        float omega = rb->GetAngularVelocity();
        float L     = I * omega;
        EXPECT_NEAR(L, L0, 1e-3f) << "Angular momentum changed at step " << s;
    }
}
