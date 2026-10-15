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
//
// Palette JS contract (panel-palette.md). Every string argument is <= 63 chars.
//   app.onPaletteOpen()
//       Palette opened: C++ rebuilds its index from the live registries and pushes
//       paletteResults for the empty query, mask "3".
//   app.onPaletteSearch(query, mask)
//       query: <= 60 chars; mask: "1" (commands), "2" (domains) or "3" (both).
//   paletteResults(json)      {"total":N,"counts":{"all":N,"commands":N,"domains":N},
//                              "items":[{"type":"command|domain","name":"...",
//                              "description":"...","category":"...","example":"...",
//                              "domainId":"...","action":"fill|reveal","actionText":"...",
//                              "hl":"0101..."}]}
//       counts are true totals for the query; items are the ranked results for the
//       mask (<= 50, truncated if the buffer fills). action "fill": put actionText
//       in the console input; "reveal": reveal the domain whose id is actionText.
//       hl has one '0'/'1' per character of name (1 = matched by the query).
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

        // Palette bindings. mask: 1 = commands, 2 = domains, 3 = both.
        virtual void OnPaletteOpen() = 0;
        virtual void OnPaletteSearch(const char* query, int mask) = 0;
    };

    explicit DebugPanelPage(ICallbacks* callbacks);

    void InitializePage();

private:
    void OnCommand_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleLine_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleComplete_JS(const Dia::UI::BoundMethodArgs& args);
    void OnConsoleHistory_JS(const Dia::UI::BoundMethodArgs& args);
    void OnPaletteOpen_JS(const Dia::UI::BoundMethodArgs& args);
    void OnPaletteSearch_JS(const Dia::UI::BoundMethodArgs& args);

    ICallbacks* mCallbacks;
};

} } // namespace Cluiche::AppFlow

#endif // DIA_DEBUG
