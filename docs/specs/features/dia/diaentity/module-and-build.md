# Feature Spec: module-and-build

**System:** DiaEntity
**App:** Dia
**Status:** Draft

## Summary

Create the `DiaEntity` Visual Studio project, register it in `Cluiche.sln`, write the YAML module documentation file, and add the dependency edge in `dia_modules.py`. This is the scaffolding that makes all other DiaEntity features buildable as a static lib.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/PLATFORM.md) |
| Application | [dia.md](../../../applications/dia.md) |
| System | [diaentity.md](../../systems/dia/diaentity.md) |

## Goals

- `DiaEntity` builds as a static lib in Debug\|x64 and Release\|x64
- All other DiaEntity features can reference the project without additional setup
- Module is documented and discoverable via the module registry

## Acceptance Criteria

- `Dia/DiaEntity/DiaEntity.vcxproj` exists and builds an empty static lib
- `Dia/DiaEntity/DiaEntity.vcxproj.filters` exists and matches project structure
- Project registered in `Cluiche.sln` under the `Dia` solution folder
- `DiaEntity.vcxproj` does not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard` (PD-008)
- Project references `DiaCore`, `DiaMaths`, and `DiaMailbox` as project dependencies
- `Dia/DiaEntity/dia.entity.architecture.module.md` exists with valid YAML frontmatter (module ID, dependencies, public API, responsibilities)
- `dia_modules.py` (or equivalent registry) has a `diaentity` entry with correct dependency edges to `diacore`, `diamaths`, `diamailbox`
- `dia run googletest` builds and links DiaEntity without errors

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntity/DiaEntity.vcxproj` | New |
| `Dia/DiaEntity/DiaEntity.vcxproj.filters` | New |
| `Cluiche/Cluiche.sln` | Add DiaEntity under Dia solution folder |
| `Dia/DiaEntity/dia.entity.architecture.module.md` | New — YAML module doc |
| `dia_modules.py` | Add `diaentity` entry |

## YAML Module Doc Shape

```yaml
---
module: dia.entity
version: 1
namespace: Dia::Entity
project: Dia/DiaEntity/DiaEntity.vcxproj
dependent_modules:
  - dia.core
  - dia.maths
  - dia.mailbox
public_headers:
  - Dia/DiaEntity/Domain.h
  - Dia/DiaEntity/Entity.h
  - Dia/DiaEntity/IComponent.h
  - Dia/DiaEntity/ComponentTypeDesc.h
  - Dia/DiaEntity/ComponentRegistry.h
  - Dia/DiaEntity/ComponentMacros.h
  - Dia/DiaEntity/EntityRef.h
  - Dia/DiaEntity/EntityAddress.h
  - Dia/DiaEntity/EntityRouter.h
  - Dia/DiaEntity/IBlueprintLoader.h
  - Dia/DiaEntity/JsonBlueprintLoader.h
  - Dia/DiaEntity/IEntityInspectable.h
  - Dia/DiaEntity/QueryView.h
  - Dia/DiaEntity/Hierarchy/ParentComponent.h
  - Dia/DiaEntity/Hierarchy/ChildBufferComponent.h
  - Dia/DiaEntity/Hierarchy/Hierarchy.h
  - Dia/DiaEntity/Messages/EntityDestroyedMessage.h
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
| PD-005 | x64 only | `DiaEntity.vcxproj` targets x64 exclusively. Compliant. |
| PD-006 | VS project files are source of truth | `.vcxproj` + `.vcxproj.filters` created and maintained manually. Compliant. |
| PD-008 | Directory.Build.props owns toolchain | Project does not override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard. Compliant. |
| AD-001 | Module YAML frontmatter | `dia.entity.architecture.module.md` created with full YAML schema. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Project dependency order | `DiaEntity` references `DiaCore`, `DiaMaths`, `DiaMailbox`. Does `DiaMailbox` already reference `DiaCore`? | Yes — `DiaMailbox` depends on `DiaCore`. No circular dependency. Build order: DiaCore → DiaMaths → DiaMailbox → DiaEntity. |
| 2 | Include paths | What include directories does `DiaEntity.vcxproj` need? | `$(SolutionDir)Dia` — same root as all other Dia modules, so `#include <DiaEntity/Domain.h>` resolves correctly. No additional paths needed. |
| 3 | Solution folder | `Dia` solution folder already exists (DiaCore, DiaMaths, etc. live there). Just add DiaEntity to it. | Correct — no new solution folder needed. |

## Open Questions

None.

## Status

`Approved`
