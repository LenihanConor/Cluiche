# Feature Spec: Sim-Hosted Command Help

## Parent System
@docs/specs/applications/dia/systems/diadebugserver/diadebugserver.md

**Prerequisite:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-queries.md (Done).
**Origin:** DiaCellLayers Streams Migration, task 3 (`docs/specs/applications/dia/systems/diacelllayers/streams-migration.plan.md`) and the backlog row "Sim-hosted commands carry no help metadata (2026-11-01)".
**AD-001 sign-off:** given by the user on 2026-11-01 for the DiaDebugServer and `SimQueryHostModule` changes. Any change to another Dia project (see Open Design Question 1) needs its own sign-off.

## Summary

A name registered on `SimQueryHost` (query or command) carries the same help metadata a `Dia::API::CommandInfoJson` carries today: description, category, owner, `ValueParam`s and `noArgs`. The metadata travels from SimPU to Main inside the existing registration message, is kept by the Main-side DebugServer in a **sim-hosted catalogue**, and is read by the three consumers of command help: the DebugServer command list, the debug-panel palette, and the DiaValueSchema code-side collector (`dia validate schema`). A description is mandatory: registration without one is rejected.

## Problem Statement

When the DiaCellLayers viewer commands moved from the DiaAPI `CommandRegistry` to the sim-hosted path, they lost their descriptions, categories and parameter help, because `SimQueryHost::RegisterCommand(name, handler)` only takes a name and a handler. `--help`, the palette and `dia validate schema` no longer know the 20 viewer commands and 4 mirror queries, and nothing tests for the loss. The same will happen to every future sim-hosted name unless the API makes help impossible to forget.

## Goals

1. `SimQueryHost::RegisterQuery` and `RegisterCommand` (and the `SimQueryHostModule` wrappers) take a help info; the same struct shape serves queries and commands.
2. The info is announced to Main in the register message and stored in a Main-side catalogue; unregistering removes it.
3. A missing description is rejected at registration (returns false, logs an Error), so help cannot be forgotten.
4. The command list, palette and schema collector include sim-hosted names with their metadata, merged with the DiaAPI list; sim-hosted names still do **not** enter the DiaAPI `CommandRegistry` (Streams Migration decision stands).
5. The DiaCellLayers names (4 mirror queries, 17 viewer commands, 3 pointer-injection names) and the `sim.echo` test name are described.

## Non-Goals

- Moving sim-hosted names into the DiaAPI `CommandRegistry` (rejected: see Resolved Design Questions).
- Changing reply wire format or the success/error convention of sim-hosted replies.
- Help for Main-hosted `QueryRegistry` handlers (unchanged).
- Raising the 32-name cap or the `SimQueryReply` capacity (tracked separately; this feature must stay within them and test the cap).
- Localisation or rich-text help.

## API Design

```cpp
namespace Dia::DebugServer {
    // Help for one sim-hosted name. Strings are copied at registration; the caller's storage need not outlive the call.
    struct SimHelpInfo {
        const char*                description = nullptr;   // required, non-empty
        const char*                category    = nullptr;   // optional, e.g. "cells"
        const char*                owner       = nullptr;   // optional, e.g. "DiaCellLayers"
        const Dia::API::ValueParam* params     = nullptr;   // optional; same struct DiaAPI commands use
        unsigned int               paramCount  = 0;
        bool                       noArgs      = false;
    };

    class SimQueryHost {
    public:
        bool RegisterQuery  (StringCRC name, const SimHelpInfo& help, QueryHandler handler);
        bool RegisterCommand(StringCRC name, const SimHelpInfo& help, QueryHandler handler);
        // The two-argument overloads are removed (no way to register without help).
    };
}

// Cluiche::AppFlow::SimQueryHostModule::RegisterQuery/RegisterCommand(const char* name, const SimHelpInfo&, handler)
// Registration message: {"register":{"name":..,"kind":..,"info":{"description":..,"category":..,"owner":..,
//                         "noArgs":false,"params":[{"name":..,"type":..,"required":..,"description":..,"unit":..,"min":..,"max":..,"enum":..}]}}}
```

Main side: `DebugServer::RegisterSimHosted(name, kind, info)` stores the info in a fixed-size catalogue (one entry per sim name, same 32-name cap); `UnregisterSimHosted` removes it. A read accessor lists catalogue entries in the same record shape the three consumers already use for `Dia::API::CommandInfoJson`.

Limits (fixed, no heap): description 256 bytes, category and owner 64, up to 16 params per name, each param name 64, type 16, description 192, unit 16, enum list 128. The whole register message must fit the 16 KB event; over-limit info is rejected at registration with an Error, never truncated silently.

## Acceptance Criteria

1. `SimQueryHost` has no registration overload without a `SimHelpInfo`; registering with a null or empty `description` returns false, registers nothing and logs an Error naming the name.
2. A successful registration stores the info (copied); mutating or freeing the caller's strings afterwards does not change the stored help.
3. The register message carries the info; `ParseSimRegistrationReply` returns it, and a message with a missing `info` or description is `Malformed` (not registered on Main).
4. `DebugServer::RegisterSimHosted` stores the info in the catalogue; `UnregisterSimHosted` removes it; re-registering a name replaces its entry; a full catalogue (32) rejects with a Warning.
5. The DebugServer command list (today `ListCommandInfos`-based) returns sim-hosted names with description, category, owner and params, merged with DiaAPI commands, with no duplicates; a name present in both is reported once, from the Main-hosted entry, with a Warning.
6. The debug-panel palette index includes sim-hosted names with params (typed arguments as for DiaAPI commands).
7. `dia validate schema`'s code-side collection includes sim-hosted names; a sim-hosted name without a described param value schema is listed as undescribed exactly as a DiaAPI one is.
8. Limit handling: description over 256 bytes, more than 16 params, or an over-long param string is rejected at registration (false + Error); a maximal valid info round-trips through the register message and the catalogue unchanged.
9. Sim-hosted names are still absent from the DiaAPI `CommandRegistry` (`GetCommand` returns null for them).
10. Every name registered by `CellLayersViewerTestStageModule` (24) and by `SimEchoTestModule` has a non-empty description and param help matching the Task 3 comment block in `CellLayerViewerDomain`; a unit test enumerates the registrar's names and fails if any lacks a description.
11. Wire E2E: asking the running game for the help of a viewer command returns a non-empty description and its params; the existing celllayers E2E still passes.
12. Unregister on stage teardown removes the catalogue entries (no help for a name whose handler is gone).

## Tasks

| # | Task | Size |
|---|------|------|
| 1 | `SimHelpInfo` + limits + `SimQueryHost` overloads (remove the old ones); handler-side tests (AC 1, 2, 8) | M |
| 2 | Register-message `info` encode/decode in `SimRegistrationMessage.h`, `SimQueryHostModule` wrappers and announce path (AC 3); size cap for 24 names within the 16 KB event | M |
| 3 | Main catalogue in `DebugServer` (`RegisterSimHosted(name, kind, info)`, unregister, list accessor); `DebugServerHostModule` wiring (AC 4, 9, 12) | M |
| 4 | Consumers: DebugServer command list, palette index, schema collector read the merged list (AC 5, 6, 7) | L |
| 5 | Describe the DiaCellLayers names and `sim.echo`; description-required unit test over the registrar (AC 10) | M |
| 6 | Wire E2E for help (AC 11); module docs; backlog row closed; exhaustive unit + integration test pass | M |

## Files Affected (expected)

`Dia/DiaDebugServer/SimQueryHost.{h,cpp}`, `SimHostedTypes.h`, `DebugServer.{h,cpp}`; `Cluiche/CluicheGameBaseline/Modules/SimQueryHostModule.{h,cpp}`, `DebugServerHostModule.{h,cpp}`, `Types/SimRegistrationMessage.h`; consumers `Dia/DiaVisualDebugger/Console/PaletteIndex.cpp`, `Dia/DiaValueSchema/CodeSideCollector.cpp` (see Open Design Question 1); `Dia/DiaCellLayers/MirrorQueries.{h,cpp}`, `CellLayerViewerDomain.cpp`, `Testing/FakeMirrorRegistrar.h`; `Cluiche/CluicheTest/Modules/SimEchoTestModule.cpp`, `CellLayersViewerTestStageModule.cpp`, `CellLayersPointerQueue.h`; tests under `Cluiche/Tests/GoogleTests/DebugServer`, `CluicheGameBaseline`, `DiaCellLayers`, `DiaVisualDebugger`, `DiaValueSchema`; E2E `Cluiche/Tests/E2E/scenarios/cluichetest/celllayers/`.

## Binding Decisions

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Dia DebugServer | DDS-001 | Generic server, not app-specific | `SimHelpInfo` is generic; no game types |
| Dia DebugServer | DDS-005, DDS-010 | JSON over WebSocket, JSON messages | Help travels as JSON in the existing register message |
| Dia DebugServer | DDS-008 | Server forwards commands to the DiaAPI `CommandRegistry` | **Narrowed, not broken:** sim-hosted names were already excluded from `CommandRegistry` by Sim-Hosted Queries; their help now lives in the catalogue. Flagged here so the system spec's DDS-008 can be read with that exception |
| Dia | AD-002 | No STL in public APIs | `SimHelpInfo` uses `const char*` and `ValueParam*`; fixed buffers in the catalogue. `QueryHandler` is already `std::function` (existing) |
| Platform rule | CLAUDE.md Thread Safety | PU boundaries are crossed by streams only | Help crosses in the register message over the existing `SimQueryReply` stream; the catalogue is Main-owned, no mutex across PUs |
| Sim-Hosted Queries | AC 11 | No mutex for the feature; 16 KB event cap | Info size limits keep the register message under the cap |

## Open Design Questions

1. **Where do the three consumers read the catalogue without a new layering dependency?** `PaletteIndex` (DiaVisualDebugger) and `CodeSideCollector` (DiaValueSchema) today read `Dia::API::ListCommandInfos`; neither depends on DiaDebugServer. Options: (a) a small read-only catalogue interface in DiaAPI (a header in another Dia project: needs its own AD-001 sign-off, and keeps consumers unchanged apart from merging), (b) the DebugServer pushes catalogue entries to the consumers through an existing stream or registry, (c) the consumers take a dependency on DiaDebugServer (check `dia check deps` layer rules first). Recommendation to evaluate at Task 4: (a). Decide before Task 4 and record in the plan; if (a), get the sign-off first.
2. **Does the schema collector need help for names registered after it ran?** `dia schema export` runs in describe mode at startup; sim-hosted names register in `OnStart` on SimPU and are announced over a stream, so the collector may run before the catalogue is filled. Task 4 must check the describe barrier and either wait on the announcement or have the collector ask the sim side; unproven.
3. **Replacing a name's help on re-registration vs rejecting a duplicate.** The sim host rejects duplicate names today and the stage unregisters on teardown; the catalogue replaces on re-register (AC 4). If a stale entry can survive a missed unregister (crash path), help for a dead name would be listed. Decide whether the catalogue should also expire entries when `SimQueryHost` reports the name gone.

## Resolved Design Questions

1. **Main-side home (user, 2026-11-01):** a separate sim-hosted catalogue, not stub entries in `CommandRegistry`. Keeps the Streams Migration rule and the `QueryRegistry` veto unchanged.
2. **Mandatory description (user, 2026-11-01):** registration is rejected without one. Existing callers are updated in this feature (Task 5).
3. **Scope (user, 2026-11-01):** queries and commands, with the viewer descriptions written in this feature.
4. **Transport (user, 2026-11-01):** inside the register message (an `info` object), one event per name, no new stream.

## Status

`Draft`
