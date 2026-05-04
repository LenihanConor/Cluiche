# Feature Spec: DiaAPI Debug Commands

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaAssetRuntime | @docs/specs/systems/dia/diaassetruntime.md |
| Feature | DiaAPI Debug Commands | (this document) |

## Summary

Registers DiaAPI commands that expose DiaAssetRuntime's state to editors and debug tools over WebSocket via DiaDebugServer. Commands provide JSON-formatted responses for loaded/staged asset lists, per-asset state queries, stage dependency lookups, full runtime snapshots, and a push-based state transition event stream. This is the remote-inspection layer that enables a live editor view of asset runtime state without coupling the editor to DiaAssetRuntime's C++ API.

## Problem

DiaAssetRuntime's debug query API (Feature 5) is a C++ in-process API. External tools -- editors, web dashboards, automated monitoring scripts -- cannot access it without a network bridge. DiaAPI provides a command registration framework and DiaDebugServer provides the WebSocket transport. This feature connects the two by registering commands that translate HTTP/WebSocket requests into Feature 5 query calls and return JSON responses.

## Acceptance Criteria

1. All commands are registered under the `asset_runtime` namespace in DiaAPI
2. `asset_runtime.get_loaded` returns a JSON array of StringCRC asset IDs currently in `Loaded` state
3. `asset_runtime.get_staged` returns a JSON array of StringCRC asset IDs currently in `Staged` state
4. `asset_runtime.get_state { assetId }` returns a JSON object with the asset's state, scope, ref count, and deploy path. Returns an error response for unknown asset IDs.
5. `asset_runtime.get_stage_deps { stageId }` returns a JSON array of asset IDs belonging to the specified Stage. Returns an error response for unknown stage IDs.
6. `asset_runtime.get_all_states` returns a JSON array of all assets with their state, scope, ref count, and deploy path -- a full runtime snapshot
7. `asset_runtime.subscribe_transitions` establishes a push event stream. Each state transition (asset ID, old state, new state, ref count) is pushed as a JSON message to the subscriber. Subscription is per-connection.
8. All commands use DiaCore containers for internal processing and serialize to JSON for the response
9. Command registration happens once via a single `RegisterAssetRuntimeCommands(DiaAPI&, AssetRuntime&)` function
10. Commands are read-only -- they do not modify DiaAssetRuntime state

## API Design

### Command Registration

```cpp
namespace Dia::AssetRuntime
{
    void RegisterAssetRuntimeCommands(Dia::API::DiaAPI& api,
                                      AssetRuntime& runtime);
}
```

### Command Table

| Command | Parameters | Response |
|---------|-----------|----------|
| `asset_runtime.get_loaded` | (none) | `{ "assets": ["texture.player_ship", ...] }` |
| `asset_runtime.get_staged` | (none) | `{ "assets": ["texture.enemy_boss", ...] }` |
| `asset_runtime.get_state` | `{ "assetId": "texture.player_ship" }` | `{ "assetId": "...", "state": "Loaded", "scope": "stage", "refCount": 1, "deployPath": "..." }` |
| `asset_runtime.get_stage_deps` | `{ "stageId": "stage.gameplay" }` | `{ "stageId": "...", "assets": ["texture.player_ship", ...] }` |
| `asset_runtime.get_all_states` | (none) | `{ "assets": [{ "assetId": "...", "state": "...", "scope": "...", "refCount": 0, "deployPath": "..." }, ...] }` |
| `asset_runtime.subscribe_transitions` | (none) | Push stream: `{ "assetId": "...", "oldState": "Registered", "newState": "Staged", "refCount": 1 }` |

### Error Responses

```json
{
    "error": "unknown_asset",
    "message": "Asset ID 'texture.unknown' not found in runtime"
}
```

### Usage Pattern

```javascript
// From a web editor (JavaScript via WebSocket)
ws.send(JSON.stringify({ command: "asset_runtime.get_loaded" }));
// Response: { "assets": ["texture.player_ship", "config.ship_stats"] }

ws.send(JSON.stringify({ command: "asset_runtime.get_state", params: { assetId: "texture.player_ship" } }));
// Response: { "assetId": "texture.player_ship", "state": "Loaded", "scope": "stage", "refCount": 1, "deployPath": "C:/Game/bin/.../player_ship.texture.png" }

ws.send(JSON.stringify({ command: "asset_runtime.subscribe_transitions" }));
// Push events arrive as state changes happen
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Register get_loaded and get_staged commands | DiaAPI command handlers that call GetLoadedAssets/GetStagedAssets (Feature 5) and serialize results to JSON array |
| 2 | Register get_state command | DiaAPI command handler that parses assetId parameter, calls GetAssetState/GetAssetRefCount/ResolveAssetPath, serializes to JSON object. Error response for unknown IDs. |
| 3 | Register get_stage_deps command | DiaAPI command handler that parses stageId parameter, calls GetStageDependencies (Feature 5), serializes to JSON array. Error response for unknown stages. |
| 4 | Register get_all_states command | DiaAPI command handler that iterates all assets, collects state/scope/refCount/deployPath, serializes to JSON array |
| 5 | Register subscribe_transitions (push event stream) | Implement an internal IAssetStateListener that forwards transition events to subscribed DiaAPI connections as JSON messages. Subscription is per-connection; unsubscribe on disconnect. |
| 6 | Add GoogleTest coverage | Tests for: command registration, get_loaded JSON response format, get_state with valid/invalid asset ID, get_stage_deps with valid/invalid stage ID, get_all_states snapshot completeness, subscribe_transitions event format |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaAssetRuntime -- Manifest Load & Path Resolution (Feature 1) | RuntimeAssetEntry for deploy path and scope in get_state/get_all_states |
| DiaAssetRuntime -- Asset State Machine (Feature 2) | GetAssetState for per-asset state queries |
| DiaAssetRuntime -- Stage Lifecycle & Reference Counting (Feature 3) | GetAssetRefCount for ref count in responses |
| DiaAssetRuntime -- Event Notification (Feature 4) | IAssetStateListener for subscribe_transitions push stream |
| DiaAssetRuntime -- Debug Query API (Feature 5) | GetLoadedAssets, GetStagedAssets, GetStageDependencies for bulk queries |
| DiaAPI | Command registration framework, DiaAPI class, command handler interface |
| DiaCore JSON | jsoncpp for serializing responses |
| DiaCore CRC/StringCRC | Asset IDs and Stage IDs |
| DiaCore Containers | DynamicArrayC for intermediate query results |

## Files

| File | Action |
|------|--------|
| `Dia/DiaAssetRuntime/AssetRuntimeDebugCommands.h` | Create -- RegisterAssetRuntimeCommands declaration |
| `Dia/DiaAssetRuntime/AssetRuntimeDebugCommands.cpp` | Create -- command handler implementations and registration |
| `Dia/DiaAssetRuntime/DiaAssetRuntime.vcxproj` | Modify -- add new files, add DiaAPI project reference |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Compliant.** Asset IDs and Stage IDs are StringCRC internally. JSON responses serialize them as human-readable strings for editor consumption. |
| PD-004 | No STL in public APIs | **Compliant.** RegisterAssetRuntimeCommands takes DiaAPI& and AssetRuntime& references. JSON serialization uses DiaCore JSON (jsoncpp wrapper). No STL in public interface. |
| PD-005 | x64 Windows only | **Compliant.** No platform-specific code. |
| PD-007 | C++20 required | **Compliant.** Available but no specific C++20 features required. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj build setting overrides. |
| PD-009 | Generated output under Cluiche/out/ | **Not applicable.** No generated output. |
| AD-001 | YAML frontmatter module docs | **Compliant.** Module doc created in Feature 1. |
| AD-002 | No STL in public APIs | **Compliant.** Same as PD-004. |
| AD-003 | Namespace Dia::AssetRuntime:: | **Compliant.** All code under `Dia::AssetRuntime::`. |
| SD-CAT-001 | Asset IDs are type.name composites | **Compliant.** JSON responses use type.name strings. |
| SD-ARUN-001 | No DiaApplication dependency | **Compliant.** DiaAPI is a separate framework from DiaApplication. Command registration does not introduce a DiaApplication dependency. |
| SD-ARUN-008 | No DiaAssetCatalogue dependency | **Compliant.** Commands query DiaAssetRuntime state only. No imports from DiaAssetCatalogue. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Dependencies | This feature adds a DiaAPI dependency to DiaAssetRuntime. Is that acceptable given SD-ARUN-001 (no DiaApplication dependency)? | Yes. DiaAPI is the plugin-based CLI/command framework, separate from DiaApplication. It does not introduce lifecycle coupling. The dependency is optional -- if a game does not use DiaAPI, these commands are simply not registered. |
| 2 | subscribe_transitions | How does the subscription listener handle multiple WebSocket connections subscribing simultaneously? | Each connection gets its own subscription. The internal IAssetStateListener multiplexes events to all active subscribers. On disconnect, the subscription is cleaned up. The listener list capacity (Feature 4's DynamicArrayC<IAssetStateListener*, 16>) is shared with game-code listeners. |
| 3 | Performance | get_all_states iterates all 512 assets. Is that acceptable for a debug command called over WebSocket? | Yes. 512 entries with JSON serialization is fast (sub-millisecond). This is a debug tool, not a hot path. The WebSocket transport latency dominates. |
| 4 | Serialization | Should StringCRC values in JSON responses be human-readable strings or numeric CRC values? | Human-readable strings. The editor needs to display asset names, not CRC hashes. The runtime manifest already contains the string form of IDs, so they are available. |
| 5 | Error handling | What if DiaAPI is not available (e.g., headless build without DiaAPI)? | RegisterAssetRuntimeCommands is called explicitly by game code. If the game does not call it, no commands are registered. DiaAssetRuntime has no implicit dependency on DiaAPI at startup. The header and implementation files can be excluded from builds that do not link DiaAPI. |
| 6 | Security | Should debug commands be restricted in release builds? | Not at this layer. DiaAPI and DiaDebugServer own access control. DiaAssetRuntime registers read-only commands and trusts DiaAPI to gate access. If DiaDebugServer is not started in a release build, the commands are unreachable. |

## Status

`Approved`
