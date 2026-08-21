# Feature Spec: module-and-build

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

DiaMessageBus has no compiled build target or module documentation yet. Every downstream feature spec (core-bus, diagamemessages-format, schema-browser, etc.) must link against a `DiaMessageBus.vcxproj` static library; without it nothing can be built or tested. The module YAML establishes DiaMessageBus's place in the dependency graph and enables `dia check sln-sync` and `dia check deps` to enforce layer boundaries and detect import violations before implementation begins. This scaffold delivers the empty project shell — a compilable static lib, an `Cluiche.sln` registration, and a validated module doc — so that all subsequent features have a stable build foundation to target.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `Dia/DiaMessageBus/DiaMessageBus.vcxproj` exists, is a static library project, and compiles clean (0 errors, 0 warnings) from an empty translation unit in `Debug\|x64` | Run `msbuild DiaMessageBus.vcxproj /p:Configuration=Debug /p:Platform=x64`; confirm exit 0 |
| AC-2 | `DiaMessageBus.vcxproj` does not contain any of the five forbidden property overrides: `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, `LanguageStandard` | Inspect the vcxproj; none of those element names appear anywhere in the file |
| AC-3 | `DiaMessageBus` appears in `Cluiche.sln` nested under the solution folder that matches its `framework` layer assignment | Inspect the sln file; the project's `GlobalSection(NestedProjects)` entry resolves to the correct folder GUID |
| AC-4 | `dia check sln-sync` reports no layer-mismatch findings for `DiaMessageBus` | Run `dia check sln-sync`; output contains 0 errors referencing this project |
| AC-5 | `Dia/DiaMessageBus/dia.messagebus.architecture.module.md` exists with valid `dia.module.v1` YAML frontmatter containing all required fields (`schema`, `module_id`, `name`, `layer`, `status`, `path`, `language`, `summary`) | Run `dia_modules.py --validate`; no errors reported for `dia.messagebus` |
| AC-6 | The YAML `dependencies.required` list contains exactly these five entries: `dia.mailbox`, `dia.core`, `dia.application.flow`, `dia.streams`, `dia.observation` | Inspect the YAML frontmatter; all five IDs are present under `dependencies.required` |

## Design

### vcxproj structure

`DiaMessageBus.vcxproj` is a standard Dia static library project. It follows the same MSBuild skeleton as peer libraries such as `DiaMailbox.vcxproj`:

- `ConfigurationType`: `StaticLibrary`
- `Platform`: `x64` only (PD-005)
- **No property overrides for** `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` — these are inherited from `Directory.Build.props` (PD-008). Adding any of these is a hard build-system violation.
- `AdditionalIncludeDirectories` should include `$(SolutionDir)Dia/DiaMessageBus` and the include roots needed by the declared dependencies.
- A minimal stub `.cpp` (e.g. `DiaMessageBus.cpp` with a single `// placeholder` comment) ensures the static library produces an `.lib` artifact rather than an empty archive that some linkers reject.

`dia scaffold module Dia DiaMessageBus` generates the directory, the skeleton vcxproj, a companion vcxproj.filters file, and a stub `.cpp`. The scaffold output must be reviewed against AC-2 before marking the task complete.

### Cluiche.sln registration

DiaMessageBus sits at the `framework` layer (it depends on `DiaApplicationFlow`, itself a framework-layer module). The project must be placed in the solution folder assigned to that layer. `dia check sln-sync` validates this automatically; if the scaffold creates the sln entry in the wrong folder, edit the `GlobalSection(NestedProjects)` block to point at the correct folder GUID before committing.

### Module YAML (`dia.messagebus.architecture.module.md`)

The file lives at `Dia/DiaMessageBus/dia.messagebus.architecture.module.md`. Filename note: the system spec's Responsibilities section refers to `dia.messagingbus.architecture.module.md` (with "messaging"), but the canonical module ID is `dia.messagebus` (matching the directory `DiaMessageBus` and namespace `Dia::MessageBus`). Use `dia.messagebus` — not `dia.messagingbus` — everywhere.

Conforming YAML:

```yaml
---
schema: dia.module.v1
module_id: dia.messagebus
name: DiaMessageBus
owner_team: TBD
layer: framework
status: stub
maturity: dev

path: Dia/DiaMessageBus
language: cpp
parent_module_id: null

summary: >
  Typed message routing layer for gameplay systems and entity components on the sim thread.

intent: >
  Owns one Dia::Mailbox::Mailbox instance per MessageBusModule, registers BroadcastRouter
  and an EntityRouter registration point, and runs a two-pass per-tick flush
  (pre-Primary flush adapters → Primary pass → Reaction pass).
  The primary build surface is the generated RegisterMessages wiring produced by
  dia codegen messages from a .diagamemessages IDL document.

responsibilities:
  - Own one Dia::Mailbox::Mailbox instance per MessageBusModule (sim-thread-scoped)
  - Register BroadcastRouter and provide the EntityRouter registration point
  - Run two-pass flush per sim tick (pre-Primary, Primary, Reaction)
  - Provide Bus::Post<T>, Bus::Broadcast<T>, Bus::Subscribe<T>, Bus::RegisterType<T, kCapacity> API
  - Expose last-tick LedgerSnapshot via GetLastTickLedger() (double-buffered)
  - Implement IModule on SimPU (MessageBusModule)
  - Provide IFlushAdapter interface for physics and input adapters

non_responsibilities:
  - Message schema definitions (application layer — DiaGameplayMessages or equivalent)
  - EntityRouter implementation (owned by diaentitytemplate)
  - Cross-PU messaging (DiaStreams handles that boundary)
  - Thread safety (single-threaded sim thread only, per SD-MBX2-008)
  - Ledger history ring buffer and ServiceStream cross-PU export (deferred live-tooling follow-on, SD-MBX2-004)
  - EventDispatcher (removed in eventdispatcher-removal feature, SD-MBX2-006)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaMessageBus/Bus.h
    - Dia/DiaMessageBus/MessageBusModule.h
    - Dia/DiaMessageBus/IFlushAdapter.h
    - Dia/DiaMessageBus/LedgerSnapshot.h
    - Dia/DiaMessageBus/BusSubscriptionHandle.h
  namespaces:
    - Dia::MessageBus
  entry_points:
    - Bus
    - MessageBusModule
    - IFlushAdapter
    - LedgerSnapshot
    - BusSubscriptionHandle

dependencies:
  required:
    - dia.mailbox
    - dia.core
    - dia.application.flow
    - dia.streams
    - dia.observation
  optional: []
  forbidden:
    - dia.entity.template
    - dia.gameplay.messages
---
```

`dia_modules.py --validate` must pass with no errors before the task is marked Done. The `public_api` headers listed above are forward declarations of the eventual public surface; at the stub stage only the directory and the vcxproj exist — the headers themselves are created in the core-bus feature.

### Inherited constraints active for this feature

- **PD-005** — x64 only; no Win32 configuration block in the vcxproj.
- **PD-008** — Directory.Build.props owns build settings; the five forbidden overrides must be absent.
- **AD-001** — Module YAML required; the file must exist and validate before this feature is Done.
- **AD-003** — Namespace `Dia::MessageBus::` declared in the YAML `public_api.namespaces`.

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Run `dia scaffold module Dia DiaMessageBus` to create `Dia/DiaMessageBus/`, `DiaMessageBus.vcxproj`, `DiaMessageBus.vcxproj.filters`, a stub `.cpp`, and a skeleton module doc | Review generated vcxproj against AC-2 immediately after; fix any forbidden prop overrides before committing |
| 2 | Author `dia.messagebus.architecture.module.md`: fill YAML per the conforming example in the Design section; run `dia_modules.py --validate` and confirm 0 errors for `dia.messagebus` | Filename must be `dia.messagebus.architecture.module.md` — not `dia.messagingbus.*` |
| 3 | Register `DiaMessageBus.vcxproj` in `Cluiche.sln` under the `framework` layer solution folder; run `dia check sln-sync` and confirm 0 layer-mismatch findings | The scaffold may place the project in the sln automatically — verify the folder GUID is correct regardless |
| 4 | Build `DiaMessageBus` in `Debug\|x64` and confirm 0 errors, 0 warnings | Run `msbuild DiaMessageBus.vcxproj /p:Configuration=Debug /p:Platform=x64`; quote the result line in task notes |

## Status

`Approved`
