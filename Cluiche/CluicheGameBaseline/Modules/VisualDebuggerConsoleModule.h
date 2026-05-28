#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaVisualDebuggerConsole/DiaVisualDebuggerConsole.h>
#include "Modules/VisualDebuggerModule.h"
#include "Modules/DebugUIModule.h"

namespace Cluiche { namespace AppFlow {

class VisualDebuggerConsoleModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
    static constexpr const char* kDescription = "ImGui debug console — layer toggles, log tail, DiaAPI input";
    explicit VisualDebuggerConsoleModule(const Dia::Core::StringCRC& instanceId);

    void ToggleConsole() { mConsole.Toggle(); }
    bool IsConsoleVisible() const { return mConsole.IsVisible(); }

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    Dia::ApplicationFlow::ModuleRef<DebugUIModule> mDebugUI{this, Dia::Core::StringCRC("DebugUI")};

    Dia::Debug::DiaVisualDebuggerConsole mConsole;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
