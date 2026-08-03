#include <DiaSteering/Behaviours.h>

#include <cmath>

namespace Dia
{
    namespace Steering
    {
        Dia::Maths::Vector2D Seek(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos)
        {
            Dia::Maths::Vector2D toTarget = targetPos - agent.position;
            if (toTarget.Magnitude() < 0.001f)
            {
                return Dia::Maths::Vector2D(0.0f, 0.0f);
            }
            return toTarget.AsNormalSafe() * agent.maxSpeed;
        }

        Dia::Maths::Vector2D Flee(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos)
        {
            Dia::Maths::Vector2D awayFromTarget = agent.position - targetPos;
            if (awayFromTarget.Magnitude() < 0.001f)
            {
                return Dia::Maths::Vector2D(0.0f, 0.0f);
            }
            return awayFromTarget.AsNormalSafe() * agent.maxSpeed;
        }

        Dia::Maths::Vector2D Arrive(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos, float slowingRadius)
        {
            Dia::Maths::Vector2D toTarget = targetPos - agent.position;
            float distance = toTarget.Magnitude();

            if (distance < 0.001f)
            {
                return Dia::Maths::Vector2D(0.0f, 0.0f);
            }

            float speed = (distance >= slowingRadius)
                ? agent.maxSpeed
                : agent.maxSpeed * (distance / slowingRadius);

            return toTarget.AsNormalSafe() * speed;
        }

        Dia::Maths::Vector2D Wander(const SteeringAgent& agent,
                                    float circleOffset,
                                    float circleRadius,
                                    float& inOutWanderAngle)
        {
            // Project a circle centre ahead of the agent along the velocity direction.
            Dia::Maths::Vector2D heading = agent.velocity.AsNormalSafe();
            Dia::Maths::Vector2D circleCenter = agent.position + heading * circleOffset;

            // Pick the target point on the circle using the current wander angle.
            Dia::Maths::Vector2D circleTarget(
                circleCenter.X() + cosf(inOutWanderAngle) * circleRadius,
                circleCenter.Y() + sinf(inOutWanderAngle) * circleRadius
            );

            // Advance angle for next call (constant drift — caller controls call frequency).
            inOutWanderAngle += 0.3f;

            return Seek(agent, circleTarget);
        }

        Dia::Maths::Vector2D Pursue(const SteeringAgent& agent,
                                    Dia::Maths::Vector2D targetPos,
                                    Dia::Maths::Vector2D targetVelocity,
                                    float predictionTime)
        {
            Dia::Maths::Vector2D predictedPos = targetPos + targetVelocity * predictionTime;
            return Seek(agent, predictedPos);
        }

        Dia::Maths::Vector2D Evade(const SteeringAgent& agent,
                                   Dia::Maths::Vector2D threatPos,
                                   Dia::Maths::Vector2D threatVelocity,
                                   float predictionTime)
        {
            Dia::Maths::Vector2D predictedPos = threatPos + threatVelocity * predictionTime;
            return Flee(agent, predictedPos);
        }
    }
}
