#include "Modules/MessageBusHostModule.h"
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#ifdef DIA_DEBUG
#include <DiaMessageBus/MessageBusDebugDomain.h>
#include "Modules/VisualDebuggerModule.h"
#endif

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC MessageBusHostModule::kTypeId("MessageBusHostModule");

MessageBusHostModule::MessageBusHostModule(const Dia::Core::StringCRC& instanceId)
    : MessageBusModule(instanceId)
{
}

// Out of line: MessageBusDebugDomain is only forward-declared in the header.
MessageBusHostModule::~MessageBusHostModule() = default;

Dia::ApplicationFlow::StartResult MessageBusHostModule::DoStart()
{
    const Dia::ApplicationFlow::StartResult result = MessageBusModule::DoStart();
#ifdef DIA_DEBUG
    mDebugDomain = std::make_unique<Dia::MessageBus::MessageBusDebugDomain>(*this);
    mDomainRegistered = false;
#endif
    return result;
}

void MessageBusHostModule::DoUpdate(const Dia::SimTime::SimTimeContext& ctx)
{
    MessageBusModule::DoUpdate(ctx);
#ifdef DIA_DEBUG
    TryRegisterDebugDomain();
#endif
}

Dia::ApplicationFlow::StopResult MessageBusHostModule::DoStop()
{
#ifdef DIA_DEBUG
    UnregisterDebugDomain();
    mDebugDomain.reset();
#endif
    return MessageBusModule::DoStop();
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
    }
}

void MessageBusHostModule::UnregisterDebugDomain()
{
    if (!mDomainRegistered || !mDebugDomain)
        return;
    if (VisualDebuggerModule* vd = mVisualDebuggerRef.Get())
        vd->UnregisterDomain(*mDebugDomain);
    mDomainRegistered = false;
}
#endif

} } // namespace Cluiche::AppFlow

namespace { using MessageBusHostModule_ = Cluiche::AppFlow::MessageBusHostModule; }
DIA_MODULE(MessageBusHostModule_);
DIA_DESCRIBE(MessageBusHostModule_::kTypeId, "Hosts the DiaMessageBus Bus for a game and shows it in the debug panel.");
