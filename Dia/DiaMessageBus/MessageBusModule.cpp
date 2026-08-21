#include <DiaMessageBus/MessageBusModule.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia::MessageBus {

const Dia::Core::StringCRC MessageBusModule::kInstanceId("message-bus-module");

MessageBusModule::MessageBusModule()
    : Module(kInstanceId)
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

    return Dia::ApplicationFlow::StartResult::kReady;
}

void MessageBusModule::DoUpdate(float /*deltaTime*/)
{
    mBus.Update();
#ifdef DIA_DEBUG
    mLedgerHistory.Push(mBus.GetLastTickLedger());
#endif
}

Dia::ApplicationFlow::StopResult MessageBusModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Dia::MessageBus
