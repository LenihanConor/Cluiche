#pragma once

#include <gtest/gtest.h>
#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/Behaviours.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cmath>

namespace Dia { namespace Steering { namespace Testing {

    // Assert that Seek(agent, targetPos) output direction points toward targetPos
    // within toleranceDeg degrees.
    inline void AssertSeekDirection(const SteeringAgent& agent,
                                    Dia::Maths::Vector2D targetPos,
                                    float toleranceDeg = 5.0f)
    {
        Dia::Maths::Vector2D output = Seek(agent, targetPos);
        Dia::Maths::Vector2D expected = (targetPos - agent.position).AsNormalSafe();
        float cosThresh = cosf(toleranceDeg * 3.14159265f / 180.0f);
        float dot = output.AsNormalSafe().Dot(expected);
        EXPECT_GE(dot, cosThresh)
            << "Seek direction mismatch — expected toward target, got unexpected direction";
    }

    // Assert that Arrive output speed is less than agent.maxSpeed when agent is
    // within slowingRadius of target (deceleration zone).
    inline void AssertArriveDeceleration(const SteeringAgent& agent,
                                         Dia::Maths::Vector2D targetPos,
                                         float slowingRadius)
    {
        float dist = (targetPos - agent.position).Magnitude();
        ASSERT_LT(dist, slowingRadius)
            << "AssertArriveDeceleration: agent must be within slowingRadius for this check";
        Dia::Maths::Vector2D output = Arrive(agent, targetPos, slowingRadius);
        float speed = output.Magnitude();
        EXPECT_LT(speed, agent.maxSpeed)
            << "Arrive should decelerate inside slowingRadius; got speed=" << speed
            << " maxSpeed=" << agent.maxSpeed;
    }

    // Assert that Separation output pushes agent away from a single neighbour
    // (output direction has a positive dot product with the away vector).
    inline void AssertSeparationDirection(const SteeringAgent& agent,
                                          Dia::Maths::Vector2D neighbourPos,
                                          float desiredSeparation)
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64> neighbours;
        neighbours.Add(neighbourPos);

        Dia::Maths::Vector2D output = Separation(agent, neighbours, desiredSeparation);
        Dia::Maths::Vector2D away   = (agent.position - neighbourPos).AsNormalSafe();

        float dot = output.AsNormalSafe().Dot(away);
        EXPECT_GT(dot, 0.0f)
            << "Separation should push agent away from neighbour";
    }

    // Helper for building obstacle position/radius lists for ObstacleAvoidance tests.
    struct MockObstacleSet
    {
        Dia::Core::Containers::DynamicArrayC<Dia::Maths::Vector2D, 64> positions;
        Dia::Core::Containers::DynamicArrayC<float, 64>                radii;

        void Add(Dia::Maths::Vector2D pos, float radius)
        {
            positions.Add(pos);
            radii.Add(radius);
        }

        void Clear()
        {
            positions.RemoveAll();
            radii.RemoveAll();
        }
    };

} } } // namespace Dia::Steering::Testing
