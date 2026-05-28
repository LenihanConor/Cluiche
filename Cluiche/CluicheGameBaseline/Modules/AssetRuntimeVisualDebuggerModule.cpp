#include "Modules/AssetRuntimeVisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include "Modules/VisualDebuggerModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC AssetRuntimeVisualDebuggerModule::kTypeId("AssetRuntimeVisualDebuggerModule");

AssetRuntimeVisualDebuggerModule::AssetRuntimeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult AssetRuntimeVisualDebuggerModule::DoStart()
{
    if (!mDebugUI.Get())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mRegistered)
    {
        if (auto* mgr = VisualDebuggerModule::GetStaticLayerManager())
        {
            mgr->Register(&mDebugger, 50, Dia::Core::StringCRC("AssetRuntimeTestStage"));
            mRegistered = true;
        }
        else
        {
            return Dia::ApplicationFlow::StartResult::kLoading;
        }
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void AssetRuntimeVisualDebuggerModule::DoUpdate(float /*dt*/)
{
}

Dia::ApplicationFlow::StopResult AssetRuntimeVisualDebuggerModule::DoStop()
{
    if (mRegistered)
    {
        if (auto* mgr = VisualDebuggerModule::GetStaticLayerManager())
            mgr->Unregister(mDebugger.GetLayerName());
        mRegistered = false;
    }

    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::AppFlow

namespace { using AssetRuntimeVisualDebuggerModule_ = Cluiche::AppFlow::AssetRuntimeVisualDebuggerModule; }
DIA_MODULE(AssetRuntimeVisualDebuggerModule_);

#endif // DIA_DEBUG
