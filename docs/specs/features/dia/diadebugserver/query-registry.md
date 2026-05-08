# Feature Spec: QueryRegistry

## Parent System
@docs/specs/systems/dia/diadebugserver.md

## Traceability

| Level | Spec | Key Decisions Referenced |
|-------|------|------------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001 (StringCRC IDs), PD-004 (no STL in public APIs), PD-006 (vcxproj source of truth) |
| Application | @docs/specs/applications/dia.md | AD-001 (module system), AD-002 (no STL public APIs), AD-003 (namespace Dia::\<Module\>::) |
| System | @docs/specs/systems/dia/diadebugserver.md | DDS-001 (generic system), DDS-005 (JSON over WebSocket), DDS-008 (forward commands to DiaAPI), DDS-010 (JSON serialization) |

## Summary

A structured remote query dispatch system within DiaDebugServer that allows game-side domain modules to register JSON-in/JSON-out handlers. Unlike DiaAPI commands (which return an int exit code and log output), QueryRegistry handlers return full `Json::Value` payloads that are serialized into protobuf responses and sent to the editor. This separates the CLI concern (DiaAPI) from the remote-query concern (QueryRegistry) without polluting either layer.

## Problem Statement

The DiaAPI command system returns `int` exit codes and logs output via `DIA_LOG_INFO`. When the editor polls game-side data via `SendCommandWithResponse`, the `CommandDispatcher::ExecuteDiaAPICommand` path only returns `{success, message}` — no structured payload. The editor receives `success=true` but zero data. Domain modules need a way to serve structured JSON responses to remote editors without changing DiaAPI's CLI-oriented contract.

## Goals

1. Provide a query handler registry owned by DiaDebugServer with a clean `Json::Value(const Json::Value&)` handler signature
2. Establish dispatch priority: QueryRegistry > ProtocolCommands > DiaAPI fallback
3. Migrate existing protocol commands (get_state, list_commands, get_server_stats) into QueryRegistry
4. Enable domain modules (AssetRuntime) to register query handlers at startup
5. Keep DiaAPI completely unchanged — no transport/serialization concerns

## Non-Goals

- Modifying the DiaAPI CommandCallback signature
- Adding authentication or access control to queries
- Handling binary payloads (JSON only per DDS-005/DDS-010)
- Making QueryRegistry available outside DiaDebugServer

## API Design

### QueryRegistry Class

```cpp
namespace Dia::DebugServer {
    class QueryRegistry {
    public:
        using QueryHandler = std::function<Json::Value(const Json::Value& args)>;

        void Register(const Dia::Core::StringCRC& name, QueryHandler handler);
        void Unregister(const Dia::Core::StringCRC& name);
        bool Has(const Dia::Core::StringCRC& name) const;
        Json::Value Execute(const Dia::Core::StringCRC& name, const Json::Value& args) const;

    private:
        static const unsigned int kMaxHandlers = 64;
        struct Entry {
            Dia::Core::StringCRC name;
            QueryHandler handler;
        };
        Dia::Core::Containers::DynamicArrayC<Entry, kMaxHandlers> mHandlers;
    };
}
```

### DebugServerModule Additions

```cpp
// New public accessor
QueryRegistry& GetQueryRegistry();

// HandleCommand dispatch change (pseudocode):
// 1. if (mQueryRegistry.Has(commandName))  → execute, populate payload
// 2. else if (mCommandDispatcher.IsProtocolCommand(commandName))  → legacy path (to be removed)
// 3. else → ExecuteDiaAPICommand (fire-and-forget, success/message only)
```

### Registration Pattern (domain modules)

```cpp
// In AssetServiceModule::DoStart() or equivalent:
auto* debugServer = GetModule<DebugServerModule>();
if (debugServer) {
    debugServer->GetQueryRegistry().Register(
        Dia::Core::StringCRC("asset-runtime-get-all-states"),
        [&runtime](const Json::Value& /*args*/) -> Json::Value {
            // Build and return structured response
            Json::Value root;
            Json::Value assets(Json::arrayValue);
            // ... populate from runtime ...
            root["assets"] = assets;
            root["total"] = static_cast<int>(total);
            return root;
        });
}
```

## Acceptance Criteria

1. `QueryRegistry` class exists in `Dia/DiaDebugServer/` with Register/Unregister/Has/Execute API
2. Handler signature is `Json::Value(const Json::Value& args)`
3. `HandleCommand` dispatch priority: QueryRegistry → (legacy ProtocolCommands) → DiaAPI fallback
4. Existing protocol commands (get_state, list_commands, get_server_stats) migrated into QueryRegistry
5. After migration, the old `mProtocolHandlers` map in CommandDispatcher is removed
6. `DebugServerModule::GetQueryRegistry()` is public so external modules can register handlers
7. AssetRuntime query handlers registered: get-all-states, get-state, get-staged, get-loaded, get-stage-deps
8. Editor `AssetStateTablePanel` receives full JSON payload with asset data after polling
9. DiaAPI `CommandRegistry` and `CommandCallback` signature remain completely unchanged
10. Unit tests cover: handler registration, dispatch priority, execute returns correct payload, unknown command falls through to DiaAPI

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | Create `QueryRegistry.h/.cpp` in DiaDebugServer with Register/Unregister/Has/Execute | S |
| 2 | Add `GetQueryRegistry()` to DebugServerModule, add QueryRegistry member | S |
| 3 | Modify `HandleCommand` dispatch to check QueryRegistry first | S |
| 4 | Migrate get_state into QueryRegistry (remove from mProtocolHandlers) | S |
| 5 | Migrate list_commands into QueryRegistry | S |
| 6 | Migrate get_server_stats into QueryRegistry | S |
| 7 | Remove old ProtocolCommands infrastructure from CommandDispatcher | S |
| 8 | Register AssetRuntime query handlers (get-all-states, get-state, get-staged, get-loaded, get-stage-deps) | M |
| 9 | Update DiaDebugServer vcxproj/filters for new files | S |
| 10 | Write unit tests for QueryRegistry dispatch and payload correctness | M |
| 11 | Integration test: editor polls asset-runtime-get-all-states, receives data | M |

## Files Affected

| File | Change |
|------|--------|
| `Dia/DiaDebugServer/QueryRegistry.h` | Create — class declaration |
| `Dia/DiaDebugServer/QueryRegistry.cpp` | Create — implementation |
| `Dia/DiaDebugServer/DebugServerModule.h` | Modify — add QueryRegistry member, GetQueryRegistry() |
| `Dia/DiaDebugServer/DebugServerModule.cpp` | Modify — HandleCommand dispatch, migrate protocol commands |
| `Dia/DiaDebugServer/CommandDispatcher.h` | Modify — remove ProtocolCommand infrastructure |
| `Dia/DiaDebugServer/CommandDispatcher.cpp` | Modify — remove ProtocolCommand methods |
| `Dia/DiaDebugServer/DiaDebugServer.vcxproj` | Modify — add QueryRegistry files |
| `Dia/DiaDebugServer/DiaDebugServer.vcxproj.filters` | Modify — add QueryRegistry files |
| `Dia/DiaAssetRuntime/AssetRuntimeDebugCommands.h` | Modify — add RegisterAssetRuntimeQueryHandlers declaration |
| `Dia/DiaAssetRuntime/AssetRuntimeDebugCommands.cpp` | Modify — add query handler registrations |
| `Cluiche/CluicheTest/` (AssetServiceModule) | Modify — call RegisterAssetRuntimeQueryHandlers if DebugServer available |
| `Cluiche/Tests/GoogleTests/` | Modify/Create — QueryRegistry unit tests |

## Binding Decisions Compliance

| Source | ID | Decision | Compliance |
|--------|-----|----------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | **Compliant.** QueryRegistry keys are `Dia::Core::StringCRC`. Handler lookup uses CRC comparison. |
| Platform | PD-004 | No STL containers in public APIs | **Compliant.** Public API uses `DynamicArrayC` for internal storage. `std::function` is used for the handler type (same pattern as existing `ProtocolCommandHandler` in the system spec). |
| Platform | PD-005 | x64 only | **Compliant.** No platform-specific code. |
| Platform | PD-006 | Visual Studio project files are source of truth | **Compliant.** New files added to vcxproj. |
| Platform | PD-007 | C++20 required | **Compliant.** No C++20 features required but compatible. |
| Platform | PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| Dia | AD-001 | Module system with YAML frontmatter | **Compliant.** QueryRegistry is an internal class of DiaDebugServer module, not a new module. |
| Dia | AD-002 | No STL in public APIs | **Compliant.** Same note as PD-004. `std::function` matches existing system spec pattern for handlers. |
| Dia | AD-003 | Namespace: `Dia::<Module>::` | **Compliant.** `Dia::DebugServer::QueryRegistry` |
| System | DDS-001 | Generic system, not app-specific | **Compliant.** QueryRegistry is domain-agnostic. Any module can register handlers. |
| System | DDS-005 | JSON over WebSocket | **Compliant.** Handlers receive and return `Json::Value`. |
| System | DDS-008 | Forward commands to DiaAPI CommandRegistry | **Compliant.** DiaAPI remains the fallback path when no query handler matches. |
| System | DDS-010 | JSON serialization for all messages | **Compliant.** All payloads are JSON. |

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Thread Safety | QueryRegistry handlers are called on the main game thread (inside DoUpdate/HandleMessage). Should we mutex-protect the handler map for registration from other threads? | No — registration happens in DoStart (single-threaded init), execution in DoUpdate (same thread). No mutex needed. | Registration occurs during module DoStart (sequential, same thread as DoUpdate). No mutex needed. |
| 2 | Error Handling | What happens if a query handler throws or returns null? | Wrap in try/catch, return `{success: false, error: "handler failed"}`. | Handler execution is wrapped; on failure, response has success=false with error message. |
| 3 | Naming | Should query handler names use hyphens (matching DiaAPI convention) or dots? | Hyphens — `asset-runtime-get-all-states`. Matches existing DiaAPI command naming. | Hyphens. Consistent with DiaAPI command naming convention. |
| 4 | Migration | After migrating protocol commands to QueryRegistry, should we keep the ProtocolCommand infrastructure for backward compatibility? | No — remove it entirely. No external consumers. Clean break. | Remove entirely. No external consumers exist. |
| 5 | DiaAssetRuntime dependency | DiaAssetRuntime doesn't currently depend on DiaDebugServer. How do we register query handlers without adding a hard dependency? | Registration call site is in the game application (AssetServiceModule in CluicheTest), not in DiaAssetRuntime itself. DiaAssetRuntime provides a registration function that takes a QueryRegistry reference. | The registration function lives in DiaAssetRuntime and takes `QueryRegistry&` as a parameter. The call site (AssetServiceModule) has visibility to both. DiaAssetRuntime gains a dependency on DiaDebugServer for the QueryRegistry type. |
| 6 | Capacity | 64 max handlers sufficient? | Yes — current count is 3 protocol + 5 asset runtime = 8. 64 provides ample headroom. | 64 is sufficient. |

## Open Questions

None — all resolved during interview and review.

## Status

`Approved`

**Plan:** @docs/specs/features/dia/diadebugserver/query-registry.plan.md
