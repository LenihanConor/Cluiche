#include "DiaRigidBody2D/Integration/UpdateSleepTimers.h"

#include <cmath>

namespace Dia::RigidBody2D {

void UpdateSleepTimers(
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies,
    float dt, float linearThreshold, float sleepTimeThreshold)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        PointBody2D* body = bodies[i];
        if (body->GetBodyType() != BodyType::kDynamic) continue;
        if (!body->AllowsSleeping()) continue;

        const Dia::Maths::Vector2D& v = body->GetVelocity();
        const float speedSq = v.x * v.x + v.y * v.y;

        if (speedSq < linearThreshold * linearThreshold)
        {
            body->SetSleepTimer(body->GetSleepTimer() + dt);
            if (body->GetSleepTimer() >= sleepTimeThreshold)
            {
                body->SetSleepState(SleepState::kSleeping);
                body->SetVelocity(Dia::Maths::Vector2D::Zero());
            }
        }
        else
        {
            body->SetSleepTimer(0.0f);
            body->SetSleepState(SleepState::kAwake);
        }
    }
}

void UpdateSleepTimers(
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies,
    float dt, float linearThreshold, float angularThreshold, float sleepTimeThreshold)
{
    for (unsigned int i = 0; i < bodies.Size(); ++i)
    {
        RigidBody2D* body = bodies[i];
        if (body->GetBodyType() != BodyType::kDynamic) continue;
        if (!body->AllowsSleeping()) continue;

        const Dia::Maths::Vector2D& v = body->GetVelocity();
        const float speedSq  = v.x * v.x + v.y * v.y;
        const float angSpeed = std::abs(body->GetAngularVelocity());

        if (speedSq < linearThreshold * linearThreshold &&
            angSpeed < angularThreshold)
        {
            body->SetSleepTimer(body->GetSleepTimer() + dt);
            if (body->GetSleepTimer() >= sleepTimeThreshold)
            {
                body->SetSleepState(SleepState::kSleeping);
                body->SetVelocity(Dia::Maths::Vector2D::Zero());
                body->SetAngularVelocity(0.0f);
            }
        }
        else
        {
            body->SetSleepTimer(0.0f);
            body->SetSleepState(SleepState::kAwake);
        }
    }
}

} // namespace Dia::RigidBody2D
