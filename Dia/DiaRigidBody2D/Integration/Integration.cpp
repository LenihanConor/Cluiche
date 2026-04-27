#include "DiaRigidBody2D/Integration/Integration.h"

#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaMaths/Core/Angle.h"

namespace Dia::RigidBody2D {

// ---------------------------------------------------------------------------
// IntegrateLinearForces
// ---------------------------------------------------------------------------

void IntegrateLinearForces(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        PointBody2D* body = bodies[i];
        if (body->GetBodyType() != BodyType::kDynamic) continue;
        if (!body->IsAwake()) continue;

        Dia::Maths::Vector2D accel = gravity + body->GetForceAccum() * body->GetInverseMass();
        body->SetVelocity(body->GetVelocity() + accel * dt);
        body->SetVelocity(body->GetVelocity() * (1.0f - body->GetLinearDamping() * dt));
    }
}

void IntegrateLinearForces(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        RigidBody2D* body = bodies[i];
        if (body->GetBodyType() != BodyType::kDynamic) continue;
        if (!body->IsAwake()) continue;

        Dia::Maths::Vector2D accel = gravity + body->GetForceAccum() * body->GetInverseMass();
        body->SetVelocity(body->GetVelocity() + accel * dt);
        body->SetVelocity(body->GetVelocity() * (1.0f - body->GetLinearDamping() * dt));
    }
}

// ---------------------------------------------------------------------------
// IntegrateAngularForces (RigidBody2D only)
// ---------------------------------------------------------------------------

void IntegrateAngularForces(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        RigidBody2D* body = bodies[i];
        if (body->GetBodyType() != BodyType::kDynamic) continue;
        if (!body->IsAwake()) continue;

        float angularAccel = body->GetTorqueAccum() * body->GetInverseInertia();
        body->SetAngularVelocity(body->GetAngularVelocity() + angularAccel * dt);
        body->SetAngularVelocity(body->GetAngularVelocity() * (1.0f - body->GetAngularDamping() * dt));
    }
}

// ---------------------------------------------------------------------------
// IntegrateLinearVelocities
// ---------------------------------------------------------------------------

void IntegrateLinearVelocities(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        PointBody2D* body = bodies[i];
        if (body->GetBodyType() == BodyType::kStatic) continue;
        if (!body->IsAwake()) continue;
        if (!body->GetTransform()) continue;

        Dia::Geometry2D::Transform* t = body->GetTransform();
        t->SetLocalPosition(t->GetLocalPosition() + body->GetVelocity() * dt);
    }
}

void IntegrateLinearVelocities(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        RigidBody2D* body = bodies[i];
        if (body->GetBodyType() == BodyType::kStatic) continue;
        if (!body->IsAwake()) continue;
        if (!body->GetTransform()) continue;

        Dia::Geometry2D::Transform* t = body->GetTransform();
        t->SetLocalPosition(t->GetLocalPosition() + body->GetVelocity() * dt);
    }
}

// ---------------------------------------------------------------------------
// IntegrateAngularVelocities (RigidBody2D only)
// ---------------------------------------------------------------------------

void IntegrateAngularVelocities(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        RigidBody2D* body = bodies[i];
        if (body->GetBodyType() == BodyType::kStatic) continue;
        if (!body->IsAwake()) continue;
        if (!body->GetTransform()) continue;

        Dia::Geometry2D::Transform* t = body->GetTransform();
        float currentRad = t->GetLocalRotation().AsRadians();
        t->SetLocalRotation(Dia::Maths::Angle::FromRadians(currentRad + body->GetAngularVelocity() * dt));
    }
}

// ---------------------------------------------------------------------------
// ClearForceAccumulators
// ---------------------------------------------------------------------------

void ClearForceAccumulators(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
        bodies[i]->ClearForces();
}

void ClearForceAccumulators(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
        bodies[i]->ClearForces();
}

} // namespace Dia::RigidBody2D
