#include <DiaSteering/SteeringPipeline.h>

#include <DiaObservation/Profile/DiaProfile.h>
#include <algorithm>
#include <vector>

namespace Dia { namespace Steering {

    SteeringPipeline::SteeringPipeline()
    {
    }

    void SteeringPipeline::AddContribution(int priority, float groupWeight,
                                           Dia::Maths::Vector2D desiredVelocity, float behaviourWeight)
    {
        // Find existing group with this priority
        for (GroupEntry& group : mGroups)
        {
            if (group.priority == priority)
            {
                GroupEntry::Contribution contribution;
                contribution.desiredVelocity = desiredVelocity;
                contribution.behaviourWeight = behaviourWeight;
                group.contributions.push_back(contribution);
                return;
            }
        }

        // Group does not exist yet — create it
        GroupEntry newGroup;
        newGroup.priority    = priority;
        newGroup.groupWeight = groupWeight;

        GroupEntry::Contribution contribution;
        contribution.desiredVelocity = desiredVelocity;
        contribution.behaviourWeight = behaviourWeight;
        newGroup.contributions.push_back(contribution);

        mGroups.push_back(newGroup);
    }

    void SteeringPipeline::Clear()
    {
        mGroups.clear();
    }

    Dia::Maths::Vector2D SteeringPipeline::Evaluate() const
    {
        DIA_PROFILE_SCOPE("steering.pipeline.evaluate", ::Dia::Observation::Profile::Category::kNone);

        // Work on a sorted copy so the original insertion order is preserved
        std::vector<const GroupEntry*> sortedGroups;
        sortedGroups.reserve(mGroups.size());
        for (const GroupEntry& group : mGroups)
        {
            sortedGroups.push_back(&group);
        }

        std::sort(sortedGroups.begin(), sortedGroups.end(),
                  [](const GroupEntry* a, const GroupEntry* b)
                  {
                      return a->priority < b->priority;
                  });

        static const float kNonZeroThreshold = 0.001f;

        for (const GroupEntry* group : sortedGroups)
        {
            float totalWeight = 0.0f;
            Dia::Maths::Vector2D blended(0.0f, 0.0f);

            for (const GroupEntry::Contribution& c : group->contributions)
            {
                blended    += c.desiredVelocity * c.behaviourWeight;
                totalWeight += c.behaviourWeight;
            }

            if (totalWeight <= 0.0f)
            {
                continue;
            }

            blended = blended * (1.0f / totalWeight);

            // SD-003: first group with non-zero output wins
            if (blended.Magnitude() > kNonZeroThreshold)
            {
                return blended;
            }
        }

        return Dia::Maths::Vector2D(0.0f, 0.0f);
    }

} }
