#pragma once

#include <vector>

#include <DiaSteering/SteeringAgent.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia { namespace Steering {

    // Priority-group composition container.
    // Groups are sorted ascending by priority (lower int = higher priority).
    // Within a group, contributions are blended via weighted average.
    // Evaluate() returns the first (highest-priority) group whose blended output is non-zero.
    class SteeringPipeline
    {
    public:
        SteeringPipeline();

        // Add a behaviour contribution to the group at `priority`.
        // If the group does not yet exist it is created with `groupWeight`.
        // `desiredVelocity` is the output of a behaviour free function.
        // `behaviourWeight` weights this contribution within the group blend.
        void AddContribution(int priority, float groupWeight,
                             Dia::Maths::Vector2D desiredVelocity, float behaviourWeight);

        // Clear all groups and contributions (call before rebuilding each frame).
        void Clear();

        // Evaluate: blend within each priority group (weighted average), return
        // the first non-zero blended result in priority order (lowest priority int first).
        // Returns {0,0} if all groups produce zero output.
        Dia::Maths::Vector2D Evaluate() const;

    private:
        // Internal storage — STL is allowed internally (PD-004 is public-API only)
        struct GroupEntry
        {
            int   priority;
            float groupWeight;

            struct Contribution
            {
                Dia::Maths::Vector2D desiredVelocity;
                float                behaviourWeight;
            };

            std::vector<Contribution> contributions;
        };

        std::vector<GroupEntry> mGroups;
    };

} }
