#pragma once

#include <DiaSteering/SteeringAgent.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
    namespace Steering
    {
        // Returns desired velocity directly toward targetPos at agent.maxSpeed.
        // Returns zero vector when the agent is already at the target.
        Dia::Maths::Vector2D Seek(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos);

        // Returns desired velocity directly away from targetPos at agent.maxSpeed.
        // Returns zero vector when the agent is already at the threat position.
        Dia::Maths::Vector2D Flee(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos);

        // Returns desired velocity toward targetPos, decelerating linearly within slowingRadius.
        // Full maxSpeed outside the slowing radius; zero at the target.
        Dia::Maths::Vector2D Arrive(const SteeringAgent& agent, Dia::Maths::Vector2D targetPos, float slowingRadius);

        // Returns desired velocity for smooth random drift on a circle projected ahead of the agent.
        // circleOffset  — distance ahead of agent.position to project the circle centre.
        // circleRadius  — radius of the wander circle.
        // inOutWanderAngle — caller-owned angle state; advanced by a fixed increment each call.
        Dia::Maths::Vector2D Wander(const SteeringAgent& agent,
                                    float circleOffset,
                                    float circleRadius,
                                    float& inOutWanderAngle);

        // Returns desired velocity toward the predicted future position of a moving target.
        // predictionTime == 0.0f seeks the current position (degenerates to Seek).
        Dia::Maths::Vector2D Pursue(const SteeringAgent& agent,
                                    Dia::Maths::Vector2D targetPos,
                                    Dia::Maths::Vector2D targetVelocity,
                                    float predictionTime = 0.0f);

        // Returns desired velocity away from the predicted future position of a moving threat.
        // predictionTime == 0.0f flees the current position (degenerates to Flee).
        Dia::Maths::Vector2D Evade(const SteeringAgent& agent,
                                   Dia::Maths::Vector2D threatPos,
                                   Dia::Maths::Vector2D threatVelocity,
                                   float predictionTime = 0.0f);

        // Returns a lateral avoidance force steering away from the nearest obstacle that
        // intersects the detection box projected ahead of the agent.
        // Returns zero vector if no obstacles are in range.
        Dia::Maths::Vector2D ObstacleAvoidance(const SteeringAgent& agent,
                                               const Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64>& obstaclePositions,
                                               const Dia::Core::Containers::DynamicArrayC<float, 64>& obstacleRadii,
                                               float detectionBoxLength);

        // Returns a separation force pushing the agent away from nearby neighbours within
        // desiredSeparation radius. Uses inverse-distance weighting; returns zero if no
        // neighbours are in range.
        Dia::Maths::Vector2D Separation(const SteeringAgent& agent,
                                        const Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64>& neighbourPositions,
                                        float desiredSeparation);
    }
}
