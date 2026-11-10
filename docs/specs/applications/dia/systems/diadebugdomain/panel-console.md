# Feature Spec: Panel Console

**Parent:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
**Mockup:** @docs/specs/applications/cow/systems/f1-harness/mockups/debug-console.html (visual reference for the output area; task 1 updates it to show the strip-in-panel layout)
**Status:** Done
**Plan:** @docs/specs/applications/dia/systems/diadebugdomain/panel-console.plan.md
**Depends on:** @docs/specs/applications/dia/systems/diaapi/command-unregister.md (thread-safe registry, uncapped list)

## Summary

Upgrade the command strip that already exists in DiaDebugPanel (`~`) into a usable console: results shown in the panel, `--flags` and `key=value` arguments, command history, autocomplete, and a live log view. This **replaces** the separate "DiaDebugConsole" module the earlier draft proposed. There is no second window, no second toggle key, and no new module: the logic lives in `Dia/DiaVisualDebugger/Console/` and the UI is an extension of `debug-panel.html`.

## Problem

Today (`VisualDebuggerModule::ExecuteConsoleCommand`, `debug-panel.html` command strip):

- Input is split on spaces and everything becomes a positional argument, so `--flag` and `key=value` never reach a command as flags or named args, even though `DiaAPI/Parser/ArgumentParser` already parses them.
- The strip shows `->` after Enter. The exit code and anything the command logged only reach the log file.
- Commands longer than 52 characters are refused (`MAX_CMD_TEXT` in `debug-panel.html`). Ultralight marshals each JS string argument through `String64` (63 characters), and the strip sends the text inside a JSON object, so a long command or path is rejected.
- No history, no autocomplete, no way to see what happened.
- The parsing and execution logic lives in the CluicheGameBaseline, so a game cannot reuse it and it cannot be unit-tested without the whole module.

## Goals

1. Typed text up to 240 characters is accepted, tokenized (double quotes group a token) and parsed by `ArgumentParser` into `CommandArgs`.
2. The panel shows, per executed command: the text, the exit code (0 green, non-zero red, 3 shown as "not found"), and the log lines produced since.
3. Up/Down recall history (50 entries, survives panel close/open, not app restart).
4. Tab autocompletes command names (top 5, ranked prefix > contains > subsequence).
5. A log view in the panel shows the last 500 log lines with a level filter (All / Info / Warn / Error).
6. All logic (tokenizing, history, matching, log ring) is plain C++ with unit tests, independent of Ultralight.
7. The strip and log view work in both CluicheTest and CoW.

## Non-Goals

- A separate console window, module or key. `~` toggles the panel as it does today.
- Scripting, batch execution, aliases or macros.
- Capturing a command's stdout. Commands communicate through their exit code and `DIA_LOG_*`.
- Log export to file.
- Panel/data-panel registration (DiaDebugDataPanel was dropped).

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `CommandLineParser::Parse("foo a \"b c\" --v key=val")` yields name `foo`, positional `a` and `b c`, flag `v`, named `key=val` | GoogleTest |
| AC-2 | Empty, whitespace-only, unterminated quote and over-long (>240 chars) input return a typed error and never crash | GoogleTest |
| AC-3 | `ConsoleExecutor::Execute` returns the command's exit code, and 3 for an unknown command | GoogleTest |
| AC-4 | `ConsoleHistory` keeps the last 50 entries, Up/Down walk them, `Reset()` returns to the end, consecutive duplicates are not stored | GoogleTest |
| AC-5 | `FuzzyMatcher` ranks exact prefix > contains > subsequence, ties alphabetical; `"bstrfc"` matches `"bus_traffic.start"` | GoogleTest |
| AC-6 | `CommandCompleter` returns at most 5 matches from the live registry and reflects commands registered or unregistered since the last call | GoogleTest |
| AC-7 | `ConsoleLogRing` registered as a logger sink stores the last 500 lines; entries logged from several threads are all kept in order or dropped oldest-first, never torn | GoogleTest + `dia check sanitizer` |
| AC-8 | `DrainSince(cursor)` returns only new lines and survives wrap-around of the ring | GoogleTest |
| AC-9 | Typing `debug.layer.list` in the panel in CluicheTest shows exit code 0 and the log lines it produced | Manual: `dia run cluichetest` |
| AC-10 | Typing an unknown command shows "not found" (exit 3) in red | Manual |
| AC-11 | Up arrow recalls the previous command; Tab completes `debug.l` to `debug.layer.*` suggestions | Manual |
| AC-12 | Closing and reopening the panel (`~`) keeps history and log content | Manual |
| AC-13 | Same behaviour in CoW (`dia run cow`) after the CoW wiring task | Manual |
| AC-14 | Panel visual layout matches the updated mockup | Visual comparison against mockup |
| AC-15 | `DebugConsoleResult` crosses the Sim to Main stream intact (text, exit code, sequence) | GoogleTest on the event struct round trip |
| AC-16 | A 200-character command typed in the panel reaches `ExecuteCommand` unchanged (no truncation at the JS bridge) | Manual + GoogleTest on `ConsoleLineAssembler` |

## Design

### Where things live

```
Dia/DiaVisualDebugger/Console/        (existing module, already depends on DiaAPI)
  CommandLineParser.h/.cpp   tokenize + ArgumentParser -> ParsedCommand
  ConsoleExecutor.h/.cpp     ParsedCommand -> ExecuteCommand -> ConsoleResult
  ConsoleLineAssembler.h/.cpp   join <=4 chunks into one line (<=240 chars)
  ConsoleHistory.h/.cpp
  FuzzyMatcher.h/.cpp        shared with panel-palette.md
  CommandCompleter.h/.cpp    FuzzyMatcher over ListCommandInfos()
  ConsoleLogRing.h/.cpp      Observation::Log::ISink, thread-safe ring

Cluiche/CluicheGameBaseline/            (wiring only)
  Types/DebugConsoleLineEvent.h          Main -> Sim event (command text)
  Types/DebugConsoleResultEvent.h        Sim -> Main event
  Modules/VisualDebuggerModule.cpp       ExecuteConsoleCommand becomes a call to ConsoleExecutor
  Modules/DebugPanelPageModule.*         owns history + log ring, completes, pushes to JS
  Modules/DebugPanelPage.*               new JS bindings

Cluiche/Assets/CluicheTest/Global/Presentation/UI/debug-panel.html   strip + log view
```

Following the layer rule: the logic is reusable by any game, so it is Dia; the stream wiring stays in the baseline.

### Types

```cpp
namespace Dia::VisualDebugger::Console {

struct ParsedCommand {
    Dia::Core::StringCRC name;
    Dia::API::CommandArgs args;
    char storage[512];            // owns the token characters args points into
};

enum class ParseError { None, Empty, TooLong, UnterminatedQuote, TooManyArgs };

ParseError ParseCommandLine(const char* text, ParsedCommand& out);

struct ConsoleResult {
    int  exitCode;                // 0 ok, 3 not found, 2 bad arguments, else command-specific
    ParseError parseError;
};

ConsoleResult ExecuteConsoleCommand(const char* text);   // parse + Dia::API::ExecuteCommand

} // namespace
```

`ParseCommandLine` builds an `argv` (`{"dia", name, tokens...}`) in `storage` and calls `ArgumentParser::ParseArguments`, so the flag grammar is the CLI's.

Amended after task 2 (2026-10-13), where the code differed from the first draft:
- The CLI only treats `--key=val` as a named argument; a bare `key=val` would be positional. To meet AC-1, the console parser rewrites an **unquoted** bare `key=val` (before any `--`) to `--key=val`. Quoted tokens stay positional.
- `ParseError` gained `InvalidArgument` for lines `ArgumentParser` rejects (`---x`, unknown short flag, a name that starts with `-`). `TooManyArgs` now means only more than 32 tokens.
- `ConsoleHistory::GetNext` returns `nullptr` once it steps past the newest entry, meaning "restore the draft the user was typing".
- `ConsoleLogRing` is about 175 KB: own it as a heap object or a member of a heap object, never on the stack. `GetTotalWritten()` exists for the probe-line check in `DoStart`.
- A thread must call `Logger::RegisterThreadBuffer()` or its `DIA_LOG_*` lines are silently dropped; tests that log from helper threads do this.

### Threading and data flow

```
Main PU (Ultralight)                     Sim PU
 debug-panel.html strip
   Enter -> app.onConsoleLine(seq, part0..part3)
   DebugPanelPageModule::OnConsoleLine -- DebugConsoleLine (new EventStream) ------->
                                                   VisualDebuggerModule::DrainConsoleLines
                                                   ConsoleExecutor -> Dia::API::ExecuteCommand
   DebugPanelPageModule::DoUpdate <------ DebugConsoleResult (new EventStream) -------
     CallJSFunction("consoleResult", {seq, text, exitCode})

 ConsoleLogRing (Main-owned, Logger sink, any-thread writes)
   DoUpdate: DrainSince(cursor) -> CallJSFunction("appendLogLines", json)
 ConsoleHistory, CommandCompleter: Main thread only
```

**Input length.** The existing path (`app.onCommand(domain, cmd, argsJson)`) cannot carry a long command. The strip instead calls a new binding `app.onConsoleLine(seq, part0, part1, part2, part3)`: JS splits the text into up to four chunks of at most 60 characters, and `ConsoleLineAssembler` (Dia, pure logic) joins them into one string (limit 240). `DebugPanelPage` forwards it to the Sim PU as a `DebugConsoleLine` event (text up to 256 bytes) instead of reusing `DebugPanelCommand`, whose `argsJson` is 512 bytes but already JSON-wrapped.

**No JS calls from inside a JS callback.** `onConsoleLine`, `onConsoleComplete` and `onConsoleHistory` arrive while Ultralight is executing a script. Calling `CallJSFunction` (which runs `EvaluateScript`) from there re-enters the JS engine; Ctrl+P crashed the app this way (found 2026-10-13, see the palette flow). The handlers therefore only record the request and `DebugPanelPageModule::ProcessDeferredRequests()` performs it from `DoUpdate`, the same way `onCommand` queues an event. Any new JS-callable binding must follow this rule.

Autocomplete runs on the Main thread and reads the registry directly, which is why `command-unregister.md` makes the registry thread-safe and adds copy-returning list functions. The JS bridge truncates strings to 63 characters (see `DebugPanelPage.h`), so domain commands keep the three-argument convention; `consoleResult`/`appendLogLines` go the other way (C++ to JS) and are not affected.

### Log ring

`ConsoleLogRing` registers with `Logger::RegisterSink` in `DebugPanelPageModule::DoStart()` and unregisters in `DoStop()`. The Logger drains on its own thread and calls sinks under its registry mutex, so `OnLogEntry` takes its own mutex, writes into a fixed ring of 500 lines (message truncated to 255, plus level, channel and timestamp from `LogEntry`) and never logs. `RegisterSink` returns void and ignores the call once 8 sinks exist, so `DoStart` checks that a probe line arrives within a few frames and warns once if not. Level filtering is done in the page, not the ring.

### Observation

Logs: parse failures and non-zero exit codes at Info level on the `Debug` channel. Metrics: executed-command count. No health reporter (no failure mode that is not already visible in the panel).

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| AD-001 (f1-harness) | Baseline, Dia and CluicheTest edits need explicit human sign-off per change | The plan marks the tasks that touch `CluicheGameBaseline` and CluicheTest assets; each waits for sign-off |
| (Dia rule) | No new singletons without sign-off | History, ring and completer are owned by `DebugPanelPageModule`; nothing is global |
| SD-F1-001 | Debug-only | New baseline code stays inside `#ifdef DIA_DEBUG`; Dia console classes are plain logic and compile in all configs |

## Open Design Questions

1. **Command output.** Commands return only an int. The panel shows the exit code plus *all* log lines since the command ran, which may include unrelated lines. Is that acceptable, or should commands get a way to return text? Proposal: accept for v1; revisit if a command genuinely needs to print a table.
2. ~~Where the log ring runs.~~ **Resolved (read `Logger.cpp`):** the drain thread dispatches to sinks while holding `mRegistryMutex`, and `UnregisterSink` takes the same mutex, so unregistering in `DoStop()` waits for any in-flight `OnLogEntry`. Two constraints follow: `OnLogEntry` must not call `Logger` or `DIA_LOG_*` (the mutex is not recursive), and `Logger::kMaxSinks` is 8 with `RegisterSink` silently ignoring the call when full, so `DoStart` must confirm the ring receives a line.
3. **`debug-panel.html` ownership.** The file lives in CluicheTest's assets and CoW has a copy (decided: copy). This feature changes it, so the copy must be re-synced in the plan's last task. Should the console task also move the file to a shared location now? Decision to date: no.
4. **Escape sequences.** Only double quotes group tokens; no backslash escapes. Is that enough for the paths debug commands take?
