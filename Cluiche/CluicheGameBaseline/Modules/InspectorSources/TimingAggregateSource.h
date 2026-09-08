#pragma once
#include <DiaDebugServer/InspectorDataSourceBase.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

// Broadcasts app.timings — per-PU min/avg/max/hitches over a 60s window.
// Fires periodically; accumulates every tick.
class TimingAggregateSource final : public Dia::DebugServer::PeriodicSourceBase
{
public:
    explicit TimingAggregateSource(Dia::ApplicationFlow::IApplicationInspectable* app);

    Dia::Core::StringCRC           GetTopic()  const override;
    Dia::DebugServer::SourcePolicy GetPolicy() const override;

protected:
    void        AccumulateSample(float deltaTime) override;
    Json::Value BuildPayload()                    override;
    void        Reset()                           override;

private:
    Dia::ApplicationFlow::IApplicationInspectable* mApp = nullptr;

    struct PUAccumulator {
        double sumMs   = 0.0;
        double minMs   = 9999.0;
        double maxMs   = 0.0;
        int    samples = 0;
        int    hitches = 0;
    };

    static constexpr unsigned int kMaxPUs          = 4;
    static constexpr float        kHitchThresholdMs = 33.33f;

    Dia::Core::StringCRC mPUIds[kMaxPUs];
    PUAccumulator        mAccumulators[kMaxPUs];
    unsigned int         mPUCount = 0;

    void EnsurePUsCached();
};

}} // namespace Cluiche::AppFlow
