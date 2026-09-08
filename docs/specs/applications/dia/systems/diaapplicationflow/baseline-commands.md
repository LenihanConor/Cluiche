# Feature Spec: Baseline Commands

## Parent System
@docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md

## Builds On
@docs/specs/applications/dia/systems/diaapplicationflow/inspectable.md
@docs/specs/applications/dia/systems/diaapplicationflow/lifecycle-events.md

## Research
@docs/research/e2e_testing/design-decisions.md (Section 4.2, 4.5)

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-005, PD-006, PD-007, PD-008 |
| Application | @docs/specs/applications/dia/dia.md | AD-001, AD-002, AD-003 |
| System (DiaApplicationFlow) | @docs/specs/applications/dia/systems/diaapplicationflow/diaapplicationflow.md | SD-001, SD-004, SD-011, SD-016 |
| System (DiaAPI) | @docs/specs/applications/dia/systems/diaapi/diaapi.md | SD-API-001, SD-API-002, SD-API-005 |

## Problem Statement

DiaApplicationFlow has no way to externally query its runtime state or request shutdown via DiaAPI. Today you can only do this from C++ code (`Application::RequestShutdown()`, `IApplicationInspectable::GetCurrentStage()`). This blocks remote debugging, automation tooling, and the E2E orchestration stack, all of which need to drive and inspect the application over the wire.

Additionally, DiaAPI's current command name validation (`[a-z0-9-]+`) rejects the dotted namespace grammar (`dia.app.quit`) that the automation architecture mandates. And DiaAPI only supports CLI-style callbacks (`int(const CommandArgs&)`) with no structured JSON response path — blocking commands like `dia.app.report` that need to return rich data.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | DiaAPI `ValidateCommandName` accepts `[a-z0-9._-]+` (dots and underscores added). | Unit test |
| AC2 | DiaAPI provides a JSON callback overload: `using CommandCallbackJson = std::function<Json::Value(const Json::Value& params)>;` registered via `RegisterCommand` with a `CommandInfo` variant that carries a JSON callback. | Unit test |
| AC3 | DiaAPI provides `Json::Value ExecuteCommandJson(const StringCRC& name, const Json::Value& params)` that dispatches to JSON-callback commands. Returns `{"error": "command not found"}` for unknown commands. | Unit test |
| AC4 | DiaDebugServer's `CommandDispatcher::ExecuteDiaAPICommand` routes to `ExecuteCommandJson` for JSON-registered commands, returning the structured `Json::Value` response to the WebSocket caller. | Unit test or integration verification |
| AC5 | `dia.app.quit` command registered with DiaAPI (JSON callback). Params: `{}`. Effect: calls `Application::RequestShutdown()`. Returns `{"success": true, "data": {}}`. | Unit test |
| AC6 | `dia.app.report` command registered with DiaAPI (JSON callback). Params: `{}`. Returns `{"success": true, "data": {stage, modules: [{instanceId, typeId, state}], transition: {inProgress, fromStage, toStage, heldByGuards}}}`. Module state is a string enum: `"Inactive"`, `"Starting"`, `"Active"`, `"Stopping"`, `"Failed"`. | Unit test |
| AC7 | Commands registered from `Application::Start()` (after manifest validation passes, before entering the initial stage). | Code review + unit test |
| AC8 | Commands use dotted namespace grammar per design-decisions §4.1. | Code review |
| AC9 | Commands are available regardless of whether DiaAutomation or AutomationModule are active. | Unit test (no automation in test setup) |
| AC10 | DiaAPI is always linked by DiaApplicationFlow. If the registry is not initialized when `Start()` runs, commands are queued as pending registrations (existing DiaAPI behaviour). | Code review |
| AC11 | Existing CLI-style commands (`[a-z0-9-]+` names) continue to register and execute unchanged. | Existing test suite passes |
| AC12 | `dia run googletest` passes. `dia run cluichetest` boots, runs, and shuts down with no behavioural change. | Build + test verification gate |

## Design

### DiaAPI Extensions (prerequisite — backlog item #0)

`Dia/DiaAPI/CommandRegistry/CommandRegistry.h` — additions:

```cpp
namespace Dia { namespace API {

    // JSON-oriented callback — returns structured response
    using CommandCallbackJson = std::function<Json::Value(const Json::Value& params)>;

    struct CommandInfoJson
    {
        Dia::Core::StringCRC name;
        const char* description;
        Dia::Core::StringCRC category;
        const char* owner;
        CommandCallbackJson callback;
    };

    // Register a JSON-callback command
    // Logs a warning if a CLI command with the same name already exists
    bool RegisterCommandJson(const CommandInfoJson& info);

    // Execute a JSON-callback command
    // Returns {"success": true, "data": <handler result>} on success
    // Returns {"success": false, "error": "command not found"} if not registered
    // Returns {"success": false, "error": "<message>"} on handler failure
    Json::Value ExecuteCommandJson(const Dia::Core::StringCRC& name, const Json::Value& params);

}}
```

Name validation change in `CommandRegistry.cpp`:
```cpp
// Allow lowercase letters, digits, dots, underscores, and hyphens
if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
```

### DiaDebugServer Routing

`Dia/DiaDebugServer/CommandDispatcher.cpp` — update `ExecuteDiaAPICommand`:

```cpp
Json::Value CommandDispatcher::ExecuteDiaAPICommand(const StringCRC& commandName, const Json::Value& payload)
{
    // Try JSON-registered commands first
    Json::Value jsonResult = Dia::API::ExecuteCommandJson(commandName, payload);
    if (jsonResult["success"].asBool() || jsonResult["error"].asString() != "command not found")
    {
        return jsonResult; // Already in {success, data/error} envelope
    }

    // Fallback to legacy CLI-style command execution
    // ... existing code wraps int exit code in {"success": exitCode==0, ...} ...
}
```

### DiaApplicationFlow Command Registration

`Dia/DiaApplicationFlow/Application.cpp` — inside `Start()`, after manifest validation:

```cpp
void Application::RegisterBaselineCommands()
{
    Dia::API::CommandInfoJson quitCmd;
    quitCmd.name = Dia::Core::StringCRC("dia.app.quit");
    quitCmd.description = "Request application shutdown";
    quitCmd.category = Dia::Core::StringCRC("dia.app");
    quitCmd.owner = "DiaApplicationFlow";
    quitCmd.callback = [this](const Json::Value&) -> Json::Value {
        RequestShutdown();
        return Json::Value(Json::objectValue); // empty data — envelope adds success
    };
    Dia::API::RegisterCommandJson(quitCmd);

    Dia::API::CommandInfoJson reportCmd;
    reportCmd.name = Dia::Core::StringCRC("dia.app.report");
    reportCmd.description = "Report current application state";
    reportCmd.category = Dia::Core::StringCRC("dia.app");
    reportCmd.owner = "DiaApplicationFlow";
    reportCmd.callback = [this](const Json::Value&) -> Json::Value {
        Json::Value result;
        result["stage"] = GetCurrentStage().AsChar();

        // Active modules from IApplicationInspectable
        Dia::Core::Containers::DynamicArrayC<ModuleStateInfo, 64> modules;
        // ... populate from all PUs ...
        Json::Value modulesArr(Json::arrayValue);
        for (unsigned int i = 0; i < modules.Size(); ++i)
        {
            Json::Value m;
            m["instanceId"] = modules[i].instanceId.AsChar();
            m["typeId"] = modules[i].typeId.AsChar();
            m["state"] = ModuleStateToString(modules[i].state);
            modulesArr.append(m);
        }
        result["modules"] = modulesArr;

        // Transition info
        TransitionInfo ti = GetTransitionInfo();
        Json::Value transition;
        transition["inProgress"] = ti.inProgress;
        transition["fromStage"] = ti.fromStage.AsChar();
        transition["toStage"] = ti.toStage.AsChar();
        transition["heldByGuards"] = ti.heldByGuards;
        result["transition"] = transition;

        return result; // envelope wraps in {"success": true, "data": result}
    };
    Dia::API::RegisterCommandJson(reportCmd);
}
```

### Dependency

DiaApplicationFlow gains a **hard** dependency on DiaAPI. DiaAPI is a static lib with zero runtime cost if no commands are executed — same weight class as DiaCore and DiaObservation which are already linked. No conditional compilation; no preprocessor guard.

### Ownership Rule (§4.5)

> "The handler lives with the data it needs to operate on."

- `dia.app.quit` needs `Application::RequestShutdown()` — lives in Application
- `dia.app.report` needs `IApplicationInspectable` data — lives in Application

Both handlers capture `this` (the Application instance).

### Out of Scope

- `dia.automation.*` commands — those belong to DiaAutomation (item #3)
- Game-specific commands (`cluichetest.*`) — registered by game modules
- Unifying CLI and JSON callback into a single type — unnecessary complexity
- Deprecating the existing CLI callback — it serves its purpose for CLI-first commands

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaAPI/CommandRegistry/CommandRegistry.h` | Add `CommandCallbackJson`, `CommandInfoJson`, `RegisterCommandJson`, `ExecuteCommandJson` |
| `Dia/DiaAPI/CommandRegistry/CommandRegistry.cpp` | Relax `ValidateCommandName` to `[a-z0-9._-]+`; implement `RegisterCommandJson` / `ExecuteCommandJson`; add internal JSON command storage |
| `Dia/DiaDebugServer/CommandDispatcher.cpp` | Route through `ExecuteCommandJson` before legacy fallback |
| `Dia/DiaApplicationFlow/Application.h` | Declare `RegisterBaselineCommands()` (private) |
| `Dia/DiaApplicationFlow/Application.cpp` | Implement `RegisterBaselineCommands()`; call from `Start()` |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Add DiaAPI project reference |
| `Cluiche/Tests/GoogleTests/API/TestCommandRegistryJson.cpp` | New test file for AC1-AC4, AC11 |
| `Cluiche/Tests/GoogleTests/ApplicationFlow/TestBaselineCommands.cpp` | New test file for AC5-AC10 |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Add new test files |
| `docs/specs/systems/dia/diaapplicationflow.md` | Add feature row |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Relax `ValidateCommandName` to `[a-z0-9._-]+`. Update existing validation test. | Dotted names pass; existing tests pass | Todo | haiku | Backlog item #0 partial |
| 2 | Add `CommandCallbackJson`, `CommandInfoJson`, `RegisterCommandJson`, `ExecuteCommandJson` to DiaAPI. | AC2, AC3 unit tests pass | Todo | sonnet | |
| 3 | TDD-RED: write `TestCommandRegistryJson.cpp` covering AC1-AC4, AC11 (dotted names, JSON registration, JSON execution, legacy unchanged). Quote failing output. | Tests fail with "not declared" | Todo | sonnet | TDD red gate |
| 4 | TDD-GREEN: implement JSON command storage + dispatch in `CommandRegistry.cpp`. | AC1-AC4, AC11 green | Todo | sonnet | |
| 5 | Update `CommandDispatcher::ExecuteDiaAPICommand` to route via `ExecuteCommandJson` first. | AC4 passes end-to-end | Todo | sonnet | |
| 6 | Add DiaAPI project reference to `DiaApplicationFlow.vcxproj`. | Compiles | Todo | haiku | |
| 7 | TDD-RED: write `TestBaselineCommands.cpp` covering AC5-AC10. Quote failing output. | Tests fail — commands not registered | Todo | sonnet | TDD red gate |
| 8 | Implement `Application::RegisterBaselineCommands()` — register `dia.app.quit` + `dia.app.report`, call from `Start()`. | AC5-AC10 green | Todo | sonnet | |
| 9 | Run `dia run googletest`; confirm all pass. Run `dia run cluichetest`. Quote output. | AC12 verification gate | Todo | sonnet | |
| 10 | Add feature row to `diaapplicationflow.md` system spec. Update module doc. Commit. | Doc only | Todo | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Command names stored as `StringCRC`. `dia.app.quit` and `dia.app.report` are StringCRC constants. Category (`dia.app`) is StringCRC. |
| PD-004 | No STL containers in public APIs | `CommandInfoJson` uses `Dia::Core::StringCRC` and raw `const char*` — no STL containers. `CommandCallbackJson` uses `std::function` (same as existing `CommandCallback` — internal callable, not a container per PD-004 scope). JSON response uses jsoncpp `Json::Value` (external library type, same pattern as DiaDebugServer). |
| PD-005 | x64 only | No platform-specific code. |
| PD-006 | VS project files source of truth | New test files added to `GoogleTests.vcxproj`. DiaAPI reference added to `DiaApplicationFlow.vcxproj`. |
| PD-007 | C++20 required | Uses `std::function`, lambda captures, `enum class` — all within C++20. |
| PD-008 | Directory.Build.props owns build settings | No per-project build setting overrides. Only a project reference added. |
| AD-001 | Module docs with YAML frontmatter | `dia.applicationflow.architecture.module.md` updated to note DiaAPI dependency. |
| AD-002 | No STL in public APIs | See PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | DiaAPI additions in `Dia::API::`. DiaApplicationFlow additions in `Dia::ApplicationFlow::`. |
| SD-001 | Config sole source of truth for structural wiring | Commands are runtime behaviour, not structural wiring. No manifest schema changes. |
| SD-004 | TransitionTo is app-wide | `dia.app.quit` calls `RequestShutdown()` (app-wide). `dia.app.report` reads app-wide state. Consistent. |
| SD-011 | Shutdown is framework-level | `dia.app.quit` delegates to `RequestShutdown()` — the existing framework shutdown mechanism. |
| SD-016 | IApplicationInspectable for debug/editor/test | `dia.app.report` reads from the inspectable interface — it does not bypass it or expose internal state directly. |
| SD-API-001 | Commands identified by StringCRC | `dia.app.quit` and `dia.app.report` are StringCRC names. |
| SD-API-002 | Exit codes follow Unix conventions | JSON commands return `Json::Value` (not int exit codes). The two APIs are parallel — CLI commands return `int`, JSON commands return structured responses. No conflict; different contracts for different consumers. |
| SD-API-005 | Registry is global singleton | JSON commands stored in the same global registry state (`Internal::gRegistryState`). |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Conditional compilation | Should `DIA_HAS_API` be a preprocessor guard, or should DiaAPI always be linked by DiaApplicationFlow? | Always link. DiaAPI is a static lib with zero runtime cost if unused. Every DiaApplicationFlow consumer already links DiaCore/DiaObservation — same weight class. No preprocessor guard. |
| 2 | Report response shape | Should `dia.app.report` return module state as a string enum (`"Active"`, `"Starting"`) or integer? | String. The response is consumed by Python/humans over WebSocket. `"Active"` is self-documenting; integer requires a lookup table. No performance concern for a debug query. |
| 3 | Thread safety | `dia.app.quit` calls `RequestShutdown()`. If the command arrives on the DebugServer's WebSocket thread, is this safe? | Safe. `RequestShutdown()` sets an atomic flag (`mShutdownRequested`), already designed to be called from any thread. Consumed next frame from main thread's `Update()`. |
| 4 | Registration timing | Commands are registered in `Application::Start()`. If DiaDebugServer connects before `Start()` completes, could a `dia.app.report` arrive before registration? | Not an issue. DiaDebugServer is a Module — its `DoStart()` runs during stage entry, which happens after `Application::Start()` completes. Commands are registered before any module's `DoStart`, so DiaDebugServer cannot be listening yet. Safe by construction. |
| 5 | JSON command collision | What if both a CLI command and a JSON command register the same name? Should the registry reject, or allow both (dispatched by call path)? | Allow both — they're different dispatch paths (`ExecuteCommand` for CLI, `ExecuteCommandJson` for JSON). A command may have one or both. Log a warning when both are registered under the same name to flag potential confusion. |
| 6 | Error shape | Should JSON command errors follow a consistent envelope (`{success, error, data}`) or let each command define its own shape? | Consistent envelope. All JSON command responses: `{"success": true, "data": {...}}` on success, `{"success": false, "error": "message"}` on failure. Framework wraps the envelope; command callback just returns its data or signals error. Python client parses uniformly. |

## Status

`Done` (2026-05-21) — All ACs implemented and verified. Plan: [baseline-commands.plan.md](baseline-commands.plan.md)
