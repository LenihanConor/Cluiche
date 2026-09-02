#include <DiaMessageBus/MessageBusModule.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Health/HealthRegistry.h>

namespace Dia::MessageBus {

const Dia::Core::StringCRC MessageBusModule::kInstanceId("message-bus-module");

MessageBusModule::MessageBusModule()
    : SimModule(kInstanceId)
{
}

Bus& MessageBusModule::GetBus()
{
    return mBus;
}

const Bus& MessageBusModule::GetBus() const
{
    return mBus;
}

#ifdef DIA_DEBUG
const LedgerHistory& MessageBusModule::GetLedgerHistory() const
{
    return mLedgerHistory;
}
#endif

Dia::ApplicationFlow::StartResult MessageBusModule::DoStart()
{
    mBus.Initialize();

    if (!mBus.IsRouterRegistered(Bus::kBroadcastRouterId))
    {
        DIA_LOG_WARNING("DiaMessageBus",
            "MessageBusModule::DoStart: BroadcastRouter failed to register");
    }
    else
    {
        DIA_LOG_INFO("DiaMessageBus", "MessageBusModule::DoStart: Bus initialized, BroadcastRouter registered");
    }

    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealthReporter);

    return Dia::ApplicationFlow::StartResult::kReady;
}

void MessageBusModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    mBus.Update();
    mHealthReporter.Check();
#ifdef DIA_DEBUG
    mLedgerHistory.Push(mBus.GetLastTickLedger());
#endif
}

Dia::ApplicationFlow::StopResult MessageBusModule::DoStop()
{
    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);
    DIA_LOG_INFO("DiaMessageBus", "MessageBusModule::DoStop");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Dia::MessageBus
