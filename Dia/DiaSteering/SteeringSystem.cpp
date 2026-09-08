#include <DiaSteering/SteeringSystem.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Health/HealthRegistry.h>

namespace Dia { namespace Steering {

    void SteeringSystem::RegisterObservability()
    {
        auto& registry = Dia::Observation::Metric::MetricRegistry::Instance();
        mMetricAgentCount  = registry.RegisterGauge(Dia::Core::StringCRC("dia.steering.agent_count"));
        mMetricUpdates     = registry.RegisterCounter(Dia::Core::StringCRC("dia.steering.updates"));
        mMetricZeroOutputs = registry.RegisterCounter(Dia::Core::StringCRC("dia.steering.zero_outputs"));

        Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealthReporter);
    }

    void SteeringSystem::UnregisterObservability()
    {
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);
        mMetricAgentCount  = nullptr;
        mMetricUpdates     = nullptr;
        mMetricZeroOutputs = nullptr;
    }

    void SteeringSystem::AddAgent(SteeringAgentId id, const SteeringAgent& agent)
    {
        mAgents[id] = agent;
        DIA_LOG_INFO("Steering", "SteeringSystem: agent added id=%u", id.Value());

        if (mMetricAgentCount)
            mMetricAgentCount->Set(static_cast<double>(mAgents.size()));
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

        if (mMetricAgentCount)
            mMetricAgentCount->Set(static_cast<double>(mAgents.size()));
    }

    void SteeringSystem::UpdateAgentState(SteeringAgentId id, const SteeringAgent& agent)
    {
        auto it = mAgents.find(id);
        if (it == mAgents.end())
        {
            DIA_LOG_WARNING("Steering",
                "SteeringSystem::UpdateAgentState — unknown agent id=%u", id.Value());
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
        DIA_TRACE_ZONE("steering.update", ::Dia::Observation::Trace::Category::kNone);

        if (mMetricUpdates)
            mMetricUpdates->Inc();

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

                    if (mMetricZeroOutputs)
                        mMetricZeroOutputs->Inc();
                }
            }
        }
    }

    int SteeringSystem::GetAgentCount() const
    {
        return static_cast<int>(mAgents.size());
    }

} }
