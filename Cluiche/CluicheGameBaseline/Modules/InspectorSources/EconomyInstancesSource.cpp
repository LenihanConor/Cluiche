#include "Modules/InspectorSources/EconomyInstancesSource.h"

#include <DiaEconomy/EconomySystem.h>
#include <DiaEconomy/EconomyInstance.h>
#include <DiaEconomy/EconomySchema.h>
#include <DiaCore/Json/external/json/json.h>

namespace Cluiche { namespace AppFlow {

EconomyInstancesSource::EconomyInstancesSource(Dia::Economy::EconomySystem& system)
    : mSystem(system)
{
}

Dia::Core::StringCRC EconomyInstancesSource::GetTopic() const
{
    static const Dia::Core::StringCRC kTopic("economy.instances");
    return kTopic;
}

Dia::DebugServer::SourcePolicy EconomyInstancesSource::GetPolicy() const
{
    return { Dia::DebugServer::SourceStrategy::kChangeDetected, 0.0f, 0, 0.0f };
}

// ---------------------------------------------------------------------------
// Tick — store delta for use in CollectAndHash, advance sample accumulator,
// then let the base decide whether to broadcast.
// ---------------------------------------------------------------------------
void EconomyInstancesSource::Tick(float deltaTime, int connectionCount, int subscriptionCount)
{
    mLastDelta          = deltaTime;
    mSampleAccumulator += deltaTime;
    ChangeDetectedSourceBase::Tick(deltaTime, connectionCount, subscriptionCount);
}

// ---------------------------------------------------------------------------
// GetOrCreateTracker — returns an existing tracker or inserts a new one.
// ---------------------------------------------------------------------------
EconomyInstancesSource::ResourceTracker*
EconomyInstancesSource::GetOrCreateTracker(Dia::Core::StringCRC instanceName,
                                            Dia::Core::StringCRC resourceName)
{
    // Find existing instance tracker
    for (unsigned int i = 0; i < mTrackers.Size(); ++i)
    {
        InstanceTracker& it = mTrackers[i];
        if (it.instance_name == instanceName)
        {
            // Find existing resource tracker
            for (unsigned int r = 0; r < it.resources.Size(); ++r)
            {
                if (it.resources[r].resource_name == resourceName)
                    return &it.resources[r];
            }
            // Add new resource tracker to this instance
            ResourceTracker rt;
            rt.resource_name      = resourceName;
            rt.capped_duration_s  = 0.0f;
            rt.starved_duration_s = 0.0f;
            rt.consecutive_starved = 0;
            rt.history_count      = 0;
            rt.history_head       = 0;
            memset(rt.history, 0, sizeof(rt.history));
            it.resources.Add(rt);
            return &it.resources[it.resources.Size() - 1];
        }
    }

    // Add new instance tracker, then recurse to add the resource tracker
    InstanceTracker newIt;
    newIt.instance_name = instanceName;
    mTrackers.Add(newIt);
    return GetOrCreateTracker(instanceName, resourceName);
}

// ---------------------------------------------------------------------------
// PushHistory — appends value to the ring buffer (newest-last).
// ---------------------------------------------------------------------------
/*static*/ void EconomyInstancesSource::PushHistory(ResourceTracker& rt, float value)
{
    if (rt.history_count < kHistoryDepth)
    {
        rt.history[rt.history_count] = value;
        ++rt.history_count;
    }
    else
    {
        // Ring is full — overwrite oldest slot and advance head
        rt.history[rt.history_head] = value;
        rt.history_head = (rt.history_head + 1) % kHistoryDepth;
    }
}

// ---------------------------------------------------------------------------
// UpdateTrackers — ensure trackers exist, update saturation/starvation
// counters, and conditionally push a history sample.
// ---------------------------------------------------------------------------
void EconomyInstancesSource::UpdateTrackers()
{
    const bool shouldSample = (mSampleAccumulator >= kSampleIntervalSec);

    const unsigned int instanceCount = mSystem.GetInstanceCount();
    for (unsigned int i = 0; i < instanceCount; ++i)
    {
        const Dia::Economy::EconomyInstance& instance = mSystem.GetInstanceByIndex(i);
        const Dia::Economy::EconomySchema*   schema   = instance.GetSchema();
        if (!schema)
            continue;

        const Dia::Core::StringCRC instanceName = instance.GetInstanceName();
        const unsigned int resourceCount = schema->GetResourceCount();

        for (unsigned int r = 0; r < resourceCount; ++r)
        {
            const Dia::Economy::ResourceDefinition& def = schema->GetResourceByIndex(r);
            const Dia::Core::StringCRC resourceName = def.resource_name;

            // Derived resources carry no meaningful rate or saturation state
            if (mSystem.IsDerivedResource(resourceName))
            {
                // Ensure tracker exists so history is still recorded
                ResourceTracker* rt = GetOrCreateTracker(instanceName, resourceName);
                if (shouldSample)
                    PushHistory(*rt, instance.GetValue(resourceName));
                continue;
            }

            ResourceTracker* rt = GetOrCreateTracker(instanceName, resourceName);

            const float value   = instance.GetValue(resourceName);
            const float maximum = instance.GetMaximum(resourceName);
            const float minimum = instance.GetMinimum(resourceName);

            // --- cap saturation (EI-005) -------------------------------------
            // Pool is full when value has reached its maximum and minimum < maximum.
            if (maximum > minimum && value >= maximum)
            {
                rt->capped_duration_s += mLastDelta;
            }

            // --- starvation (EI-005) -----------------------------------------
            // Count consecutive ticks at minimum; only increment duration after
            // kStarvationThresholdTicks ticks to avoid noise from momentary dips.
            if (value <= minimum)
            {
                ++rt->consecutive_starved;
                if (rt->consecutive_starved >= kStarvationThresholdTicks)
                    rt->starved_duration_s += mLastDelta;
            }
            else
            {
                rt->consecutive_starved = 0;
            }

            // --- history sample -----------------------------------------------
            if (shouldSample)
                PushHistory(*rt, value);
        }
    }

    // Consume the accumulated time once per CollectAndHash call
    if (shouldSample)
        mSampleAccumulator -= kSampleIntervalSec;
}

// ---------------------------------------------------------------------------
// CollectAndHash — update trackers then build JSON payload.
// ---------------------------------------------------------------------------
unsigned int EconomyInstancesSource::CollectAndHash(Json::Value& payload)
{
    ++mFrame;

    UpdateTrackers();

    payload["frame"] = mFrame;

    Json::Value instancesArray(Json::arrayValue);
    unsigned int hash = HashCombine(0u, mFrame);

    const unsigned int instanceCount = mSystem.GetInstanceCount();
    hash = HashCombine(hash, instanceCount);

    for (unsigned int i = 0; i < instanceCount; ++i)
    {
        const Dia::Economy::EconomyInstance& instance = mSystem.GetInstanceByIndex(i);
        const Dia::Economy::EconomySchema*   schema   = instance.GetSchema();

        const Dia::Core::StringCRC instanceName = instance.GetInstanceName();

        Json::Value instanceJson;
        instanceJson["id"] = instanceName.AsChar();

        Json::Value resourcesArray(Json::arrayValue);

        if (schema)
        {
            const unsigned int resourceCount = schema->GetResourceCount();
            for (unsigned int r = 0; r < resourceCount; ++r)
            {
                const Dia::Economy::ResourceDefinition& def = schema->GetResourceByIndex(r);
                const Dia::Core::StringCRC resourceName = def.resource_name;

                const bool isDerived = mSystem.IsDerivedResource(resourceName);
                const float value    = instance.GetValue(resourceName);

                // Contribute to hash
                hash = HashCombine(hash, resourceName.Value());
                hash = HashCombine(hash, *reinterpret_cast<const unsigned int*>(&value));

                Json::Value resJson;
                resJson["name"] = resourceName.AsChar();
                resJson["type"] = isDerived ? "derived" : "base";

                if (isDerived)
                {
                    resJson["current"]          = value;
                    resJson["cap"]              = -1;
                    resJson["net_rate"]         = 0.0f;
                    resJson["gross_income"]     = 0.0f;
                    resJson["gross_spend"]      = 0.0f;
                    resJson["capped_duration_s"]  = 0.0f;
                    resJson["starved_duration_s"] = 0.0f;
                }
                else
                {
                    const float grossIncome = instance.GetLastTickIncome(resourceName);
                    const float grossSpend  = instance.GetLastTickSpend(resourceName);
                    const float netRate     = grossIncome - grossSpend;
                    const float cap         = instance.GetMaximum(resourceName);

                    // Retrieve tracker for saturation/starvation durations
                    ResourceTracker* rt = GetOrCreateTracker(instanceName, resourceName);

                    resJson["current"]            = value;
                    resJson["cap"]                = cap;
                    resJson["net_rate"]           = netRate;
                    resJson["gross_income"]       = grossIncome;
                    resJson["gross_spend"]        = grossSpend;
                    resJson["capped_duration_s"]  = rt ? rt->capped_duration_s  : 0.0f;
                    resJson["starved_duration_s"] = rt ? rt->starved_duration_s : 0.0f;
                }

                // --- history --------------------------------------------------
                // Emit newest-last. When the ring is not yet full the oldest entry
                // is at index 0 (history_head == 0); when full, history_head is
                // the oldest entry.
                Json::Value historyArray(Json::arrayValue);
                ResourceTracker* rt = GetOrCreateTracker(instanceName, resourceName);
                if (rt && rt->history_count > 0)
                {
                    const unsigned int count = rt->history_count;
                    const unsigned int head  = (count < kHistoryDepth) ? 0u : rt->history_head;

                    for (unsigned int h = 0; h < count; ++h)
                    {
                        const unsigned int idx = (head + h) % kHistoryDepth;
                        historyArray.append(rt->history[idx]);
                    }
                }
                resJson["history"] = historyArray;

                resourcesArray.append(resJson);
            }
        }

        instanceJson["resources"] = resourcesArray;
        instancesArray.append(instanceJson);
    }

    payload["instances"] = instancesArray;
    return hash;
}

}} // namespace Cluiche::AppFlow
