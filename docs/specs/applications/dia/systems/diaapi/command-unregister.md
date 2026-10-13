# Feature Spec: Command Unregister

**Parent:** @docs/specs/applications/dia/systems/diaapi/diaapi.md
**Status:** Approved
**Plan:** @docs/specs/applications/dia/systems/diaapi/command-unregister.plan.md

## Summary

Add `UnregisterCommand` / `UnregisterCommandJson` to the DiaAPI command registry, make the registry safe to use from more than one thread, and lift the 64-entry cap on `ListCommands()`. A module that registers a debug command in `DoStart()` can now remove it in `DoStop()`.

## Problem

`RegisterCommand` rejects duplicate names and nothing can remove a command. A stage-scoped module that registers a command therefore:

1. leaves a callback behind that captures a stopped module (`CommandCallback` is a `std::function`, usually capturing `this`), and
2. fails to register on the next stage entry (duplicate is rejected and logged as an error), so the stale callback keeps being called.

The registry also has no lock, and it is already read from several threads: `DebugServer` (`CommandDispatcher.cpp::GetCommand`, `DebugServer.cpp::ListCommands`) runs on its own thread, while modules register in `DoStart()` on their PU threads. The panel console will execute commands on the Sim thread while the Palette and autocomplete read the list from the Main thread (see panel-console.md, panel-palette.md), widening the overlap.

`ListCommands()` and `GetCommandsByCategory()` return `DynamicArrayC<const CommandInfo*, 64>`, which truncates once more than 64 commands exist.

## Goals

1. `bool UnregisterCommand(const StringCRC& name)` and `bool UnregisterCommandJson(const StringCRC& name)`. Return false if the name is not registered. Also remove from the pending list (commands registered before `Initialize()`).
2. After unregister, the same name can be registered again.
3. All registry reads and writes are thread-safe.
4. `ListCommands()` and `GetCommandsByCategory()` do not silently truncate.
5. Current registrants that are stage-scoped unregister in `DoStop()`.

## Non-Goals

- A RAII `CommandScope` helper. Add it later if three or more modules write the same unregister boilerplate.
- Changing duplicate-name behaviour (still rejected and logged).
- Aliases, hot-reload of command callbacks.
- Unregistering commands registered by `Application.cpp` / `DebugLayerManager` (process-lifetime owners).

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `UnregisterCommand` on a registered command returns true; `GetCommand` then returns nullptr and `ExecuteCommand` returns 3 | GoogleTest |
| AC-2 | `UnregisterCommand` on an unknown name returns false and logs nothing at error level | GoogleTest |
| AC-3 | Register, unregister, register the same name: second register returns true and the new callback runs | GoogleTest |
| AC-4 | A command registered before `Initialize()` (pending list) can be unregistered | GoogleTest |
| AC-5 | `UnregisterCommandJson` mirrors AC-1 to AC-3 for JSON commands | GoogleTest |
| AC-6 | Registering 100 commands and calling `ListCommands()` returns all 100 | GoogleTest |
| AC-7 | N threads register/unregister distinct names while M threads call `ExecuteCommand` and `ListCommands`: no crash, no lost or duplicated entry (run under ASan) | GoogleTest + `dia check sanitizer` |
| AC-8 | `UnregisterCommand` called while that command's callback is running on another thread blocks until the callback returns | GoogleTest |
| AC-9 | A callback that calls `RegisterCommand` or `UnregisterCommand` asserts in Debug (documented rule), and does not deadlock in Release | GoogleTest (death test) |
| AC-10 | Existing `CommandRegistryTest` suite still passes unchanged | `dia run googletest --filter="CommandRegistry*"` |

## Design

### API (DiaAPI/CommandRegistry/CommandRegistry.h)

```cpp
// Remove a CLI command. Returns false if not found.
// Must not be called from inside a command callback.
bool UnregisterCommand(const Dia::Core::StringCRC& name);

// Remove a JSON command. Returns false if not found.
bool UnregisterCommandJson(const Dia::Core::StringCRC& name);
```

### Locking

One `std::shared_mutex` guards `gRegistryState` (commands, pendingRegistrations, jsonCommands).

- `GetCommand`, `ListCommands`, `GetCommandsByCategory`: shared lock, return copies of the data they need (see below).
- `RegisterCommand`, `RegisterCommandJson`, `UnregisterCommand*`, `Initialize`, `Shutdown`: exclusive lock.
- `ExecuteCommand*`: shared lock held **for the duration of the callback**. This is what makes AC-8 true (Unregister waits for in-flight callbacks, so the owner can be destroyed straight after Unregister returns).
- A callback must not register or unregister (it would need the exclusive lock while holding the shared one). A `thread_local` "inside callback" flag asserts on this in Debug (AC-9).

`GetCommand` currently returns `const CommandInfo*`. A raw pointer can dangle once another thread unregisters. Keep the signature (callers are in tests and `HelpSystem`) but document that the pointer is only valid until the next `Unregister*`; add `bool CopyCommandInfo(name, CommandInfo& out)` for cross-thread callers (Palette, autocomplete).

### List capacity

Replace the return type with an alias `CommandList = DynamicArrayC<const CommandInfo*, 256>`. All callers found by the survey use `auto` (`HelpSystem`, `PythonBindings`, `DebugServer`, tests), so the alias is source compatible. Pointers in the list carry the same validity rule as `GetCommand`. Cross-thread callers use a new `ListCommandInfos(DynamicArrayC<CommandInfo, 256>& out)` that copies values.

### Adoption

Registrants found by survey (2026-10-13), with the action each needs:

| Registrant | Commands | Owner lifetime | Action |
|---|---|---|---|
| `Application.cpp` (quit, report, stages) | 3 JSON | process | none |
| `DebugLayerManager.cpp` (`debug.layer.*`) | 5 CLI | owned by `VisualDebuggerModule`, always-active | none; confirm in task 5 it is never destroyed and recreated |
| `AutomationService.cpp` | 7 JSON | `AutomationModule`, stage `all` | unregister in destructor for tidiness; low priority |
| `AssetRuntimeDebugCommands.cpp` | 6 CLI | asset runtime service | check in task 5 |
| `EntityInspectorModule.cpp` | 2 JSON | baseline module, check its stage list | unregister in `DoStop()` if not `all` |
| `Geometry2DTestStageModule.cpp` | 2 JSON | CluicheTest stage-scoped | **unregister in `DoStop()`**: the proven leak |

Callers that must stay source compatible: `HelpSystem.cpp` (`auto commands = ListCommands()`, `GetCommand`), `PythonBindings.cpp` (`auto`), `DebugServer.cpp` (`auto`), `CommandDispatcher.cpp` (`GetCommand` pointer then callback). `CommandDispatcher` runs on the DebugServer thread and holds the pointer across the call, so task 4 migrates it to `CopyCommandInfo`/`ExecuteCommand*`.

## Binding Decisions

No binding constraints apply beyond the existing DiaAPI conventions (no STL in public APIs: `std::shared_mutex` stays private to the .cpp).

## Open Design Questions

1. **Lock held during callbacks.** Holding the shared lock while a long command runs blocks `Unregister` for that time and blocks any registration at all (writers wait). Commands here are short debug actions, so proposal is to accept it. If a command can run for seconds, switch to a per-command in-flight counter instead.
2. ~~Pointer-returning APIs.~~ **Resolved:** keep the pointer APIs (documented lifetime), add copy variants, migrate cross-thread callers only (`CommandDispatcher`, Palette, autocomplete). All current callers use `auto`, so no signature churn.
3. **Cap value.** 256 is a guess. The Palette spec cites 47 commands today. Is 256 enough, or should the list be heap-backed?
