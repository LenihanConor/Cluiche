#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstring>

// Forward declarations — keep DiaEconomy headers out of this header
namespace Dia { namespace Economy {
    class EconomySystem;
}} // namespace Dia::Economy

namespace Cluiche { namespace AppFlow {

// Broadcasts economy.instances — per-instance pool state with sparkline history.
// Uses change-detected strategy; sampled at ~10 Hz; ring buffer depth 600 entries.
class EconomyInstancesSource : public Dia::DebugServer::ChangeDetectedSourceBase
{
public:
    explicit EconomyInstancesSource(Dia::Economy::EconomySystem& system);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

    // Override to advance sample accumulator before base triggers CollectAndHash.
    void Tick(float deltaTime, int connectionCount, int subscriptionCount) override;

protected:
    unsigned int CollectAndHash(Json::Value& payload) override;

private:
    static constexpr float        kSampleIntervalSec        = 0.1f;
    static constexpr unsigned int kHistoryDepth             = 600;
    static constexpr unsigned int kStarvationThresholdTicks = 3;

    struct ResourceTracker
    {
        Dia::Core::StringCRC resource_name;
        float                capped_duration_s   = 0.0f;
        float                starved_duration_s  = 0.0f;
        unsigned int         consecutive_starved = 0;
        float                history[kHistoryDepth];
        unsigned int         history_count       = 0;
        unsigned int         history_head        = 0;   // index of oldest entry in ring
    };

    struct InstanceTracker
    {
        Dia::Core::StringCRC                                           instance_name;
        Dia::Core::Containers::DynamicArrayC<ResourceTracker, 16>     resources;
    };

    // Returns or creates the tracker for the given (instance, resource) pair.
    ResourceTracker* GetOrCreateTracker(Dia::Core::StringCRC instanceName,
                                        Dia::Core::StringCRC resourceName);

    // Appends value to the ring buffer (newest-last).
    static void PushHistory(ResourceTracker& rt, float value);

    // Updates saturation/starvation counters and conditionally samples history.
    // Called at the start of CollectAndHash each tick.
    void UpdateTrackers();

    Dia::Economy::EconomySystem&                                mSystem;
    Dia::Core::Containers::DynamicArrayC<InstanceTracker, 16>  mTrackers;
    float                                                       mSampleAccumulator = 0.0f;
    float                                                       mLastDelta         = 0.0f;
    unsigned int                                                mFrame             = 0;
};

}} // namespace Cluiche::AppFlow
