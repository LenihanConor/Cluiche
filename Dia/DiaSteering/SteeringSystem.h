#pragma once

#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/SteeringPipeline.h>
#include <DiaSteering/Health/SteeringSystemHealth.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <unordered_map>
#include <utility>

namespace Dia { namespace Observation { namespace Metric { class Gauge; class Counter; } } }

namespace Dia { namespace Steering {

    using SteeringAgentId = Dia::Core::StringCRC;

    // Registry and output cache for steering agents.
    // Callers register agents, update their state each frame, build pipelines,
    // then call Update() to evaluate all pipelines and cache desired-velocity outputs.
    // SD-005: agent state and pipeline construction are separated concerns.
    class SteeringSystem
    {
    public:
        // Register an agent. Overwrites if id already exists.
        void AddAgent(SteeringAgentId id, const SteeringAgent& agent);
        void RemoveAgent(SteeringAgentId id);

        // Update agent state (position, velocity) before calling Update().
        // No-op if id is not registered.
        void UpdateAgentState(SteeringAgentId id, const SteeringAgent& agent);

        // Retrieve the cached desired velocity from the last Update().
        // Returns {0,0} if agent is unknown or pipeline produced no output.
        Dia::Maths::Vector2D GetOutput(SteeringAgentId id) const;

        // Evaluate all per-agent pipelines and cache their outputs.
        // dt: frame delta time in seconds.
        void Update(float dt,
                    const Dia::Core::Containers::DynamicArrayC<
                        std::pair<SteeringAgentId, SteeringPipeline>, 256>& pipelines);

        int GetAgentCount() const;

        // Call once at module start to register metrics and health.
        void RegisterObservability();
        // Call once at module stop.
        void UnregisterObservability();

#ifdef DIA_DEBUG
        // Iterates all registered agents, calling fn(id, agent, desiredVelocity) for each.
        // desiredVelocity is the cached output from the most recent Update() call (zero if no Update yet).
        // Template body defined inline here — only compiled in DIA_DEBUG builds.
        template<typename Fn>
        void VisitAgents(Fn&& fn) const
        {
            for (const auto& kv : mAgents)
            {
                const Dia::Core::StringCRC id  = kv.first;
                const SteeringAgent& agent     = kv.second;
                auto outIt = mOutputs.find(id);
                const Dia::Maths::Vector2D desired = (outIt != mOutputs.end())
                    ? outIt->second
                    : Dia::Maths::Vector2D(0.0f, 0.0f);
                fn(id, agent, desired);
            }
        }
#endif

    private:
        std::unordered_map<Dia::Core::StringCRC, SteeringAgent>          mAgents;
        std::unordered_map<Dia::Core::StringCRC, Dia::Maths::Vector2D>   mOutputs;

        // Observability
        Dia::Observation::Metric::Gauge*   mMetricAgentCount    = nullptr;
        Dia::Observation::Metric::Counter* mMetricUpdates       = nullptr;
        Dia::Observation::Metric::Counter* mMetricZeroOutputs   = nullptr;
        SteeringSystemHealth               mHealthReporter{*this};
    };

} }
