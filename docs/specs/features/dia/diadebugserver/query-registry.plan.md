# Implementation Plan: QueryRegistry

**Spec:** @docs/specs/features/dia/diadebugserver/query-registry.md

## Session Notes

**Spec decisions summary:** QueryRegistry is a JSON-in/JSON-out handler dispatch system owned by DiaDebugServer. Dispatch priority: QueryRegistry > legacy ProtocolCommands > DiaAPI fallback. Handler signature: `Json::Value(const Json::Value&)`. Names use hyphens (DiaAPI convention). Existing protocol commands (get_state, list_commands, get_server_stats) migrate into QueryRegistry; old ProtocolCommands infrastructure removed. DiaAssetRuntime provides a `RegisterAssetRuntimeQueryHandlers(QueryRegistry&, AssetRuntime&)` function. The call site is in the game application (AssetServiceModule or MainBootStrapPhase). DiaAPI remains completely unchanged.

**Key constraint:** PD-004 (no STL in public API) — `std::function` is acceptable for handler types per existing system pattern (ProtocolCommandHandler already uses it). Internal storage uses `DynamicArrayC<Entry, 64>`.

**Discovery:** `RegisterAssetRuntimeCommands` is never called in the game — only in tests. The query handler registration must also wire up the DiaAPI commands so both CLI and remote query paths work.

## Implementation Patterns

### Phase 1: QueryRegistry Class (Tasks 1-2)

**Pattern:** Flat array registry with StringCRC key lookup. Matches existing patterns in EditorPluginRegistry, CommandRegistry.

```cpp
// QueryRegistry.h - in Dia::DebugServer namespace
class QueryRegistry {
public:
    using QueryHandler = std::function<Json::Value(const Json::Value& args)>;
    void Register(const Dia::Core::StringCRC& name, QueryHandler handler);
    void Unregister(const Dia::Core::StringCRC& name);
    bool Has(const Dia::Core::StringCRC& name) const;
    Json::Value Execute(const Dia::Core::StringCRC& name, const Json::Value& args) const;
private:
    struct Entry { Dia::Core::StringCRC name; QueryHandler handler; };
    static const unsigned int kMaxHandlers = 64;
    Dia::Core::Containers::DynamicArrayC<Entry, kMaxHandlers> mHandlers;
};
```

DebugServerModule gains: `QueryRegistry mQueryRegistry;` member, `QueryRegistry& GetQueryRegistry() { return mQueryRegistry; }` accessor.

### Phase 2: Dispatch Integration (Task 3)

**Pattern:** In `HandleCommand()`, insert QueryRegistry check before the existing protocol command check:

```cpp
if (mQueryRegistry.Has(commandName)) {
    Json::Value result = mQueryRegistry.Execute(commandName, payload);
    resp->set_success(true);
    Dia::Proto::JsonValueToProtoStruct(result, resp->mutable_payload());
} else if (mCommandDispatcher.IsProtocolCommand(commandName)) {
    // existing path (to be removed in Phase 3)
} else {
    // DiaAPI fallback
}
```

### Phase 3: Protocol Command Migration (Tasks 4-7)

**Pattern:** Move the three lambda bodies from `RegisterProtocolCommands()` into `RegisterQueryHandlers()` on DebugServerModule. The lambdas change signature from `void(Json::Value&, CommandResponse*)` to `Json::Value(const Json::Value&)` — they return the payload directly instead of writing to a protobuf response.

After migration, remove: `CommandDispatcher::mProtocolHandlers`, `RegisterProtocolCommand`, `UnregisterProtocolCommand`, `IsProtocolCommand`, `ExecuteProtocolCommand`. CommandDispatcher becomes solely the DiaAPI fallback (`ExecuteDiaAPICommand` only).

### Phase 4: AssetRuntime Query Handlers (Task 8)

**Pattern:** New function in `DiaAssetRuntime/AssetRuntimeDebugCommands.h`:

```cpp
namespace Dia::AssetRuntime {
    void RegisterAssetRuntimeQueryHandlers(Dia::DebugServer::QueryRegistry& registry, AssetRuntime& runtime);
}
```

Each handler calls existing AssetRuntime API methods (GetAllAssets, GetAssetState, etc.) and builds a Json::Value response. Same logic as existing CLI command callbacks but returning JSON instead of logging.

**Call site:** `MainBootStrapPhase::AfterModulesStart()` — DebugServerModule and AssetServiceModule are both available at this point. Also call `RegisterAssetRuntimeCommands` for CLI parity.

### Phase 5: Project Files and Tests (Tasks 9-11)

**Test pattern:** GoogleTest fixture with a QueryRegistry instance. Tests: Register/Has/Execute, unknown name returns empty, dispatch priority (QueryRegistry beats DiaAPI for same name).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create QueryRegistry.h/.cpp with Register/Unregister/Has/Execute | Unit test: register, has, execute returns correct value | Todo | sonnet | |
| 2 | Add QueryRegistry member + GetQueryRegistry() to DebugServerModule.h | Compile check | Todo | haiku | |
| 3 | Modify HandleCommand dispatch: QueryRegistry first, then protocol, then DiaAPI | Integration: send known query name, verify payload in response | Todo | sonnet | |
| 4 | Migrate get_state to QueryRegistry | Existing behavior preserved (editor get_state still works) | Todo | sonnet | |
| 5 | Migrate list_commands to QueryRegistry | list_commands returns protocol + api commands | Todo | sonnet | |
| 6 | Migrate get_server_stats to QueryRegistry | get_server_stats returns stats payload | Todo | sonnet | |
| 7 | Remove ProtocolCommand infrastructure from CommandDispatcher | Compile clean, no references remain | Todo | sonnet | |
| 8 | Add RegisterAssetRuntimeQueryHandlers + wire up in MainBootStrapPhase | dia run googletest --filter="AssetRuntimeQuery*" | Todo | sonnet | Also call RegisterAssetRuntimeCommands for CLI |
| 9 | Update DiaDebugServer + DiaAssetRuntime vcxproj/filters | Build succeeds | Todo | haiku | |
| 10 | Write QueryRegistry unit tests (dispatch priority, error cases) | dia run googletest --filter="QueryRegistry*" | Todo | sonnet | |
| 11 | End-to-end: editor polls asset-runtime-get-all-states, receives data | Manual: launch editor + game, verify asset table populates | Todo | opus | Verify with editor launch |
