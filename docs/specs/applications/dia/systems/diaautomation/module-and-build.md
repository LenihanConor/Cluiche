# Feature Spec: DiaAutomation Module and Build

## Parent System
@docs/specs/applications/dia/systems/diaautomation/diaautomation.md

## Purpose

Scaffold the DiaAutomation library: vcxproj, namespace, module doc, project references to DiaCore, DiaApplicationFlow, and DiaAPI. Creates the `AutomationService` header and stub `.cpp`. All other DiaAutomation features depend on this one.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `Dia/DiaAutomation/` directory created with `DiaAutomation.vcxproj` referencing DiaCore, DiaApplicationFlow, DiaAPI, DiaObservation. | Build passes |
| AC2 | `AutomationService.h` declared in `Dia::Automation::` namespace; empty-body constructor takes `Application&`; all methods declared per system spec. | Compiles |
| AC3 | `AutomationService.cpp` provides stub implementations (no-op bodies) for all methods. | Compiles |
| AC4 | `dia.automation.architecture.module.md` created with valid YAML frontmatter (module_id, namespace, dependencies). | Doc present |
| AC5 | `DiaAutomation.vcxproj` added to `Cluiche/Cluiche.sln`. | Solution builds |
| AC6 | `GoogleTests.vcxproj` gains a project reference to `DiaAutomation`. | Test build passes |

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-005, PD-006, PD-007, PD-008 |
| Application | @docs/specs/applications/dia/dia.md | AD-001, AD-002, AD-003 |
| System | @docs/specs/applications/dia/systems/diaautomation/diaautomation.md | SD-AUT-001 |

## Binding Decisions Compliance

| Binding Decision | Source | How This Feature Honors It |
|-----------------|--------|---------------------------|
| PD-006 (VS project files are source of truth) | Platform | New `DiaAutomation.vcxproj` manually maintained, added to `.sln` |
| AD-001 (Module docs with YAML frontmatter) | Dia App | `dia.automation.architecture.module.md` created with full YAML |
| AD-003 (Namespace `Dia::<Module>::`) | Dia App | All code in `Dia::Automation::` namespace |
| SD-AUT-001 (Service, not Module) | DiaAutomation | `AutomationService` is a plain class; no Module base class |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Should DiaAutomation vcxproj link DiaApplicationFlow as a reference or just include its headers? | Project reference — DiaAutomation uses Application& directly, needs the compiled DiaApplicationFlow symbols. |
| 2 | Does GoogleTests need to reference DiaAutomation directly, or will it inherit transitively? | Direct reference — test project needs to include DiaAutomation headers explicitly. |

## Status

`Approved` (2026-05-21)
