# Feature Spec: IK Chain Definition

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | — |
| Application | @docs/specs/applications/dia.md | — |
| System | @docs/specs/systems/dia/diaik2d.md | **ik-chain-definition** |

**Status:** `Approved`

---

## Problem Statement

IK solvers need to know which bones form a chain, how stiff each joint is, what the reach influence should be, and which joints have rotation limits. Without a well-typed definition object, each solve call would require callers to pass raw bone indices and parameters, leading to error-prone repetition and no registration-time validation.

---

## Solution Overview

Three data types constitute the IK chain definition layer:

- **`JointLimitDef`** — per-joint rotation clamp (min/max radians, enable flag)
- **`IKChainDef`** — complete chain specification: chain ID, start/end bone IDs, solver parameters, joint limits array, reach weight
- **`PoleVector`** — per-call two-bone bend direction hint (world-space direction + weight)

These types are pure data. Bone ID→index resolution happens inside `IKSolver::RegisterChain()`, not inside these structs.

---

## Acceptance Criteria

1. `JointLimitDef` has `minAngle`, `maxAngle` (both radians, defaults `-π`/`+π`), and `enabled` (default `false`).
2. `IKChainDef` has: `id` (StringCRC), `startBoneId` (StringCRC), `endBoneId` (StringCRC), `reachWeight` (float, default 1.0, clamped [0,1] at use), `maxIterations` (int, default 20), `tolerance` (float, default 0.001), `jointLimits` (DynamicArrayC<JointLimitDef>, empty = no limits).
3. `PoleVector` has: `direction` (Vector2D, world-space unit vector), `weight` (float, default 1.0, clamped [0,1] at use).
4. All three types live in `namespace Dia::IK2D`.
5. No STL used in any public field or method.
6. Types are defined in `DiaIK2D/IKChainDef.h` — header-only, no `.cpp` needed.
7. `IKChainDef::jointLimits` is an empty `DynamicArrayC` by default; a partial array (fewer entries than bones in chain) applies limits to the first N joints only.

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | `IKChainDef::id`, `startBoneId`, `endBoneId` all use `StringCRC`. ✅ |
| PD-004 | Platform | No STL in public APIs | `jointLimits` uses `DynamicArrayC<JointLimitDef>`. ✅ |
| PD-007 | Platform | C++20 required | Header compiled under `/std:c++20`. ✅ |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. ✅ |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All types in `Dia::IK2D::`. ✅ |
| SD-009 | DiaIK2D | Bone IDs resolved at `RegisterChain` time | `IKChainDef` stores `StringCRC` IDs; resolution to indices is `IKSolver`'s responsibility. ✅ |
| SD-010 | DiaIK2D | No STL in public APIs | Reinforces PD-004. ✅ |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | JointLimitDef defaults | Should `enabled` default to `false` or should limits be enabled whenever min/max differ from ±π? | Default `false`. Callers must opt in explicitly — implicit enabling based on value would make "set to ±π = no limit" confusing and fragile. |
| 2 | IKChainDef validation | Should `IKChainDef` itself validate that `startBoneId != endBoneId`? | No — validation happens at `RegisterChain` time in `IKSolver` where the skeleton is available and bone indices can be resolved. `IKChainDef` is a plain data struct. |
| 3 | reachWeight range | Should `reachWeight` be clamped in `IKChainDef` or at solve time? | At solve time in the solver. `IKChainDef` stores the authored value verbatim; clamping on read keeps the struct simple and makes the solver responsible for safe use. |
| 4 | jointLimits partial array | If `jointLimits` has fewer entries than the chain's bone count, what happens to the remaining joints? | Uncovered joints have no limits (equivalent to `JointLimitDef{.enabled=false}`). Document this clearly in the API — partial arrays are a valid pattern for "only limit the first joint". |
| 5 | PoleVector normalisation | Should `PoleVector::direction` be normalised on construction or left to the caller? | Left to the caller. `PoleVector` is a plain data struct. Solvers that use it should normalise before use (or DIA_ASSERT in debug if magnitude is near zero). |

---

## Status

`Approved`
