# Feature Spec: module-and-build

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Create the `diaentitytemplate` Visual Studio project, register it in `Cluiche.sln`, write the YAML module documentation file, and add the dependency edge in `dia_modules.py`. This is the scaffolding that makes all other diaentitytemplate features buildable as a static lib.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentitytemplate.md](../../systems/dia/diaentitytemplate.md) |

## Goals

- `diaentitytemplate` builds as a static lib in Debug\|x64 and Release\|x64
- All other diaentitytemplate features can reference the project without additional setup
- Module is documented and discoverable via the module registry

## Acceptance Criteria

- `Dia/diaentitytemplate/diaentitytemplate.vcxproj` exists and builds an empty static lib
- `Dia/diaentitytemplate/diaentitytemplate.vcxproj.filters` exists and matches project structure
- Project registered in `Cluiche.sln` under the `Dia` solution folder
- `diaentitytemplate.vcxproj` does not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` (PD-008)
- Project references `DiaCore`, `DiaMaths`, and `DiaMailbox` as project dependencies
- `Dia/diaentitytemplate/dia.entity.architecture.module.md` exists with valid YAML frontmatter (module ID, dependencies, public API, responsibilities)
- `dia_modules.py` (or equivalent registry) has a `diaentitytemplate` entry with correct dependency edges to `diacore`, `diamaths`, `diamailbox`
- `dia run googletest` builds and links diaentitytemplate without errors

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj` | New |
| `Dia/diaentitytemplate/diaentitytemplate.vcxproj.filters` | New |
| `Cluiche/Cluiche.sln` | Add diaentitytemplate under Dia solution folder |
| `Dia/diaentitytemplate/dia.entity.architecture.module.md` | New — YAML module doc |
| `dia_modules.py` | Add `diaentitytemplate` entry |

## YAML Module Doc Shape

```yaml
---
module: dia.entity
version: 1
namespace: Dia::Entity
project: Dia/diaentitytemplate/diaentitytemplate.vcxproj
dependent_modules:
  - dia.core
  - dia.maths
  - dia.mailbox
public_headers:
  - Dia/diaentitytemplate/Domain.h
  - Dia/diaentitytemplate/Entity.h
  - Dia/diaentitytemplate/IComponent.h
  - Dia/diaentitytemplate/ComponentTypeDesc.h
  - Dia/diaentitytemplate/ComponentRegistry.h
  - Dia/diaentitytemplate/ComponentMacros.h
  - Dia/diaentitytemplate/EntityRef.h
  - Dia/diaentitytemplate/EntityAddress.h
  - Dia/diaentitytemplate/EntityRouter.h
  - Dia/diaentitytemplate/IBlueprintLoader.h
  - Dia/diaentitytemplate/JsonBlueprintLoader.h
  - Dia/diaentitytemplate/IEntityInspectable.h
  - Dia/diaentitytemplate/QueryView.h
  - Dia/diaentitytemplate/Hierarchy/ParentComponent.h
  - Dia/diaentitytemplate/Hierarchy/ChildBufferComponent.h
  - Dia/diaentitytemplate/Hierarchy/Hierarchy.h
  - Dia/diaentitytemplate/Messages/EntityDestroyedMessage.h
responsibilities:
  - Domain (entity container), Entity (generational handle), IComponent abstract base
  - Per-type component pools via HandlePool<T>
  - End-of-frame structural mutation pipeline
  - DIA_COMPONENT + FIELD + DIA_UPDATABLE macro reflection system
  - ComponentRegistry process-global type lookup
  - JsonBlueprintLoader three-pass entity graph instantiation
  - EntityRef<T> typed cross-entity reference slots
  - Parent/child hierarchy via opt-in components
  - EntityRouter IMailboxRouter implementation for Dia::Mailbox
  - Signature-keyed query cache system
  - IEntityInspectable reflection-driven editor inspection
  - Domain::Update(dt) per-frame component tick
non_responsibilities:
  - Application lifecycle and stage management (lives in application code)
  - System-side data (physics bodies, render objects, skeletons)
  - Cross-realm references or shared state
  - Network replication
  - General engine-wide reflection (that is DiaReflect)
---
```

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-005 | x64 only | `diaentitytemplate.vcxproj` targets x64 exclusively. Compliant. |
| PD-006 | VS project files are source of truth | `.vcxproj` + `.vcxproj.filters` created and maintained manually. Compliant. |
| PD-008 | Directory.Build.props owns toolchain | Project does not override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. Compliant. |
| AD-001 | Module YAML frontmatter | `dia.entity.architecture.module.md` created with full YAML schema. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Project dependency order | `diaentitytemplate` references `DiaCore`, `DiaMaths`, `DiaMailbox`. Does `DiaMailbox` already reference `DiaCore`? | Yes — `DiaMailbox` depends on `DiaCore`. No circular dependency. Build order: DiaCore → DiaMaths → DiaMailbox → diaentitytemplate. |
| 2 | Include paths | What include directories does `diaentitytemplate.vcxproj` need? | `$(SolutionDir)Dia` — same root as all other Dia modules, so `#include <diaentitytemplate/Domain.h>` resolves correctly. No additional paths needed. |
| 3 | Solution folder | `Dia` solution folder already exists (DiaCore, DiaMaths, etc. live there). Just add diaentitytemplate to it. | Correct — no new solution folder needed. |

## Open Questions

None.

## Status

`Approved`
