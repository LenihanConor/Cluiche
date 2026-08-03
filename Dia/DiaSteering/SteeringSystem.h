#pragma once

#include <DiaSteering/SteeringAgent.h>
#include <DiaSteering/SteeringPipeline.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <unordered_map>
#include <utility>

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

    private:
        std::unordered_map<Dia::Core::StringCRC, SteeringAgent>          mAgents;
        std::unordered_map<Dia::Core::StringCRC, Dia::Maths::Vector2D>   mOutputs;
    };

} }
