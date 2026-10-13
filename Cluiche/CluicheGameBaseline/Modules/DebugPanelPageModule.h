#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaVisualDebugger/Console/ConsoleHistory.h>
#include <DiaVisualDebugger/Console/PaletteIndex.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>

#include "Modules/DebugPanelPage.h"
#include "Modules/UIModule.h"
#include "Types/DebugConsoleLineEvent.h"
#include "Types/DebugConsoleResultEvent.h"
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
    : public Dia::ApplicationFlow::MainModule
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
    void OnConsoleLine(unsigned int seq, const char* const* parts, unsigned int partCount) override;
    void OnConsoleComplete(const char* token) override;
    void OnConsoleHistory(bool previous) override;
    void OnPaletteOpen() override;
    void OnPaletteSearch(const char* query, int mask) override;

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void PushDomainStatesToPanel();
    void DrainToggleEvents();
    void DrainSetVisibilityEvents();

    // Console (panel-console.md). PushToPage returns false when the page cannot receive JS calls.
    bool PushToPage(const char* jsFunction, const char* json);
    void PushConsoleResults();
    void PushLogLines();
    void CheckLogRingProbe();
    void PushPaletteResults(const char* query, int mask);

    DebugPanelPage mPage{this};
    bool           mPanelVisible         = false;
    bool           mPageInitialized      = false;
    bool           mInputModePushed      = false;

    Dia::ApplicationFlow::ModuleRef<UIModule> mUI{this};

    Dia::ApplicationFlow::ServiceStreamReader<Dia::VisualDebugger::DiaDebugDomainRegistry>
        mDomainRegistry{this, "DomainRegistry"};

    Dia::ApplicationFlow::EventStreamWriter<DebugPanelCommandEvent>
        mPanelCommands{this, "DebugPanelCommand"};

    Dia::ApplicationFlow::EventStreamReader<DebugPanelToggleEvent>
        mPanelToggle{this, "DebugPanelToggle"};

    Dia::ApplicationFlow::EventStreamReader<DebugPanelSetVisibilityEvent>
        mPanelSetVisibility{this, "DebugPanelSetVisibility"};

    Dia::ApplicationFlow::EventStreamWriter<DebugConsoleLineEvent>
        mConsoleLines{this, "DebugConsoleLine"};

    Dia::ApplicationFlow::EventStreamReader<DebugConsoleResultEvent>
        mConsoleResults{this, "DebugConsoleResult"};

    // Heap-owned: ConsoleLogRing is about 175 KB. Created in DoStart, freed in DoStop.
    struct ConsoleRuntime;
    ConsoleRuntime* mConsole = nullptr;

    // Lives in the module (not the page) so it survives panel hide/show.
    Dia::VisualDebugger::Console::ConsoleHistory mHistory;

    // Heap-owned: PaletteIndex is non-copyable and holds a snapshot. Created on first open, freed in DoStop.
    Dia::VisualDebugger::Console::PaletteIndex* mPalette = nullptr;

    unsigned int mProbeFramesLeft = 0;   // frames until the ring probe is checked; 0 = done
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
