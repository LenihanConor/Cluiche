**Spec:** @docs/specs/applications/dia/systems/diadebugserver/sim-registered-queries.md
**Status:** Done

## Task 1 audit: every `QueryRegistry` registrant and its PU

| Registrant | Registers on | Runs on (PU) | Reads Sim state? | Classification |
|---|---|---|---|---|
| `ValueSchemaModule::RegisterQuery` (DiaValueSchema) | `QueryRegistry` | `kAny`; both manifests (cluiche_main, cow_main) place it on Main with `DebugServerHostModule` | No (schema service snapshot) | Main-only. No migration. The Task 2 owner-thread check guards a future Sim placement |
| `DebugPanelPageModule` (`debug.domain_state`) | `QueryRegistry` | `kMain` | Cross-PU read is separate debt | Out of scope (Debug Domain State Snapshots) |
| `EntityInspectorModule` (`kEntityInspectRequest`) | `QueryRegistry` via `mDebugServer` pointer from `DebugServerService` stream | `kSim` (EntityTestStage) | Yes: `mEntityRef`, Entity domain | **Migrate** to `SimQueryHostModule::RegisterQuery` (real violation: registers from Sim into an unlocked registry, handler runs on Main) |
| `RegisterAssetRuntimeQueryHandlers` (DiaAssetRuntime) | `QueryRegistry&` param | No caller anywhere in the repo | n/a | Dead code. Decide: migrate the signature anyway, or delete |
| `OnDemandSourceBase::Activate` (DiaDebugServer) | `QueryRegistry` | No subclass exists; Economy sources derive `ChangeDetectedSourceBase` | n/a | Spec's "Economy on-demand sources" premise is wrong; nothing to migrate. Base class has no users |
| CluicheTest CellLayersViewer stage | `SimQueryHost` | Sim | Yes | Already sim-hosted. No work |
| `dia.*` tests registering on `GetQueryRegistry()` | `QueryRegistry` | test thread | n/a | Owner-thread check must accept them (AC 5: no owner set = any thread) |

Open Question 1 (mixed Main+Sim state): `EntityInspectorModule` query reads only Sim state; no split needed. Its `write_field` / `find_by_name` use `Dia::API::RegisterCommandJson` (a different registry), out of scope here but worth a backlog note.

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Audit all callers and owning PUs; write the plan | Table above | Done | sonnet | Scope shrinks: one real migration (EntityInspectorModule), two dead/absent registrants |
| 2 | `QueryRegistry::SetOwnerThread` + Register/Unregister owner check; DebugServer sets on Start, clears on Stop; metric `dia.debugserver.query.wrong_thread_register` | RED-GREEN: AC 3, 4, 5, 9 (std::thread test) | Done | sonnet | Dia thread-id type, not `std::thread::id` (AD-002); 8 tests RED (compile) then GREEN; mutation check fails 2+ tests. Id type is uint32_t via QueryRegistry::ThreadId (no Dia::Core::ThreadId exists). Register/Unregister now return bool |
| 3 | Migrate `EntityInspectorModule` query to `SimQueryHostModule` (add dependency + manifest entry, description required); golden-compare reply shape | AC 2, 6 | Done | sonnet | Stage-scoped: Unregister on stop via `UnregisterQuery`; entity.inspect_request now sim-hosted; null replies became {} (sim-hosted replies must be objects). Manifest: dropped DebugServerService channel, added SimQueryHostModule dependency. New E2E test_entity_inspect_sim_hosted.py (2 tests, in default plan) passes |
| 4 | Resolve dead code: `RegisterAssetRuntimeQueryHandlers` and `OnDemandSourceBase` (delete or leave) | AC 1, 7 | Done | haiku | User decision needed; Deleted RegisterAssetRuntimeQueryHandlers and OnDemandSourceBase (no callers/subclasses) |
| 5 | Exhaustive test pass (gap audit, edge tests, mutation check, sanitizer) | Whole feature | Done | sonnet | Full googletest 6.4k tests pass except CommandRegistryTest.ListCommandsEmpty (global registry order dependence in a shard; passes alone, unrelated); ASan 222 scoped pass; deps OK; mutation check done |
| 6 | Docs: module docs, registry, amend spec in place, close backlog row | AC 10 | Done | haiku | Use `dia docs` commands; module doc, spec amended, registry regenerated, backlog row closed |
