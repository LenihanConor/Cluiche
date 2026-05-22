# Implementation Plan: DiaReflect Type Coverage

**Status:** Not Started
**Created:** 2026-05-21

## Overview

Add `serialize()` bridge headers for all audited value types across DiaMaths, DiaGeometry2D, DiaGeometry3D, DiaRig2D, DiaIK2D, DiaAnimation2D, and DiaRigidBody2D. Bridge headers follow the pattern established by `DiaMathsSerializers.h`: header lives in `DiaCore/Reflect/`, includes both the type module headers and `ReflectMacros.h`, defines `serialize()` in the type's own namespace so ADL finds it automatically.

**Out of scope:** SpatialGrid types (any module), DiaSoftBody2D, DiaCore/Type migration (tracked in diareflect.plan.md T11–T13).

---

## Session Notes

**DiaReflect system state:** Phases 1–3a complete, 150 tests GREEN. Archive concept, JSON/Binary archives, container specializations (T[N], DynamicArrayC), polymorphic registry, and field attributes are all done. Already serialized: `Vector2D`, `Matrix22`, `Matrix34`, `Matrix44`, `Quaternion`.

**Bridge header pattern:**
```cpp
// DiaCore/Reflect/DiaFooSerializers.h
#pragma once
#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaFoo/Bar.h"

namespace Dia::Foo {

DIA_SERIALIZE(Bar, 1)
    DIA_FIELD(fieldA)
    DIA_FIELD(fieldB)
DIA_SERIALIZE_END

}  // namespace Dia::Foo
```

**Module boundary rule:** The bridge header is neutral — neither the type module nor DiaCore/Reflect depends on the other. Only consuming projects (GoogleTests etc.) include bridge headers, and they already have both modules as dependencies.

**Per-task deliverables:** (1) bridge header in `DiaCore/Reflect/`, (2) header added to `DiaCore/DiaCore.vcxproj` + `DiaCore.vcxproj.filters` under the Reflect filter, (3) GoogleTests round-trip test file covering JSON + binary write→read identity and missing-field tolerance for every type.

**Test minimum per type:** JSON round-trip (all fields survive), binary round-trip, missing-field tolerance (fields keep C++ defaults when absent from JSON). Array-containing types need an extra array nesting test.

**Known access risks (investigate before writing):**
- `Angle::mRadian` — private. Check if a `serialize` friend declaration or accessor exists; if not, add a `friend` in the header alongside the type (same pattern as Matrix22 uses `operator[]`).
- `Matrix33` — check whether element storage is public or private before starting T1.
- `Bone::mMetadata` — `DynamicArrayC<MetadataEntry>`. `MetadataEntry` must be serializable before `Bone` can compose it. If `MetadataEntry` is not a suitable DiaReflect type, skip `mMetadata` and document the omission in task notes.
- `ConvexPolygon` — has `mVertices[16]` (fixed array) + `mVertexCount`. Serialize only the live `[0..mVertexCount-1]` slice, not the full 16 — needs a `DIA_FIELD_NAMED` or custom loop rather than `DIA_FIELD(mVertices)`.
- `WorldDef::broadPhase` — `ISpatialStructure*` non-owning pointer. Skip this field entirely; document in bridge header.
- `Transform2D::mParent` / `Transform3D::mParent` — raw owning ptr. Skip; hierarchy is reconstructed at scene level.

---

## Task Table

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| **Phase 4a — DiaMaths completion** | | | | | |
| 1 | Extend `DiaMathsSerializers.h`: add `Vector3D`, `Vector4D`, `Angle`, `Matrix33` | Round-trip tests GREEN for all 4 types | Not Started | sonnet | Check Matrix33 element access and Angle::mRadian visibility before writing; Angle needed by T3 (Arc/Sector) |
| 2 | Extend `DiaMathsSerializers.h`: add `Transform2D`, `Transform3D` (skip `mParent`) | Round-trip tests GREEN; mParent not present in output | Not Started | sonnet | Depends on T1 (Angle for Transform2D, Vector3D for Transform3D) |
| **Phase 4b — DiaGeometry2D** | | | | | |
| 3 | New `DiaGeometry2DSerializers.h`: `Point`, `Circle`, `AARect`, `OORect`, `Line`, `Ray`, `Triangle`, `Capsule`, `Arc`, `Sector`, `ConvexPolygon`. Add to DiaCore.vcxproj. | Round-trip tests GREEN for all 11 types | Not Started | sonnet | Depends on T1 (Angle used by Arc, Sector). ConvexPolygon: serialize `mVertexCount` + live vertex slice only |
| **Phase 4c — DiaGeometry3D** | | | | | |
| 4 | New `DiaGeometry3DSerializers.h`: `Sphere`, `AABB`, `OOBB`, `Capsule3D`. Add to DiaCore.vcxproj. | Round-trip tests GREEN for all 4 types | Not Started | sonnet | Depends on T1 (Vector3D); Quaternion already done. Confirm actual type/field names in headers before writing |
| **Phase 4d — DiaRig2D** | | | | | |
| 5 | New `DiaRig2DSerializers.h`: `BoneTransform`, `Bone`, `SkeletonDef`. Add to DiaCore.vcxproj. | Round-trip tests GREEN; SkeletonDef array nesting test | Not Started | sonnet | Investigate MetadataEntry before writing Bone serializer; skip mMetadata if MetadataEntry is not serializable |
| **Phase 4e — DiaIK2D** | | | | | |
| 6 | New `DiaIK2DSerializers.h`: `JointLimitDef`, `PoleVector`, `IKChainDef`. Add to DiaCore.vcxproj. | Round-trip tests GREEN; IKChainDef array nesting test | Not Started | sonnet | IKChainDef composes `DynamicArrayC<JointLimitDef>` — define JointLimitDef serializer first within the same file |
| **Phase 4f — DiaAnimation2D** | | | | | |
| 7 | New `DiaAnimation2DSerializers.h`: `SpringNodeDef`, `SpringChainDef`, `BoneMask`, `Keyframe`, `KeyframeTrack`, `AnimClipDef`. Add to DiaCore.vcxproj. | Round-trip tests GREEN; AnimClipDef deep-nesting test (clip → tracks → keyframes) | Not Started | sonnet | Define in composition order: SpringNodeDef → SpringChainDef, Keyframe → KeyframeTrack → AnimClipDef |
| **Phase 4g — DiaRigidBody2D** | | | | | |
| 8 | New `DiaRigidBody2DSerializers.h`: `WorldDef` (skip `broadPhase`). Add to DiaCore.vcxproj. | Round-trip tests GREEN; broadPhase absent from output | Not Started | sonnet | WorldDef may compose sub-config structs (ResponseConfig, ConstraintSolverConfig) — serialize those inline or skip if runtime-only |
