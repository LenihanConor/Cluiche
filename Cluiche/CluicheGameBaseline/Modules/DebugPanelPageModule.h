#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>

#include "Modules/DebugPanelPage.h"
#include "Modules/UIModule.h"
#include "Types/DebugPanelCommandEvent.h"
#include "Types/DebugPanelToggleEvent.h"
#include "Types/DebugPanelSetVisibilityEvent.h"

namespace Cluiche { namespace AppFlow {

// DebugPanelPageModule — Main PU owner of the DiaDebugPanel Ultralight page.
//
// Reads the DomainRegistry ServiceStream (provided by VisualDebuggerModule on
// the Sim PU), serialises every registered IDebugDomain's GetJSONState() once
// per frame and pushes it to JS via CallJSFunction("updateDomainState", json).
//
// Commands coming back from JS are forwarded over the DebugPanelCommand
// EventStream (Main -> Sim); VisualDebuggerModule drains them on the Sim
// thread and dispatches to IDebugDomain::OnCommand. ModuleRef cannot be used
// for that hop because it only resolves modules inside the same PU.
class DebugPanelPageModule
    : public Dia::ApplicationFlow::Module
    , public DebugPanelPage::ICallbacks
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "DiaDebugPanel Ultralight overlay";

    // Domain id reserved for panel-wide commands (global debug scale).
    static constexpr const char* kGlobalDomainId = "__global__";

    explicit DebugPanelPageModule(const Dia::Core::StringCRC& instanceId);

    const Dia::Core::StringCRC* GetRequiredModuleTypeIds(unsigned int& outCount) const override
    {
        static const Dia::Core::StringCRC ids[] = { UIModule::kTypeId };
        outCount = 1;
        return ids;
    }

    // DebugPanelPage::ICallbacks — invoked from the Ultralight JS bridge.
    void OnCommand(const char* domainId, const char* cmd, const char* argsJson) override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void PushDomainStatesToPanel();
    void DrainToggleEvents();
    void DrainSetVisibilityEvents();

    DebugPanelPage mPage{this};
    bool           mPanelVisible    = false;
    bool           mPageInitialized = false;

    Dia::ApplicationFlow::ModuleRef<UIModule> mUI{this};

    Dia::ApplicationFlow::ServiceStreamReader<Dia::VisualDebugger::DiaDebugDomainRegistry>
        mDomainRegistry{this, "DomainRegistry"};

    Dia::ApplicationFlow::EventStreamWriter<DebugPanelCommandEvent>
        mPanelCommands{this, "DebugPanelCommand"};

    Dia::ApplicationFlow::EventStreamReader<DebugPanelToggleEvent>
        mPanelToggle{this, "DebugPanelToggle"};

    Dia::ApplicationFlow::EventStreamReader<DebugPanelSetVisibilityEvent>
        mPanelSetVisibility{this, "DebugPanelSetVisibility"};
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
