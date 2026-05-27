#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaAssetRuntimeVisualDebugger/DiaAssetRuntimeVisualDebugger.h>
#include "Modules/DebugUIModule.h"

namespace Cluiche { namespace AppFlow {

class AssetRuntimeVisualDebuggerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit AssetRuntimeVisualDebuggerModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};
    Dia::AssetRuntime::DiaAssetRuntimeVisualDebugger mDebugger;
    bool mRegistered = false;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
