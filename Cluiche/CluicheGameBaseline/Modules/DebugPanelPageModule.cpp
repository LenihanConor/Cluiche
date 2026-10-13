#include "Modules/DebugPanelPageModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaInput/InputRouter.h>
#include <DiaUI/IUISystem.h>
#include <DiaObservation/Log/Logger.h>
#include <DiaVisualDebugger/Console/CommandCompleter.h>
#include <DiaVisualDebugger/Console/CommandLineParser.h>
#include <DiaVisualDebugger/Console/ConsoleJson.h>
#include <DiaVisualDebugger/Console/ConsoleLineAssembler.h>
#include <DiaVisualDebugger/Console/ConsoleLogRing.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace Cluiche { namespace AppFlow {

namespace
{
    // Canonical group keys from the DiaDebugDomain spec. IDebugDomain::GetGroup()
    // returns a StringCRC, which cannot be reversed to text, so the panel-facing
    // group label is recovered by matching against this table.
    struct GroupEntry
    {
        const char* name;
    };

    const GroupEntry kGroups[] =
    {
        { "CoreDebug"  },
        { "Physics"    },
        { "Animation"  },
        { "Navigation" },
        { "Rendering"  },
        { "Spatial"    },
        { "Entity"     },
        { "AIBehavior" },
    };

    const char* ResolveGroupName(Dia::Core::StringCRC groupId)
    {
        for (const GroupEntry& entry : kGroups)
        {
            if (groupId == Dia::Core::StringCRC(entry.name))
                return entry.name;
        }
        return "CoreDebug";
    }

    // "#rrggbb" — the panel sets each card's --accent CSS variable from this.
    void FormatAccentHex(const Dia::Core::RGBA& colour, char (&out)[8])
    {
        snprintf(out, sizeof(out), "#%02x%02x%02x",
                 static_cast<unsigned int>(colour.R()),
                 static_cast<unsigned int>(colour.G()),
                 static_cast<unsigned int>(colour.B()));
    }
}

const Dia::Core::StringCRC DebugPanelPageModule::kTypeId("DebugPanelPageModule");

struct DebugPanelPageModule::ConsoleRuntime
{
    static const unsigned int kMaxLinesPerFrame = 64;
    static const unsigned int kJsonSize         = 64 * 1024;

    Dia::VisualDebugger::Console::ConsoleLogRing ring;
    uint64_t                                     cursor = 0;
    Dia::VisualDebugger::Console::ConsoleLogLine drain[kMaxLinesPerFrame];
    char                                         json[kJsonSize];
};

DebugPanelPageModule::DebugPanelPageModule(const Dia::Core::StringCRC& instanceId)
    : MainModule(instanceId)
{}

Dia::ApplicationFlow::StartResult DebugPanelPageModule::DoStart()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !ui->HasStarted())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mPageInitialized)
    {
        mPage.InitializePage();
        mPageInitialized = true;
    }

    if (mConsole == nullptr)
    {
        mConsole = new ConsoleRuntime();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&mConsole->ring);
        // RegisterSink is void and ignores the call when Logger::kMaxSinks is full,
        // so this line doubles as a probe checked in DoUpdate.
        DIA_LOG_INFO("Debug", "ConsoleLogRing registered");
        mProbeFramesLeft = 10;
    }

    // Auto-open panel when entering a test stage
    if (!mPanelVisible)
    {
        mPanelVisible = true;
        ui->LoadPage(mPage);
        ui->PushInputMode(Dia::Input::EInputRouting::kGameAndUI);
        mInputModePushed = true;
        DIA_LOG_INFO("Debug", "DebugPanelPageModule: AUTO-OPENED panel on DoStart");
    }

    return Dia::ApplicationFlow::StartResult::kReady;
}

void DebugPanelPageModule::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    DIA_TRACE_ZONE("DebugPanelPageModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    DrainToggleEvents();
    PushDomainStatesToPanel();
    PushConsoleResults();
    PushLogLines();
    CheckLogRingProbe();
}

Dia::ApplicationFlow::StopResult DebugPanelPageModule::DoStop()
{
    // Auto-close panel when leaving test stage (returning to Boot)
    // UIModule is in Boot stage so it's still active and reachable here
    if (mPanelVisible)
    {
        UIModule* ui = mUI.Get();
        if (ui)
        {
            if (mInputModePushed)
            {
                ui->PopInputMode();
                mInputModePushed = false;
            }
            ui->UnloadPage();
            DIA_LOG_INFO("Debug", "DebugPanelPageModule: AUTO-CLOSED panel on DoStop");
        }
        mPanelVisible = false;
    }

    if (mConsole != nullptr)
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&mConsole->ring);
        delete mConsole;
        mConsole = nullptr;
    }
    mProbeFramesLeft = 0;

    delete mPalette;
    mPalette = nullptr;

    mPageInitialized = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void DebugPanelPageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mDomainRegistry.Connect(app);
    mPanelCommands.Connect(app);
    mPanelToggle.Connect(app);
    mPanelSetVisibility.Connect(app);
    mConsoleLines.Connect(app);
    mConsoleResults.Connect(app);
}

void DebugPanelPageModule::OnCommand(const char* domainId, const char* cmd, const char* argsJson)
{
    if (domainId == nullptr || cmd == nullptr)
        return;

    DebugPanelCommandEvent event{};
    event.domainId = Dia::Core::StringCRC(domainId);
    event.cmd      = Dia::Core::StringCRC(cmd);

    if (argsJson != nullptr)
    {
        strncpy_s(event.argsJson, argsJson, sizeof(event.argsJson) - 1);
        event.argsJson[sizeof(event.argsJson) - 1] = '\0';
    }

    DIA_LOG_DEBUG("Debug", "DebugPanelPageModule: command '%s' -> '%s' %s", cmd, domainId, event.argsJson);
    mPanelCommands.Send(event);
}

void DebugPanelPageModule::OnConsoleLine(unsigned int seq, const char* const* parts, unsigned int partCount)
{
    using namespace Dia::VisualDebugger::Console;

    DebugConsoleLineEvent event{};
    event.seq = seq;

    if (!ConsoleLineAssembler::Assemble(parts, partCount, event.text, sizeof(event.text)))
    {
        DIA_LOG_WARNING("Debug", "DebugPanelPageModule: console line %u rejected (too long)", seq);
        char json[256];
        if (ConsoleJson::BuildConsoleResult(seq, 2, static_cast<unsigned>(ParseError::TooLong),
                                            "line too long", json, sizeof(json)))
            PushToPage("consoleResult", json);
        return;
    }

    mHistory.Add(event.text);
    mConsoleLines.Send(event);
}

void DebugPanelPageModule::OnConsoleComplete(const char* token)
{
    using namespace Dia::VisualDebugger::Console;

    Completion items[CommandCompleter::kDefaultMaxResults];
    const unsigned count = CommandCompleter::Complete(token != nullptr ? token : "", items,
                                                      CommandCompleter::kDefaultMaxResults);
    char json[1024];
    if (ConsoleJson::BuildCompletions(items, count, json, sizeof(json)))
        PushToPage("consoleCompletions", json);
}

void DebugPanelPageModule::OnConsoleHistory(bool previous)
{
    using Dia::VisualDebugger::Console::ConsoleJson;

    const char* entry = previous ? mHistory.GetPrevious() : mHistory.GetNext();
    if (entry == nullptr && previous)
        return;   // empty history: leave the input alone

    // entry == nullptr on "next" means we stepped past the newest entry: restore the draft.
    char json[1024];
    if (ConsoleJson::BuildSetInput(entry != nullptr ? entry : "", entry == nullptr, json, sizeof(json)))
        PushToPage("consoleSetInput", json);
}

void DebugPanelPageModule::OnPaletteOpen()
{
    using Dia::VisualDebugger::Console::PaletteIndex;

    if (mPalette == nullptr)
        mPalette = new PaletteIndex();

    if (mDomainRegistry.IsAvailable())
        mPalette->Build(mDomainRegistry.Get());
    else
        DIA_LOG_WARNING("Debug", "DebugPanelPageModule: palette opened before the domain registry is available");

    PushPaletteResults("", PaletteIndex::kTypeCommands | PaletteIndex::kTypeDomains);
}

void DebugPanelPageModule::OnPaletteSearch(const char* query, int mask)
{
    if (mPalette == nullptr)
        return;   // never opened: nothing to search
    PushPaletteResults(query != nullptr ? query : "", mask);
}

void DebugPanelPageModule::PushPaletteResults(const char* query, int mask)
{
    // 50 items x ~600 bytes worst case; heap-free static buffer, Main PU only.
    static char json[32 * 1024];
    if (Dia::VisualDebugger::Console::ConsoleJson::BuildPaletteResults(*mPalette, query, mask, json, sizeof(json)))
        PushToPage("paletteResults", json);
    else
        DIA_LOG_WARNING("Debug", "DebugPanelPageModule: palette results did not fit the JSON buffer");
}

bool DebugPanelPageModule::PushToPage(const char* jsFunction, const char* json)
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPanelVisible || !mPageInitialized)
        return false;

    Dia::UI::IUISystem* uiSystem = ui->GetUISystem();
    if (uiSystem == nullptr || !uiSystem->IsPageLoaded())
        return false;

    uiSystem->CallJSFunction(jsFunction, json);
    return true;
}

void DebugPanelPageModule::PushConsoleResults()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPanelVisible || !mPageInitialized)
        return;
    Dia::UI::IUISystem* uiSystem = ui->GetUISystem();
    if (uiSystem == nullptr || !uiSystem->IsPageLoaded())
        return;   // leave results queued until the panel is shown

    Dia::Core::Containers::DynamicArrayC<
        Dia::ApplicationFlow::Event<DebugConsoleResultEvent>, 16> events;
    mConsoleResults.Consume(events);

    char json[2048];
    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const DebugConsoleResultEvent& r = events[i].payload;
        if (Dia::VisualDebugger::Console::ConsoleJson::BuildConsoleResult(
                r.seq, r.exitCode, r.parseError, r.text, json, sizeof(json)))
            uiSystem->CallJSFunction("consoleResult", json);
    }
}

void DebugPanelPageModule::PushLogLines()
{
    if (mConsole == nullptr)
        return;

    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPanelVisible || !mPageInitialized)
        return;
    Dia::UI::IUISystem* uiSystem = ui->GetUISystem();
    if (uiSystem == nullptr || !uiSystem->IsPageLoaded())
        return;   // cursor stays put; the ring keeps the lines

    uint64_t cursor = mConsole->cursor;
    const unsigned n = mConsole->ring.DrainSince(cursor, mConsole->drain, ConsoleRuntime::kMaxLinesPerFrame);
    if (n == 0)
    {
        mConsole->cursor = cursor;
        return;
    }

    unsigned written = 0;
    if (Dia::VisualDebugger::Console::ConsoleJson::BuildAppendLogLines(
            mConsole->drain, n, mConsole->json, ConsoleRuntime::kJsonSize, &written) && written > 0)
    {
        uiSystem->CallJSFunction("appendLogLines", mConsole->json);
        // If the JSON was truncated, resume right after the last emitted line.
        mConsole->cursor = (written == n) ? cursor : mConsole->drain[written - 1].sequence + 1;
    }
    else
    {
        mConsole->cursor = cursor;   // cannot happen with a 64 KB buffer; skip rather than loop
    }
}

void DebugPanelPageModule::CheckLogRingProbe()
{
    if (mProbeFramesLeft == 0 || mConsole == nullptr)
        return;
    if (--mProbeFramesLeft != 0)
        return;

    if (mConsole->ring.GetTotalWritten() == 0)
        DIA_LOG_WARNING("Debug", "ConsoleLogRing received no log lines (sink table full?)");
}

void DebugPanelPageModule::DrainToggleEvents()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPageInitialized)
        return;

    Dia::Core::Containers::DynamicArrayC<
        Dia::ApplicationFlow::Event<DebugPanelToggleEvent>, 8> events;
    mPanelToggle.Consume(events);

    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        mPanelVisible = !mPanelVisible;
        if (mPanelVisible)
        {
            ui->LoadPage(mPage);
            ui->PushInputMode(Dia::Input::EInputRouting::kGameAndUI);
            mInputModePushed = true;
            DIA_LOG_INFO("Debug", "DebugPanelPageModule: TOGGLE open — pushed kGameAndUI");
        }
        else
        {
            if (mInputModePushed)
            {
                ui->PopInputMode();
                mInputModePushed = false;
                DIA_LOG_INFO("Debug", "DebugPanelPageModule: TOGGLE close — popped input mode");
            }
            ui->UnloadPage();
        }
    }
}

void DebugPanelPageModule::DrainSetVisibilityEvents()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPageInitialized)
        return;

    Dia::Core::Containers::DynamicArrayC<
        Dia::ApplicationFlow::Event<DebugPanelSetVisibilityEvent>, 8> events;
    mPanelSetVisibility.Consume(events);

    for (unsigned int i = 0; i < events.Size(); ++i)
    {
        const bool desiredVisibility = events[i].payload.visible;
        if (desiredVisibility == mPanelVisible)
            continue;

        mPanelVisible = desiredVisibility;
        if (mPanelVisible)
        {
            ui->LoadPage(mPage);
            ui->PushInputMode(Dia::Input::EInputRouting::kGameAndUI);
            mInputModePushed = true;
            DIA_LOG_INFO("Debug", "DebugPanelPageModule: auto-opened panel");
        }
        else
        {
            if (mInputModePushed)
            {
                ui->PopInputMode();
                mInputModePushed = false;
            }
            ui->UnloadPage();
            DIA_LOG_INFO("Debug", "DebugPanelPageModule: auto-closed panel");
        }
    }
}

void DebugPanelPageModule::PushDomainStatesToPanel()
{
    UIModule* ui = mUI.Get();
    if (ui == nullptr || !mPanelVisible)
        return;

    Dia::UI::IUISystem* uiSystem = ui->GetUISystem();
    if (uiSystem == nullptr || !uiSystem->IsPageLoaded())
        return;

    if (!mDomainRegistry.IsAvailable())
        return;

    const Dia::VisualDebugger::DiaDebugDomainRegistry& registry = mDomainRegistry.Get();

    Json::Value root(Json::objectValue);
    registry.VisitAll([&root](Dia::VisualDebugger::IDebugDomain& domain)
    {
        Json::Value state(Json::objectValue);
        domain.GetJSONState(state);

        const char* displayName = domain.GetDisplayName();
        if (displayName == nullptr)
            return;

        char accentHex[8] = {};
        FormatAccentHex(domain.GetAccentColour(), accentHex);

        state["_displayName"] = displayName;
        state["_group"]       = ResolveGroupName(domain.GetGroup());
        state["_description"] = domain.GetDescription() ? domain.GetDescription() : "";
        state["_accent"]      = accentHex;

        root[displayName] = state;
    });

    Json::FastWriter writer;
    writer.omitEndingLineFeed();
    const std::string json = writer.write(root);

    uiSystem->CallJSFunction("updateDomainState", json.c_str());
}

} } // namespace Cluiche::AppFlow

namespace { using DebugPanelPageModule_ = Cluiche::AppFlow::DebugPanelPageModule; }
DIA_MODULE(DebugPanelPageModule_);
DIA_DESCRIBE(DebugPanelPageModule_::kTypeId, "Ultralight HTML debug panel overlay: domain accordions, drawer toggles, live stats, scale sliders.");

#endif // DIA_DEBUG
