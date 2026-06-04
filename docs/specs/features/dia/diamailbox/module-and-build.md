# Feature Spec: DiaMailbox — Module and Build

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diamailbox.md | **module-and-build** |

**Status:** `Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

**Research:** @docs/research/entity_system/summary.md

**Depends on:** Nothing — this is the first feature implemented. It creates the empty vcxproj skeleton so that all subsequent features can compile and test as they are built. The vcxproj is populated incrementally: each implementation feature adds its own files as part of its tasks.

---

## Problem Statement

Before any DiaMailbox code can compile, a vcxproj and sln registration must exist. This feature creates the project skeleton first — an empty static library with DiaCore as its only reference — so that each subsequent implementation feature can add files and run TDD immediately. The YAML module doc (`dia.mailbox.architecture.module.md`) is also created here, making the module visible to the engine's module registry and AI navigation tooling from day one.

---

## Solution Overview

Create `Dia/DiaMailbox/DiaMailbox.vcxproj` as a static library following the exact same structure as `DiaThreading.vcxproj`: Debug|x64 and Release|x64 configurations only, no `OutDir`/`IntDir`/`PlatformToolset`/`LanguageStandard` overrides (those come from `Directory.Build.props`), `AdditionalIncludeDirectories` of `./;./../;`, and a `ProjectReference` to `DiaCore.vcxproj`. Add a matching `.vcxproj.filters` file organising headers and sources under `Header Files` and `Source Files` filters. Register the project in `Cluiche.sln` under the `Dia` solution folder. Finally, create `Dia/DiaMailbox/dia.mailbox.architecture.module.md` with the full `dia.module.v1` YAML frontmatter as specified in `docs/reference/registry/module-metadata-schema.md`.

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Dia/DiaMailbox/DiaMailbox.vcxproj` exists as a `StaticLibrary` project targeting `Debug\|x64` and `Release\|x64` | Code review of vcxproj |
| AC2 | `DiaMailbox.vcxproj` builds with zero errors and zero warnings in Debug\|x64 (`dia pipeline --target googletest --config Debug`) | Build output |
| AC3 | `DiaMailbox.vcxproj` builds with zero errors and zero warnings in Release\|x64 (`dia pipeline --target googletest --config Release`) | Build output |
| AC4 | `DiaMailbox.vcxproj` contains no `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` overrides | Code review |
| AC5 | `DiaMailbox.vcxproj` has a `ProjectReference` to `DiaCore.vcxproj` and no other project references | Code review |
| AC6 | `Dia/DiaMailbox/DiaMailbox.vcxproj.filters` exists; all `.h` files appear under a `Header Files` filter; all `.cpp` files appear under a `Source Files` filter | Code review of filters file |
| AC7 | `Cluiche.sln` includes `DiaMailbox.vcxproj` under the `Dia` solution folder | Code review of .sln |
| AC8 | `Dia/DiaMailbox/dia.mailbox.architecture.module.md` exists with valid `dia.module.v1` YAML frontmatter containing at minimum: `module_id`, `display_name`, `version`, `parent_module`, `type`, `namespace`, `include_root`, `public_headers`, `dependencies`, `responsibilities`, `non_responsibilities` | Code review |
| AC9 | `dia pipeline --target googletest` is green (all tests pass) in Debug\|x64 after DiaMailbox is registered in the solution | Test run |
| AC10 | The vcxproj is initially empty (no `ClInclude`/`ClCompile` entries beyond a placeholder stub); subsequent features add their own files as part of their tasks | Code review |
| AC11 | `dia.mailbox.architecture.module.md` is listed as a `None` item in `DiaMailbox.vcxproj` so it appears in the IDE | Code review |

---

## Public API

N/A — this is a build infrastructure feature. The deliverables are:

- `Dia/DiaMailbox/DiaMailbox.vcxproj` — static library project, Debug\|x64 + Release\|x64
- `Dia/DiaMailbox/DiaMailbox.vcxproj.filters` — IDE filter organisation
- `Cluiche.sln` updated — `DiaMailbox.vcxproj` registered under the `Dia` solution folder
- `Dia/DiaMailbox/dia.mailbox.architecture.module.md` — YAML module documentation with the schema below

### YAML module doc (`dia.mailbox.architecture.module.md`)

```yaml
module_id: dia.mailbox
display_name: DiaMailbox
version: 1.0.0
parent_module: dia
type: static_library
namespace: Dia::Mailbox
include_root: Dia/DiaMailbox
public_headers:
  - Mailbox.h
  - MailboxTypes.h
  - IMailboxRouter.h
dependencies:
  required:
    - dia.core
responsibilities:
  - Typed deferred message queuing (ring buffers with compile-time capacity)
  - Opaque address routing via pluggable IMailboxRouter
  - Caller-managed subscriptions with generation-tracked handles
non_responsibilities:
  - Knowledge of entities, components, or any domain concept
  - Thread safety (single-threaded primitive)
  - Cross-frame message persistence
```

### vcxproj conventions

Pattern mirrors `Dia/DiaThreading/DiaThreading.vcxproj` exactly:

- `ConfigurationType`: `StaticLibrary`
- `AdditionalIncludeDirectories`: `./;./../;%(AdditionalIncludeDirectories)`
- No `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` nodes
- `ProjectReference` to `..\DiaCore\DiaCore.vcxproj` with `<ReferenceOutputAssembly>false</ReferenceOutputAssembly>`
- Module doc listed as a `<None Include="dia.mailbox.architecture.module.md" />` item

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaMailbox/DiaMailbox.vcxproj` — empty static lib skeleton, Debug\|x64 + Release\|x64, no build prop overrides, DiaCore reference, `<None Include="dia.mailbox.architecture.module.md" />` | AC1, AC4, AC5, AC10, AC11 | Planned | haiku | Copy structure from DiaThreading.vcxproj; update ProjectGuid, RootNamespace; no ClInclude/ClCompile yet |
| 2 | Create `Dia/DiaMailbox/DiaMailbox.vcxproj.filters` — empty Header Files / Source Files filters | AC6 | Planned | haiku | Mirror DiaThreading.vcxproj.filters pattern |
| 3 | Register `DiaMailbox.vcxproj` in `Cluiche.sln` under the `Dia` solution folder | AC7 | Planned | haiku | Edit .sln manually; match the ProjectGuid used in task 1 |
| 4 | Create `Dia/DiaMailbox/dia.mailbox.architecture.module.md` with full `dia.module.v1` YAML frontmatter | AC8, AC11 | Planned | haiku | Use schema from docs/reference/registry/module-metadata-schema.md |
| 5 | Build verification — `dia pipeline --target googletest` Debug\|x64 and Release\|x64 | AC2, AC3, AC9 | Planned | haiku | Empty lib must build clean; report pass/fail |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | No new identifiers introduced by this feature; applies to the implementation features. The module doc uses `module_id: dia.mailbox` (a registry string, not a `StringCRC` instance). |
| PD-004 | No STL containers in public APIs | No public API surface in this build feature. All public APIs are in the implementation features. |
| PD-005 | x64 only | `DiaMailbox.vcxproj` declares exactly two `ProjectConfiguration` items: `Debug\|x64` and `Release\|x64`. No Win32 configuration. |
| PD-006 | VS project files are source of truth | `DiaMailbox.vcxproj`, `DiaMailbox.vcxproj.filters`, and the `Cluiche.sln` entry are all created and maintained manually. No generation tooling. |
| PD-007 | C++20 required | Language standard is set by `Directory.Build.props`. `DiaMailbox.vcxproj` carries no `LanguageStandard` node (AC4). |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` carries no `OutDir`, `IntDir`, `PlatformToolset`, or `WindowsTargetPlatformVersion` overrides (AC4). |
| AD-001 | Module system with YAML frontmatter | `dia.mailbox.architecture.module.md` created with full `dia.module.v1` schema, public API, responsibilities, and dependency declarations (AC8). |
| AD-002 | No STL containers in public APIs | Reinforces PD-004; no API surface here. |
| AD-003 | Namespace `Dia::<Module>::` | Module namespace declared as `Dia::Mailbox::` in the YAML doc. Implementation features honour this. |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | YAML `namespace: Dia::Mailbox` is consistent with the system decision. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | vcxproj file listing | This feature creates an empty skeleton. How do subsequent features register their files? | Each implementation feature (address-and-types, typed-queue, subscriptions, routers) adds its own `ClInclude`/`ClCompile` entries to the vcxproj as part of its "update vcxproj" task. This is how every other Dia module works (DiaObservation, DiaThreading) — skeleton first, files added incrementally. Per PD-006, VS project files are manually maintained. |
| 2 | vcxproj — ProjectGuid | The `ProjectGuid` in `DiaMailbox.vcxproj` must be unique across the solution. How should it be generated? | Generate a fresh GUID using Visual Studio's "Add Existing Project" dialog or any GUID generator tool (e.g. PowerShell `[guid]::NewGuid()`). The `.sln` file entry must reference the same GUID. Duplicates cause silent build failures in solutions with many projects. |
| 3 | .sln registration | Where exactly in `Cluiche.sln` does the new project entry appear, and which solution folder GUID must be used? | The existing `Dia` solution folder in `Cluiche.sln` has its own GUID (visible in the `.sln` text). The new `Project(...)` block must be added to the `Project` list, and a matching `{ProjectGuid} = {ProjectGuid}` line must appear inside the `Dia` solution folder's nested-projects block. Inspect the existing DiaThreading or DiaObservation entry in the .sln to copy the exact pattern. |
| 4 | AdditionalIncludeDirectories | `DiaMailbox.vcxproj` uses `./;./../;` as include directories. Does `DiaMailbox` code need to include its own headers as `<DiaMailbox/Foo.h>` or `"Foo.h"`? | Internal includes within DiaMailbox can use `"Foo.h"` (relative, resolved from `./`). Consumer projects that link DiaMailbox use `<DiaMailbox/Mailbox.h>` — this is resolved because consumers add `Dia/DiaMailbox/../` (i.e. `Dia/`) to their own include paths, matching the pattern for all other Dia modules. The YAML `include_root: Dia/DiaMailbox` documents this convention. |
| 5 | Verification — pipeline target | AC9 uses `dia pipeline --target googletest` as the pipeline-green check. Should `dia pipeline --target cluichetest` also be checked? | `--target googletest` is the mandatory gate because GoogleTests will be the first consumer that links against DiaMailbox (per the DiaMailbox feature specs). `--target cluichetest` is not required for this feature but should be checked before diaentitytemplate or any game-side consumer is wired in. Note that simply registering DiaMailbox in the solution (without wiring it as a dependency to any consumer) will not cause a CluicheTest build break. |
| 6 | Module doc — public_headers | The YAML lists `Mailbox.h`, `MailboxTypes.h`, and `IMailboxRouter.h` as public headers. Are these the actual filenames produced by the implementation features? | These are the planned filenames from the system spec's public interface design. If the implementation features land with different filenames, the YAML must be updated to match. The implementer of this feature must cross-check against the actual files present in `Dia/DiaMailbox/` before finalising the YAML. |
| 7 | Module doc — dependencies | The YAML lists only `dia.core` as a required dependency. Is it possible that an implementation feature introduces a dependency on a second Dia module? | Per the system spec, DiaMailbox's dependency chain is strictly `DiaMailbox → DiaCore`. The system spec's Non-Responsibilities and Out-of-Scope sections explicitly exclude diaentitytemplate, DiaApplicationFlow, and all domain modules. If an implementation feature accidentally pulls in a second module, that is a spec violation and must be resolved before this build feature proceeds. The `DiaMailbox.vcxproj` must reflect only `DiaCore` as a project reference (AC5). |
| 8 | vcxproj — Testing files | The system spec does not mention a `Testing/` subdirectory for DiaMailbox. Do test utilities ship with this module? | The project memory rule (reference_backlog.md → feedback_testing_utilities.md) says test helpers live in `<Module>/Testing/`. If any DiaMailbox implementation feature produces a test fixture or mock under `Dia/DiaMailbox/Testing/`, those files must also appear in `DiaMailbox.vcxproj` as `ClInclude` entries. This feature must include any `Testing/` files present at implementation time; the task 2 note covers this. |
| 9 | solution folder — name casing | The `Dia` solution folder name must match exactly. What if the solution has both `Dia` and `Dia Engine` folders? | Inspect `Cluiche.sln` before registering. The correct folder is the one containing `DiaCore`, `DiaObservation`, `DiaThreading` etc. Add DiaMailbox to that same folder using the exact folder GUID from the `.sln`. Do not create a second folder. |
| 10 | Ordering of .sln edits | Is there a risk of merge conflicts on `Cluiche.sln` if another feature is also adding a project concurrently? | Yes — `.sln` is a text file and concurrent edits cause conflicts. The convention in this project is serial registration: finish all four implementation features and commit them, then execute this build feature as a single task that edits both the vcxproj set and the `.sln`. The `Depends on` field in the spec header encodes this ordering. |

---

## Status

`Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
