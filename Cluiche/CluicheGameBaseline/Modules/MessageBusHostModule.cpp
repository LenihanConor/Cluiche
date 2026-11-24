#include "Modules/MessageBusHostModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Health/HealthRegistry.h>

#ifdef DIA_DEBUG
#include <DiaMessageBus/MessageBusDebugDomain.h>
#include "Modules/VisualDebuggerModule.h"
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC MessageBusHostModule::kTypeId("MessageBusHostModule");
const Dia::Core::StringCRC MessageBusHostModule::kInstanceId("message-bus-module");

MessageBusHostModule::MessageBusHostModule()
    : SimModule(kInstanceId)
{
}

MessageBusHostModule::MessageBusHostModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

// Out of line: MessageBusDebugDomain is only forward-declared in the header.
MessageBusHostModule::~MessageBusHostModule()
{
    // No-op when DoStop already unregistered; prevents a dangling reporter if destroyed without a Stop.
    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);
}

Dia::MessageBus::Bus& MessageBusHostModule::GetBus()
{
    return mBus;
}

const Dia::MessageBus::Bus& MessageBusHostModule::GetBus() const
{
    return mBus;
}

#ifdef DIA_DEBUG
const Dia::MessageBus::LedgerHistory& MessageBusHostModule::GetLedgerHistory() const
{
    return mLedgerHistory;
}
#endif

Dia::ApplicationFlow::StartResult MessageBusHostModule::DoStart()
{
    mBus.Initialize();

    if (!mBus.IsRouterRegistered(Dia::MessageBus::Bus::kBroadcastRouterId))
    {
        DIA_LOG_WARNING("DiaMessageBus",
            "MessageBusModule::DoStart: BroadcastRouter failed to register");
    }
    else
    {
        DIA_LOG_INFO("DiaMessageBus", "MessageBusModule::DoStart: Bus initialized, BroadcastRouter registered");
    }

    Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealthReporter);

#ifdef DIA_DEBUG
    mDebugDomain = std::make_unique<Dia::MessageBus::MessageBusDebugDomain>(mBus, mLedgerHistory);
    mDomainRegistered = false;
#endif
    return Dia::ApplicationFlow::StartResult::kReady;
}

void MessageBusHostModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    mBus.Update();
    mHealthReporter.Check();
#ifdef DIA_DEBUG
    mLedgerHistory.Push(mBus.GetLastTickLedger());
    TryRegisterDebugDomain();
#endif
}

Dia::ApplicationFlow::StopResult MessageBusHostModule::DoStop()
{
#ifdef DIA_DEBUG
    UnregisterDebugDomain();
    mDebugDomain.reset();
#endif
    Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealthReporter);
    DIA_LOG_INFO("DiaMessageBus", "MessageBusModule::DoStop");
    return Dia::ApplicationFlow::StopResult::kDone;
}

#ifdef DIA_DEBUG
void MessageBusHostModule::TryRegisterDebugDomain()
{
    if (mDomainRegistered || !mDebugDomain)
        return;
    if (VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
    {
        vd->RegisterDomain(*mDebugDomain);
        mDomainRegistered = true;
        DIA_LOG_INFO("DiaMessageBus", "MessageBusHostModule: bus debug domain registered with the visual debugger");
    }
}

void MessageBusHostModule::UnregisterDebugDomain()
{
    if (!mDomainRegistered || !mDebugDomain)
        return;
    if (VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
        vd->UnregisterDomain(*mDebugDomain);
    mDomainRegistered = false;
    DIA_LOG_INFO("DiaMessageBus", "MessageBusHostModule: bus debug domain unregistered");
}
#endif

} } // namespace Cluiche::AppFlow

namespace { using MessageBusHostModule_ = Cluiche::AppFlow::MessageBusHostModule; }
DIA_MODULE(MessageBusHostModule_);
DIA_DESCRIBE(MessageBusHostModule_::kTypeId, "Hosts the DiaMessageBus Bus for a game and shows it in the debug panel.");
