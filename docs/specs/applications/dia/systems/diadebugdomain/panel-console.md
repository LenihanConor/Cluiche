# Feature Spec: Debug Console

**Parent:** @docs/specs/applications/cow/systems/f1-harness/f1-harness.md  
**Mockup:** @docs/specs/applications/cow/systems/f1-harness/mockups/debug-console.html  
**Status:** Approved

## Summary

An in-game Ultralight-rendered text console for executing DiaAPI commands without external tools. Provides command input with autocomplete, multi-tab output log with filtering, command history, and context sidebar. Toggled with `~` (tilde) key. Proves CoW F-1 can execute `bus_traffic.*` commands in-game and see the output immediately.

## Problem Statement

DiaAPI command execution currently requires either:
1. External tool (`dia <command>` in terminal)
2. Remote editor connection (CluicheEditor via DebugServer)
3. Direct C++ code modification

During CoW testbed development, developers need to:
- Start/stop bus traffic recording (`bus_traffic.start`, `bus_traffic.stop`)
- Query runtime state (`asset_runtime.get_loaded`, `simtime.status`)
- Toggle debug layers (`debug.layer.enable`)
- Adjust simulation speed (`simtime.set_speed 2.0`)

Without an in-game console, these workflows break flow and require context switching to external tools.

## Goals

1. Execute any registered DiaAPI command by typing it in-game
2. See command output (success/error/info logs) immediately in the console
3. Autocomplete command names (TAB key, fuzzy search)
4. Navigate command history (up/down arrows)
5. Filter log output by level (All, Info, Warnings, Errors)
6. Show current context (stage, tick, command count)
7. Toggle visibility with `~` key
8. Draggable/dockable window (Ultralight-based)
9. Multi-tab interface (Output, History, Commands, Help)

## Non-Goals

1. Command scripting or batch execution (run one at a time)
2. Custom keybinds for individual commands (use Debug Palette for that)
3. Log file export (logs are ephemeral, view only)
4. Command aliases or macros (use full command names)
5. Visual debug domain controls (that's DiaDebugPanel's job)

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | Console toggles on/off with `~` key | Manual test: press tilde in CoW F-1 |
| AC-2 | Typing `bus_traffic.start recording.dat` and pressing Enter executes the command and logs the result | Manual test + GoogleTest mock |
| AC-3 | Typing `bus_` and pressing TAB shows autocomplete suggestions (`bus_traffic.start`, `bus_traffic.stop`, `bus_traffic.status`) | Manual test |
| AC-4 | Up arrow recalls previous command, down arrow moves forward in history | Manual test |
| AC-5 | Filter buttons (All, Info, Warnings, Errors) toggle log visibility | Manual test |
| AC-6 | Context sidebar shows current stage name, tick count, and command registry count | Manual test in CoW F-1 |
| AC-7 | Tab navigation works (Output, History, Commands, Help) | Manual test |
| AC-8 | Window can be dragged by title bar and docked to screen edges | Manual test (DiaUI docking) |
| AC-9 | Console output matches mockup visual layout | Visual comparison test |
| AC-10 | Console captures `DIA_LOG_*` output while visible | Manual test: trigger warning, see it in log |
| AC-11 | Closing console with `~` preserves log history (reopening shows same content) | Manual test |
| AC-12 | Console works across stage transitions (survives Boot → FrontEnd → Boot) | Manual test in CoW |

## Design

### Full API

```cpp
// Dia/DiaDebugConsole/DebugConsoleModule.h
namespace Dia::DebugConsole {

class DebugConsoleModule : public Dia::ApplicationFlow::MainModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
    static constexpr const char* kDescription = "In-game text console for executing DiaAPI commands";
    
    explicit DebugConsoleModule(const Dia::Core::StringCRC& instanceId);
    ~DebugConsoleModule() override;
    
    void Toggle();
    void Show();
    void Hide();
    bool IsVisible() const { return mVisible; }
    
protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
    Dia::ApplicationFlow::StopResult DoStop() override;
    
private:
    DebugConsolePage* mPage = nullptr;
    ConsoleLogSink* mLogSink = nullptr;
    ConsoleHistory mHistory;
    bool mVisible = false;
};

class ConsoleExecutor {
public:
    // Returns command exit code (0=success, 3=not found, other=command-specific error)
    int Execute(const char* commandText);
    
private:
    // Parses "cmd arg1 --flag key=value" into CommandArgs
    // Grammar: <command> [positional...] [--flag...] [key=value...]
    bool ParseCommandLine(const char* text, Dia::API::CommandArgs& outArgs);
};

class ConsoleLogSink : public Dia::Observation::ILogSink {
public:
    static constexpr int kCapacity = 500;
    
    void OnLog(Dia::Observation::LogLevel level, const char* message) override;
    
    // Drain new log lines since last call
    void DrainLogLines(Dia::Core::Containers::DynamicArrayC<LogLine, 64>& outLines);
    
private:
    struct LogLine {
        float time;
        Dia::Observation::LogLevel level;
        char message[256];
    };
    
    Dia::Core::Containers::RingBufferC<LogLine, kCapacity> mBuffer;
    int mLastDrainedIndex = 0;
};

class ConsoleHistory {
public:
    static constexpr int kCapacity = 50;
    
    void Add(const char* command);
    const char* GetPrevious();  // Navigate up
    const char* GetNext();      // Navigate down
    void Reset();               // Reset cursor to end
    
private:
    Dia::Core::Containers::RingBufferC<char[256], kCapacity> mCommands;
    int mCursor = -1;  // -1 = at end (no nav active)
};

} // namespace Dia::DebugConsole
```

### Architecture

```
DiaDebugConsole (new Dia module)
  ├─ DebugConsolePage.h/.cpp         // Ultralight page (C++ shell)
  ├─ debug-console.html                // Ultralight HTML/CSS/JS
  ├─ ConsoleExecutor.h/.cpp           // Command execution bridge
  ├─ ConsoleLogSink.h/.cpp            // DiaLog capture
  └─ ConsoleHistory.h/.cpp            // Command history buffer
```

### Components

**1. DebugConsolePage** (Ultralight page)
- Inherits from `Dia::UI::Page`
- Exposes JS binding: `app.executeCommand(commandText)`
- Receives command from HTML, forwards to `ConsoleExecutor`
- Pushes log updates to HTML via `CallJSFunction("appendLogLine", ...)`

**2. ConsoleExecutor**
- Wraps `Dia::API::ExecuteCommand()`
- Parses typed string into `CommandArgs` (split on spaces, extract named args/flags)
- Captures command result code
- Forwards output to `ConsoleLogSink`

**3. ConsoleLogSink**
- Implements `Dia::Observation::ILogSink`
- Ring buffer of last 500 log lines (time, level, message)
- Filters by level (Info, Warning, Error)
- Notifies `DebugConsolePage` on new log line

**4. ConsoleHistory**
- Ring buffer of last 50 commands
- Navigate with up/down arrow (sent from JS)
- Persists across console close/reopen (not across app restart)

**5. Autocomplete**
- Query `Dia::API::GetCommandList()` on TAB press
- Fuzzy match typed prefix against command names
- Return top 5 matches

### HTML/CSS/JS (Ultralight)

See `debug-console.html` mockup for full layout. Key features:

- **Multi-tab interface:** Output (default), History, Commands, Help
- **Output tab:** Scrolling log with timestamps, level badges (INFO/WARN/ERROR), filter buttons
- **Sidebar:** Recent commands, quick actions, context info (stage, tick, command count)
- **Input bar:** `>` prompt + text field + autocomplete overlay
- **Status bar:** Command count, panel count, keyboard hints

### Lifecycle

**Initialization:**
1. `DebugConsoleModule::DoStart()` on MainPU
2. Create `DebugConsolePage`, load `debug-console.html`
3. Register `ConsoleLogSink` with `DiaLog`
4. Bind `~` key to `DebugConsoleModule::Toggle()` (caller's responsibility, see below)

**Per-Frame:**
1. If console visible, drain new log lines from `ConsoleLogSink`
2. For each line, call `page->CallJSFunction("appendLogLine", {time, level, message})`
3. JS updates the HTML output log

**Command Execution:**
1. User types command + Enter in HTML input field
2. JS calls `app.executeCommand(commandText)`
3. `DebugConsolePage::OnExecuteCommand_JS()` → `ConsoleExecutor::Execute()`
4. `ConsoleExecutor` parses string, calls `Dia::API::ExecuteCommand()`
5. Result code + any `DIA_LOG_*` output flows back to console

**Toggle:**
- Caller (e.g., `CluicheGameBaselineInputModule`) detects `~` keypress
- Calls `DebugConsoleModule::Toggle()`
- Module shows/hides Ultralight page

### Dependencies

| Dependency | Why |
|------------|-----|
| `DiaAPI` | Command registry and execution |
| `DiaUI` | Ultralight page rendering |
| `DiaObservation` | `ILogSink` for capturing `DIA_LOG_*` |
| `DiaApplicationFlow` | Module lifecycle |
| `DiaInput` (optional) | Caller binds `~` key; console module doesn't touch input directly |

### Files Touched

**New files:**
- `Dia/DiaDebugConsole/DebugConsolePage.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleExecutor.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleLogSink.h/.cpp`
- `Dia/DiaDebugConsole/ConsoleHistory.h/.cpp`
- `Dia/DiaDebugConsole/DebugConsoleModule.h/.cpp`
- `Dia/DiaDebugConsole/DiaDebugConsole.vcxproj`
- `Dia/DiaDebugConsole/dia.debug.console.architecture.module.md`
- `Dia/DiaDebugConsole/Assets/debug-console.html` (copied from mockup)
- `Cluiche/Tests/GoogleTests/DiaDebugConsole/TestConsoleExecutor.cpp`
- `Cluiche/Tests/GoogleTests/DiaDebugConsole/TestConsoleHistory.cpp`

**Updated files:**
- `Cluiche/CluicheGameBaseline/Modules/InputModule.cpp` (bind `~` key to toggle)
- `Cluiche/Assets/CoW/cow_main.diaapp` (add `DebugConsoleModule` instance)
- `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` (add module)
- `Cluiche.sln` (add `DiaDebugConsole.vcxproj`)

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| PD-004 | No STL in public APIs | `DebugConsoleModule` API uses `const char*` and DiaCore types only |
| AD-001 | Module YAML frontmatter | `dia.debug.console.architecture.module.md` provided |
| SD-F1-001 | Console is Debug-only; #ifdef DIA_DEBUG guards module | Console compiles out in Release builds |
| SD-F1-002 | Console uses Ultralight (not ImGui) for consistency with DiaDebugPanel | All debug panels use same rendering tech |

## Open Design Questions

1. **Command parsing.** DiaAPI's `ExecuteCommand(StringCRC, CommandArgs)` signature expects pre-parsed args. Should console do full parsing (split on spaces, detect `--flags` and `key=value` named args), or pass raw string and require each command to parse? **Proposal:** Console parses into `CommandArgs` so commands remain parser-agnostic.

2. **Log sink lifecycle.** Should `ConsoleLogSink` register/unregister on console show/hide, or stay registered always? **Proposal:** Always registered (keeps history across close/reopen), but ring buffer prevents unbounded growth.

3. **Help tab content.** Should Help tab show all commands dynamically, or just usage instructions? **Proposal:** Help shows usage instructions ("Type command, press TAB to autocomplete, ↑↓ for history"); full command list is in Debug Palette.

4. **Autocomplete ranking.** When multiple matches exist, rank by exact prefix > fuzzy match > category? **Proposal:** Yes; `bus_` prefers `bus_traffic.*` over `blackboard.*`.

## Status

Draft

**Plan:** TBD (to be written after approval)
