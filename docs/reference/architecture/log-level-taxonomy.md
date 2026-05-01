# Log Level Taxonomy

**Last Updated:** 2026-04-21

This document defines the log level conventions for all code in the Cluiche platform that uses the DiaLogger system (`DIA_LOG_*` macros). It is the authoritative reference for choosing the correct log level and channel when adding logging to any module.

[-> DiaLogger System Spec](../../specs/systems/dia/dialogger.md) | [-> Coding Standards](../development/coding-standards.md)

---

## Log Levels

DiaLogger provides five severity levels, ordered from most verbose to least verbose. The `DIA_LOG_TRACE` and `DIA_LOG_DEBUG` macros compile to no-ops in Release builds (`NDEBUG` defined).

### TRACE

**When to use:** High-frequency or very granular data that would be overwhelming even during active debugging. Reserved for hot paths where you need to see every iteration.

- Per-frame update ticks
- Individual buffer push/pop operations
- Per-element iteration checks
- Anything that fires every frame or inside a tight loop

```cpp
// Correct — per-frame tick in an update loop
DIA_LOG_TRACE("Application", "ProcessingUnit: Update tick %u", frameCount);
```

**Build behavior:** Stripped in Release.

### DEBUG

**When to use:** Operational detail useful when actively investigating a problem. Too noisy for normal operation, but valuable when something goes wrong.

- Per-message/per-request routing and payloads
- Ping/pong heartbeat exchanges
- Individual handler matches within a dispatch
- Payload sizes and content summaries

```cpp
// Correct — per-request detail
DIA_LOG_DEBUG("Editor", "WebUIBridge: HandleEditorCall type='%s' reqId='%s'", type, reqId);

// Correct — periodic heartbeat
DIA_LOG_DEBUG("DebugServer", "HandlePing connId=%d ts=%llu", connId, ts);
```

**Build behavior:** Stripped in Release.

### INFO

**When to use:** Significant lifecycle or state-change events. The "flight recorder" level. If you had to reconstruct what happened from logs alone, INFO entries are the breadcrumbs you would read.

- Module/plugin/system start, stop, load, unload
- State transitions (connecting -> connected -> disconnected)
- Handler and callback registration
- Server start/stop with configuration
- Client connect/disconnect events
- Manifest and project loading results
- User-initiated actions (connect, disconnect, undo, redo)

```cpp
// Correct — lifecycle boundary
DIA_LOG_INFO("Application", "PluginLoaderModule: DoStart");

// Correct — state transition
DIA_LOG_INFO("Editor", "GameConnectionController: State %s -> %s", oldState, newState);

// Correct — registration
DIA_LOG_INFO("Editor", "WebUIBridge: RegisterEventHandler '%s' (count=%u)", name, count);
```

**Build behavior:** Always compiled. Keep INFO-level logging lean.

### WARNING

**When to use:** Something unexpected happened but the system recovered or degraded gracefully. The operation continued, but the situation is worth investigating.

- Missing optional dependency (module, bridge, UI system)
- No handler found for an incoming event type
- Skipped operation due to null optional component
- Rejected user requests (already connected, invalid input)
- Connection lost unexpectedly
- Invalid ping or malformed but non-fatal input

```cpp
// Correct — optional dependency missing, system degrades
DIA_LOG_WARNING("Editor", "WebUIBridge: uiSystem is null, JS handler not registered");

// Correct — unexpected disconnect
DIA_LOG_WARNING("Editor", "GameConnectionController: Socket dropped while in state %s", state);
```

### ERROR

**When to use:** Something failed and the intended operation cannot complete. Errors indicate a problem that likely requires attention.

- JSON parse failures on external input
- Required manager/bridge is null when it must not be
- File I/O failures (manifest not found, write failed)
- Invalid URL or configuration that prevents connection
- Any failure at a system boundary (user input, external APIs, file I/O)

```cpp
// Correct — external input parse failure
DIA_LOG_ERROR("Editor", "WebUIBridge: HandleEditorCall failed to parse JSON: %s", errors);

// Correct — required dependency missing
DIA_LOG_ERROR("Editor", "GameConnectionController: BeginConnectReal failed, manager is null");
```

---

## Channel Conventions

Channels group log output by subsystem. Each channel is a string that becomes a `StringCRC` at the call site. Channels are used by sinks for whitelist filtering.

| Channel | Owner | Scope |
|---------|-------|-------|
| `Application` | CluicheEditor phases, modules, PluginLoaderModule | Application lifecycle, phase transitions, plugin loading |
| `Editor` | DiaEditor (plugins, MVC, WebUIBridge, LiveConnection) | Editor framework, UI bridge, game connection |
| `DebugServer` | DiaDebugServer | Debug server, protocol handling, metrics broadcast |
| `WebSocket` | DiaWebSocket | WebSocket client/server transport |
| `API` | DiaAPI | Command registry, CLI framework |
| `UI` | DiaUI, DiaUICEF | UI system, CEF integration |

**Rules for channel assignment:**
- Use the channel that owns the code, not the channel that triggered the call.
- One file = one channel. If a file logs to multiple channels, it is probably in the wrong module.
- New modules should reuse an existing channel if they belong to that subsystem. Only create a new channel for a genuinely new subsystem.

---

## Rules

1. **Never log at INFO or above inside tight loops.** Per-frame updates, per-element iteration, and per-message callbacks should use DEBUG or TRACE. If you need to log inside a loop, use TRACE.

2. **Lifecycle boundaries are always INFO.** `DoStart`, `DoStop`, `Initialize`, `Shutdown`, `OnLoad`, `OnUnload` — these are the skeleton of what happened.

3. **State transitions are always INFO.** Any time an object moves between named states (connecting -> connected, running -> shutdown), log it at INFO.

4. **"Skipped because X is null" depends on whether X should exist.** If the null means a configuration error or broken dependency, use WARNING. If it is a valid early-out (e.g. no UI system in a headless test), use DEBUG.

5. **Parse and validation failures on external input are ERROR.** JSON from a WebSocket, data from a file, user-provided URLs — if the input is malformed, that's an error.

6. **User-initiated actions are INFO.** Connect, disconnect, undo, redo, load project — log the action and its outcome.

---

## Anti-Patterns

### Embedding severity in the message

```cpp
// Bad — the level is already WARNING, don't repeat it in the text
DIA_LOG_WARNING("Editor", "WARNING - type not found");

// Good
DIA_LOG_WARNING("Editor", "EditorPluginRegistry: type not found (checked %u entries)", count);
```

### INFO logging in a per-message callback

```cpp
// Bad — fires on every incoming WebSocket message
DIA_LOG_INFO("Editor", "Received text message (%u bytes)", len);

// Good — use DEBUG for per-message detail
DIA_LOG_DEBUG("Editor", "Received text message (%u bytes)", len);
```

### Silent failure at system boundaries

```cpp
// Bad — returns false with no log, caller has no idea why
if (!file.is_open())
    return false;

// Good — log the failure before returning
if (!file.is_open())
{
    DIA_LOG_WARNING("Editor", "EditorManifestLoader: Could not open '%s'", path);
    return false;
}
```

### Logging inside a hot loop

```cpp
// Bad — fires thousands of times per frame
for (unsigned int i = 0; i < count; ++i)
{
    DIA_LOG_INFO("Editor", "Processing entry %u", i);
    Process(entries[i]);
}

// Good — log the aggregate, not each iteration
DIA_LOG_INFO("Editor", "Processing %u entries", count);
for (unsigned int i = 0; i < count; ++i)
    Process(entries[i]);
```

### Using ERROR for recoverable situations

```cpp
// Bad — system continues fine, this is not an error
DIA_LOG_ERROR("Editor", "No event handler found for '%s'", type);

// Good — it's unexpected but the system handles it
DIA_LOG_WARNING("Editor", "No event handler found for '%s'", type);
```

---

## DiaCore Exception

DiaCore **cannot** use `DIA_LOG_*` macros because DiaLogger depends on DiaCore (circular dependency). DiaCore continues to use `Dia::Core::Log::OutputLine` / `OutputVaradicLine` for its internal logging. All other modules should use the `DIA_LOG_*` macros.

---

## Cross-References

- [DiaLogger System Spec](../../specs/systems/dia/dialogger.md) — system design, API contracts, binding decisions
- [Core Logger Feature Spec](../../specs/features/dia/dialogger/core-logger.md) — Logger singleton, DIA_LOG macros, thread-local buffers
- [Sink System Feature Spec](../../specs/features/dia/dialogger/sink-system.md) — ISink interface, channel filtering, level thresholds
- [Coding Standards](../../reference/development/coding-standards.md) — general code style and conventions
