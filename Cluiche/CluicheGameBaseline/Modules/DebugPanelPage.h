#pragma once

#ifdef DIA_DEBUG

#include <DiaUI/Page.h>

namespace Dia { namespace UI { class BoundMethodArgs; } }

namespace Cluiche { namespace AppFlow {

// C++ shell for debug-panel.html (DiaDebugPanel, SD-001).
//
// The page exposes a single bound method to JavaScript:
//
//   app.onCommand(domainId, cmd, argsJson)
//
// Three separate string arguments are used deliberately rather than one
// serialised JSON blob: Ultralight marshals every JS string through
// Dia::Core::Containers::String64, which truncates at 63 characters. A single
// {domainId, cmd, args} envelope routinely exceeds that; the three fields
// individually do not.
//
// State flows the other way via IUISystem::CallJSFunction("updateDomainState", ...)
// which DebugPanelPageModule drives once per frame.
//
// ---------------------------------------------------------------------------
// Console JS contract (panel-console.md). Every string argument is <= 63 chars.
//
// JS -> C++ (bound methods on `app`):
//   app.onConsoleLine(seq, part0, part1, part2, part3)
//       seq: number; part0..part3: the typed line split into <= 4 chunks of
//       <= 60 chars (unused chunks "" or omitted). Joined line <= 240 chars.
//   app.onConsoleComplete(token)
//       token: first token of the input (command name prefix, <= 63 chars).
//   app.onConsoleHistory(direction)
//       direction: "prev" (Up) or "next" (Down).
//
// C++ -> JS (global functions the page must define; arg is one JSON string):
//   consoleResult(json)       {"seq":N,"exitCode":N,"parseError":N,"text":"..."}
//   appendLogLines(json)      {"lines":[{"t":secs,"level":"INFO|WARN|ERROR|DEBUG",
//                                        "channel":"...","msg":"..."}]}
//   consoleCompletions(json)  {"items":[{"name":"...",
//                                        "rank":"exact|prefix|contains|subsequence"}]}
//   consoleSetInput(json)     {"text":"...","restore":false}; restore:true means
//                             "past the newest history entry: restore your draft".
// History and the log ring live in DebugPanelPageModule, so they survive the
// panel being hidden and shown again. appendLogLines is pushed only while the
// page is loaded; lines logged while hidden are delivered on the next show.
// ---------------------------------------------------------------------------
class DebugPanelPage : public Dia::UI::Page
{
public:
    struct ICallbacks
    {
        virtual ~ICallbacks() = default;

        // domainId — IDebugDomain::GetDomainId() source string, or "__global__"
        //            for panel-wide commands (e.g. the global scale slider).
        // cmd      — "toggle", "setScale", ...
        // argsJson — compact JSON object, never null (defaults to "{}").
        virtual void OnCommand(const char* domainId,
                               const char* cmd,
                               const char* argsJson) = 0;

        // Console bindings (see contract above). Strings are valid for the call only.
        virtual void OnConsoleLine(unsigned int seq, const char* const* parts, unsigned int partCount) = 0;
        virtual void OnConsoleComplete(const char* token) = 0;
        virtual void OnConsoleHistory(bool previous) = 0;
    };

    explicit DebugPanelPage(ICallbacks* callbacks);

    void InitializePage();

private:
    void OnCommand_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleLine_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleComplete_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleHistory_JS(const Dia::UI::BoundMethodArgs& args);

    ICallbacks* mCallbacks;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
