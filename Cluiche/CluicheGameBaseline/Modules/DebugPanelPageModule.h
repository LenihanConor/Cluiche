#pragma once

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>
#include <DiaDebugServer/QueryRegistry.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/EventStreamReader.h>
#include <DiaStreams/ServiceStreamReader.h>
#include <DiaVisualDebugger/Console/PanelController.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/ServiceKeyResolver.h>
#include <DiaValueSchema/ValueSchemaService.h>

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
    , private Dia::VisualDebugger::Console::IPanelPusher
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
    void OnValueSchemaGet(const char* prefix) override;

    // Schema keys (DiaValueSchema Feature 3, Task 4b). The resolver below is PRIVATE to this Main-PU module and is
    // used only to stamp "schemaVersion" onto every domain state that has a "controls" array (see
    // Console::BuildDomainStatesRoot), which is where debug-panel.html reads it.
    // It is deliberately not exposed: no getter, no static, no singleton. A module on another PU (e.g. a Sim
    // domain that wants SchemaControl key checks) builds its OWN resolver from the stream, the normal pattern:
    //   ServiceStreamReader<ValueSchemaService> mSchema{this, "ValueSchemaService"};
    //   ServiceKeyResolver<ValueSchemaService>  mResolver;   // SetService(mSchema.IsAvailable() ? &mSchema.Get() : nullptr)
    //   SchemaControl sc(&mResolver);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    void PushDomainStatesToPanel();
    void RefreshSchemaService();
    void DrainToggleEvents();
    void DrainSetVisibilityEvents();

    // "debug.domain_state" (Console::QueryDomainState) on the DebugServer QueryRegistry. Registered from DoUpdate once
    // the DebugServer stream is committed, removed in DoStop. Runs on the Main PU, like every other query.
    Dia::DebugServer::QueryRegistry* FindQueryRegistry();
    void RegisterDomainStateQuery();
    void UnregisterDomainStateQuery();
    bool mDomainStateQueryRegistered = false;

    // Console (panel-console.md). PushToPage returns false when the page cannot receive JS calls.
    bool PushToPage(const char* jsFunction, const char* json);
    void PushConsoleResults();
    void PushLogLines();
    void CheckLogRingProbe();

    // IPanelPusher: forwards PanelController results to the page.
    bool Push(const char* jsFunction, const char* json) override { return PushToPage(jsFunction, json); }

    // Runs "valueschema.get" (and any other panel query) on the DebugServer QueryRegistry.
    // Called only from mPanel.Process() on the Main PU, the same thread that ticks the DebugServer host.
    struct QueryBridge : public Dia::VisualDebugger::Console::IPanelQueryExecutor
    {
        explicit QueryBridge(DebugPanelPageModule& owner) : mOwner(owner) {}
        bool ExecuteQuery(const char* queryName, const char* argsJson, char* outJson, unsigned capacity) override;
        DebugPanelPageModule& mOwner;
    };
    QueryBridge mQueryBridge{*this};

    // The JS callbacks above run from inside a script that Ultralight is executing, so they only record
    // a request in mPanel; mPanel.Process() carries it out from DoUpdate (calling CallJSFunction from a
    // callback re-enters the JS engine, which crashed on Ctrl+P).
    Dia::VisualDebugger::Console::PanelController mPanel;

    DebugPanelPage mPage{this};
    bool           mPanelVisible         = false;
    bool           mPageInitialized      = false;
    bool           mInputModePushed      = false;

    Dia::ApplicationFlow::ModuleRef<UIModule> mUI{this};

    // ValueSchemaModule::kServiceStreamId; looked up by id each frame (optional service, so no
    // ServiceStreamReader: that would make this module wait for a provider some apps do not have).
    static constexpr const char* kValueSchemaStreamId = "ValueSchemaService";
    Dia::VisualDebugger::ServiceKeyResolver<Dia::ValueSchema::ValueSchemaService> mKeyResolver;

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

    unsigned int mProbeFramesLeft = 0;   // frames until the ring probe is checked; 0 = done
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
