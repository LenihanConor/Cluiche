#include "DiaRigidBody2D/Response/ResolveCollisions.h"

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaGeometry2D/Transform/Transform.h"

#include <cmath>

namespace Dia::RigidBody2D {

// ---------------------------------------------------------------------------
// Per-body state snapshot
// ---------------------------------------------------------------------------

struct BodyState {
    Dia::Maths::Vector2D        velocity;
    float                       angularVelocity = 0.0f;
    float                       invMass         = 0.0f;
    float                       invInertia      = 0.0f;
    float                       restitution     = 0.0f;
    float                       friction        = 0.0f;
    Dia::Geometry2D::Transform* transform       = nullptr;
    Body2DBase*                 body            = nullptr;
};

static BodyState MakeState(Body2DBase* b)
{
    BodyState s;
    if (!b) return s;
    s.body            = b;
    s.velocity        = b->GetVelocity();
    s.angularVelocity = b->GetAngularVelocity();
    s.invMass         = b->GetInverseMass();
    s.invInertia      = b->GetInverseInertia();
    s.restitution     = b->GetRestitution();
    s.friction        = b->GetFriction();
    s.transform       = b->GetTransform();
    return s;
}

static Dia::Maths::Vector2D ReadVelocity(const BodyState& s)
{
    if (s.body) return s.body->GetVelocity();
    return Dia::Maths::Vector2D(0.0f, 0.0f);
}

static float ReadAngularVelocity(const BodyState& s)
{
    if (s.body) return s.body->GetAngularVelocity();
    return 0.0f;
}

static void ApplyLinear(const BodyState& s, const Dia::Maths::Vector2D& imp)
{
    if (s.body) s.body->ApplyImpulse(imp);
}

static void ApplyAngular(const BodyState& s, float torqueImp)
{
    if (s.body) s.body->ApplyAngularImpulse(torqueImp);
}

static void CorrectPosition(const BodyState& s, const Dia::Maths::Vector2D& delta)
{
    if (!s.transform) return;
    Dia::Maths::Vector2D p = s.transform->GetLocalPosition();
    s.transform->SetLocalPosition(Dia::Maths::Vector2D(p.x + delta.x, p.y + delta.y));
}

static float Cross2D(const Dia::Maths::Vector2D& a, const Dia::Maths::Vector2D& b)
{
    return a.x * b.y - a.y * b.x;
}

// ---------------------------------------------------------------------------
// ResolveCollisions
// ---------------------------------------------------------------------------

void ResolveCollisions(
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>& contacts,
    const ResponseConfig&                                               config,
    float                                                               dt)
{
    (void)dt;

    for (unsigned int ci = 0; ci < contacts.Size(); ++ci)
    {
        const Contact& contact = contacts[ci];

        BodyState sA = MakeState(contact.bodyA);
        BodyState sB = MakeState(contact.bodyB);

        const Dia::Maths::Vector2D& n = contact.normal;

        Dia::Maths::Vector2D rA(0.0f, 0.0f);
        Dia::Maths::Vector2D rB(0.0f, 0.0f);
        if (sA.transform)
        {
            Dia::Maths::Vector2D cA = sA.transform->GetWorldPosition();
            rA = Dia::Maths::Vector2D(contact.point.x - cA.x, contact.point.y - cA.y);
        }
        if (sB.transform)
        {
            Dia::Maths::Vector2D cB = sB.transform->GetWorldPosition();
            rB = Dia::Maths::Vector2D(contact.point.x - cB.x, contact.point.y - cB.y);
        }

        Dia::Maths::Vector2D vAc(sA.velocity.x - sA.angularVelocity * rA.y,
                                  sA.velocity.y + sA.angularVelocity * rA.x);
        Dia::Maths::Vector2D vBc(sB.velocity.x - sB.angularVelocity * rB.y,
                                  sB.velocity.y + sB.angularVelocity * rB.x);

        Dia::Maths::Vector2D relVel(vBc.x - vAc.x, vBc.y - vAc.y);
        float contactVel = relVel.x * n.x + relVel.y * n.y;

        if (contactVel > 0.0f) continue;

        float e = (sA.restitution < sB.restitution) ? sA.restitution : sB.restitution;
        if (contactVel > -config.restitutionVelocitySlop) e = 0.0f;

        float rACrossN = Cross2D(rA, n);
        float rBCrossN = Cross2D(rB, n);
        float denom = sA.invMass + sB.invMass
                    + rACrossN * rACrossN * sA.invInertia
                    + rBCrossN * rBCrossN * sB.invInertia;
        if (denom < 1e-10f) continue;

        float j = -(1.0f + e) * contactVel / denom;

        Dia::Maths::Vector2D jn(j * n.x, j * n.y);
        ApplyLinear(sA, Dia::Maths::Vector2D(-jn.x, -jn.y));
        ApplyLinear(sB,  jn);
        ApplyAngular(sA, -Cross2D(rA, jn));
        ApplyAngular(sB,  Cross2D(rB, jn));

        // --- Friction impulse ---
        Dia::Maths::Vector2D vAn = ReadVelocity(sA);
        Dia::Maths::Vector2D vBn = ReadVelocity(sB);
        float omegaAn = ReadAngularVelocity(sA);
        float omegaBn = ReadAngularVelocity(sB);

        Dia::Maths::Vector2D vAc2(vAn.x - omegaAn * rA.y, vAn.y + omegaAn * rA.x);
        Dia::Maths::Vector2D vBc2(vBn.x - omegaBn * rB.y, vBn.y + omegaBn * rB.x);
        Dia::Maths::Vector2D relVel2(vBc2.x - vAc2.x, vBc2.y - vAc2.y);

        float relDotN = relVel2.x * n.x + relVel2.y * n.y;
        Dia::Maths::Vector2D tangent(relVel2.x - relDotN * n.x, relVel2.y - relDotN * n.y);
        float tLen = std::sqrt(tangent.x * tangent.x + tangent.y * tangent.y);

        if (tLen > 1e-6f)
        {
            tangent.x /= tLen;
            tangent.y /= tLen;

            float rACrossT = Cross2D(rA, tangent);
            float rBCrossT = Cross2D(rB, tangent);
            float denomT   = sA.invMass + sB.invMass
                           + rACrossT * rACrossT * sA.invInertia
                           + rBCrossT * rBCrossT * sB.invInertia;

            if (denomT > 1e-10f)
            {
                float jt      = -(relVel2.x * tangent.x + relVel2.y * tangent.y) / denomT;
                float mu      = std::sqrt(sA.friction * sB.friction);
                float maxFric = j * mu;

                if (jt >  maxFric) jt =  maxFric;
                if (jt < -maxFric) jt = -maxFric;

                Dia::Maths::Vector2D ft(jt * tangent.x, jt * tangent.y);
                ApplyLinear(sA, Dia::Maths::Vector2D(-ft.x, -ft.y));
                ApplyLinear(sB,  ft);
                ApplyAngular(sA, -Cross2D(rA, ft));
                ApplyAngular(sB,  Cross2D(rB, ft));
            }
        }

        // --- Baumgarte positional correction ---
        float pen = contact.depth - config.baumgarteSlop;
        if (pen > 0.0f)
        {
            float invMassSum = sA.invMass + sB.invMass;
            if (invMassSum > 1e-10f)
            {
                float mag = pen / invMassSum * config.baumgarteFactor;
                Dia::Maths::Vector2D corr(mag * n.x, mag * n.y);
                CorrectPosition(sA, Dia::Maths::Vector2D(-sA.invMass * corr.x, -sA.invMass * corr.y));
                CorrectPosition(sB, Dia::Maths::Vector2D( sB.invMass * corr.x,  sB.invMass * corr.y));
            }
        }
    }
}

} // namespace Dia::RigidBody2D
