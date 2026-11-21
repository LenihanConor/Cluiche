#include "Modules/BusProbeModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC BusProbeModule::kTypeId("BusProbeModule");

BusProbeModule::BusProbeModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult BusProbeModule::DoStart()
{
    using Dia::ApplicationFlow::StartResult;

    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return StartResult::kLoading;

    Dia::MessageBus::Bus& bus = host->GetBus();
    bus.RegisterType<BusProbeEvent, 16>();
    bus.RegisterProducer<BusProbeEvent>(GetInstanceId());
    mSubscription = bus.Subscribe<BusProbeEvent>(GetInstanceId(),
        [this](const BusProbeEvent&) {
            ++mDelivered;
            if (mDeliveredMetric)
                mDeliveredMetric->Inc();
        });

    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    mPostedMetric    = metrics.RegisterCounter(Dia::Core::StringCRC("cow.bus_probe.posted"));
    mDeliveredMetric = metrics.RegisterCounter(Dia::Core::StringCRC("cow.bus_probe.delivered"));
    mDroppedMetric   = metrics.RegisterCounter(Dia::Core::StringCRC("cow.bus_probe.dropped"));

    mPosted = 0;
    mDelivered = 0;
    DIA_LOG_INFO("CoW", "BusProbeModule: subscribed to BusProbeEvent");
    return StartResult::kReady;
}

void BusProbeModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    Cluiche::AppFlow::MessageBusHostModule* host = mBus.Get();
    if (!host)
        return;

    BusProbeEvent event;
    event.sequence = mPosted;
    if (host->GetBus().Broadcast(event))
    {
        ++mPosted;
        if (mPostedMetric)
            mPostedMetric->Inc();
    }
    else if (mDroppedMetric)
    {
        mDroppedMetric->Inc();
    }
}

Dia::ApplicationFlow::StopResult BusProbeModule::DoStop()
{
    mSubscription = Dia::MessageBus::BusSubscriptionHandle();
    mPostedMetric = nullptr;
    mDeliveredMetric = nullptr;
    mDroppedMetric = nullptr;
    DIA_LOG_INFO("CoW", "BusProbeModule: stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using BusProbeModule_ = CoW::BusProbeModule; }
DIA_MODULE(BusProbeModule_);
DIA_DESCRIBE(BusProbeModule_::kTypeId, "Posts one BusProbeEvent per tick so the message bus has live traffic.");
