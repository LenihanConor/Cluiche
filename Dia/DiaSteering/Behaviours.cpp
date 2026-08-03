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

        Dia::Maths::Vector2D ObstacleAvoidance(const SteeringAgent& agent,
                                               const Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64>& obstaclePositions,
                                               const Dia::Core::Containers::DynamicArrayC<float, 64>& obstacleRadii,
                                               float detectionBoxLength)
        {
            static const float kHalfBoxWidth = 0.5f;

            Dia::Maths::Vector2D heading = agent.velocity.AsNormalSafe();
            // Lateral axis: 90 degrees CCW from heading
            Dia::Maths::Vector2D lateral(-heading.Y(), heading.X());

            unsigned int count = obstaclePositions.Size();

            // Find the nearest obstacle that intersects the detection box
            float nearestFwdDist = detectionBoxLength + 1.0f; // sentinel: beyond box
            int   nearestIndex   = -1;
            float nearestLateral = 0.0f;

            for (unsigned int i = 0; i < count; ++i)
            {
                Dia::Maths::Vector2D toObstacle = obstaclePositions[i] - agent.position;

                // Project onto heading (forward) and lateral axes
                float fwdDist  = toObstacle.Dot(heading);
                float latDist  = toObstacle.Dot(lateral);

                // Check: obstacle centre must be ahead within [0, detectionBoxLength]
                if (fwdDist < 0.0f || fwdDist > detectionBoxLength)
                {
                    continue;
                }

                // Check: lateral distance must be within half-box-width + obstacle radius
                float combinedWidth = obstacleRadii[i] + kHalfBoxWidth;
                float absLatDist = latDist < 0.0f ? -latDist : latDist;
                if (absLatDist >= combinedWidth)
                {
                    continue;
                }

                // Keep the nearest (smallest forward distance)
                if (fwdDist < nearestFwdDist)
                {
                    nearestFwdDist = fwdDist;
                    nearestIndex   = static_cast<int>(i);
                    nearestLateral = latDist;
                }
            }

            if (nearestIndex < 0)
            {
                return Dia::Maths::Vector2D(0.0f, 0.0f);
            }

            // Steer laterally away from the nearest obstacle (opposite sign of lateral offset)
            // Closer obstacle → stronger force (scale by remaining box fraction)
            float strength = agent.maxSpeed * (1.0f - nearestFwdDist / detectionBoxLength);
            if (strength < 0.0f) { strength = 0.0f; }

            float direction = (nearestLateral >= 0.0f) ? -1.0f : 1.0f;
            return lateral * (direction * strength);
        }

        Dia::Maths::Vector2D Separation(const SteeringAgent& agent,
                                        const Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64>& neighbourPositions,
                                        float desiredSeparation)
        {
            Dia::Maths::Vector2D accumulated(0.0f, 0.0f);
            unsigned int count = neighbourPositions.Size();

            for (unsigned int i = 0; i < count; ++i)
            {
                Dia::Maths::Vector2D away = agent.position - neighbourPositions[i];
                float distance = away.Magnitude();

                // Skip neighbours at the same position (avoid divide-by-zero)
                // and neighbours outside the desired separation radius
                if (distance < 0.001f || distance >= desiredSeparation)
                {
                    continue;
                }

                // Inverse-distance weighted push
                accumulated = accumulated + away.AsNormalSafe() * (1.0f / distance);
            }

            if (accumulated.Magnitude() < 0.001f)
            {
                return Dia::Maths::Vector2D(0.0f, 0.0f);
            }

            return accumulated.AsNormalSafe() * agent.maxSpeed;
        }
    }
}
