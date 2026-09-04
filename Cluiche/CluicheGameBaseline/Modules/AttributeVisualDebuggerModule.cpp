#include "Modules/AttributeVisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include "Modules/EntityModule.h"
#include "Modules/VisualDebuggerModule.h"

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AttributeVisualDebuggerModule::kTypeId("AttributeVisualDebuggerModule");

AttributeVisualDebuggerModule::AttributeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

AttributeVisualDebuggerModule::~AttributeVisualDebuggerModule() = default;

Dia::ApplicationFlow::StartResult AttributeVisualDebuggerModule::DoStart()
{
    RegisterDebugDomain();
    return Dia::ApplicationFlow::StartResult::kReady;
}

Dia::ApplicationFlow::StopResult AttributeVisualDebuggerModule::DoStop()
{
    UnregisterDebugDomain();
    return Dia::ApplicationFlow::StopResult::kDone;
}

void AttributeVisualDebuggerModule::RegisterDebugDomain()
{
    auto* entityMod = mEntityRef.Get();
    auto* vdMod     = mVisualDebuggerRef.Get();
    if (!entityMod || !vdMod) return;

    mDebugDomain = std::make_unique<Dia::AttributeVisualDebugger::AttributeVisualDebugger>(
        entityMod->GetDomain());

    vdMod->RegisterDomain(*mDebugDomain);
}

void AttributeVisualDebuggerModule::UnregisterDebugDomain()
{
    if (!mDebugDomain) return;

    // Only unregister if VisualDebuggerModule is still active (concurrent stop).
    if (auto* vdMod = mVisualDebuggerRef.Get())
        vdMod->UnregisterDomain(*mDebugDomain);

    mDebugDomain.reset();
}

} } // namespace Cluiche::AppFlow

namespace { using AttributeVisualDebuggerModule_ = Cluiche::AppFlow::AttributeVisualDebuggerModule; }
DIA_MODULE(AttributeVisualDebuggerModule_);
DIA_DESCRIBE(AttributeVisualDebuggerModule_::kTypeId, "Attribute inspector: values, ranges, modifier stack for the selection");

#endif // DIA_DEBUG
