#include <DiaSteering/SteeringSystem.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia { namespace Steering {

    void SteeringSystem::AddAgent(SteeringAgentId id, const SteeringAgent& agent)
    {
        mAgents[id] = agent;
        DIA_LOG_INFO("Steering", "SteeringSystem: agent added id=%u", id.Value());
    }

    void SteeringSystem::RemoveAgent(SteeringAgentId id)
    {
        auto it = mAgents.find(id);
        if (it == mAgents.end())
        {
            return;
        }
        mAgents.erase(it);
        mOutputs.erase(id);
        DIA_LOG_INFO("Steering", "SteeringSystem: agent removed id=%u", id.Value());
    }

    void SteeringSystem::UpdateAgentState(SteeringAgentId id, const SteeringAgent& agent)
    {
        auto it = mAgents.find(id);
        if (it == mAgents.end())
        {
            return;
        }
        it->second = agent;
    }

    Dia::Maths::Vector2D SteeringSystem::GetOutput(SteeringAgentId id) const
    {
        auto it = mOutputs.find(id);
        if (it == mOutputs.end())
        {
            return Dia::Maths::Vector2D(0.0f, 0.0f);
        }
        return it->second;
    }

    void SteeringSystem::Update(float /*dt*/,
        const Dia::Core::Containers::DynamicArrayC<
            std::pair<SteeringAgentId, SteeringPipeline>, 256>& pipelines)
    {
        for (unsigned int i = 0; i < pipelines.Size(); ++i)
        {
            const SteeringAgentId&  agentId  = pipelines[i].first;
            const SteeringPipeline& pipeline = pipelines[i].second;

            Dia::Maths::Vector2D output = pipeline.Evaluate();
            mOutputs[agentId] = output;

            if (mAgents.find(agentId) != mAgents.end())
            {
                if (output == Dia::Maths::Vector2D(0.0f, 0.0f))
                {
                    DIA_LOG_WARNING("Steering",
                        "SteeringSystem: zero output for agent id=%u", agentId.Value());
                }
            }
        }
    }

    int SteeringSystem::GetAgentCount() const
    {
        return static_cast<int>(mAgents.size());
    }

} }
