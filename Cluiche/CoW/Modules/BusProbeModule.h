#pragma once
#include <DiaApplicationFlow/SimModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMessageBus/BusSubscriptionHandle.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include "Modules/MessageBusHostModule.h"
#include <cstdint>

namespace CoW {

// One message per sim tick, so the debug panel's bus card has live traffic.
// Posted and received by BusProbeModule itself. F-1 Test 6 (recorded traffic)
// uses this as its traffic source until a real CoW system posts messages.
struct BusProbeEvent
{
    static inline const Dia::Core::StringCRC kTypeId{ "CoW.BusProbeEvent" };
    uint32_t sequence = 0;
};

class BusProbeModule : public Dia::ApplicationFlow::SimModule
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Posts one BusProbeEvent per tick so the message bus has live traffic";

    explicit BusProbeModule(const Dia::Core::StringCRC& instanceId);

    uint32_t GetPostedCount() const    { return mPosted; }
    uint32_t GetDeliveredCount() const { return mDelivered; }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::MessageBusHostModule> mBus{this};
    Dia::MessageBus::BusSubscriptionHandle mSubscription;
    uint32_t mPosted    = 0;
    uint32_t mDelivered = 0;
    Dia::Observation::Metric::Counter* mPostedMetric    = nullptr;
    Dia::Observation::Metric::Counter* mDeliveredMetric = nullptr;
    Dia::Observation::Metric::Counter* mDroppedMetric   = nullptr;
};

} // namespace CoW
