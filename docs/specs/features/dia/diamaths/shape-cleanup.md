# Feature Spec: DiaMaths Shape Cleanup

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaMaths | @docs/specs/systems/dia/diamaths.md |
| Feature | DiaMaths Shape Cleanup | (this document) |

## Summary

Delete the duplicate shape sources that still live in `Dia/DiaMaths/Shape/2D/` and `Dia/DiaMaths/Shape/Common/` after the DiaGeometry2D migration. The migration was a copy, not a move; both DiaMaths and DiaGeometry2D ship the same shape headers today. This feature removes the DiaMaths copies, deletes the corresponding architecture docs, and updates the parent module doc and `DiaMaths.vcxproj`.

This is the cleanup feature that completes the DiaGeometry2D migration story and locks in `diamaths.md` SD-001 ("DiaMaths is pure linear algebra only — no shapes, no intersection tests, no spatial structures").

## Problem

`Dia/DiaMaths/Shape/2D/` contains 10 shape types (`AARect2D`, `Arc2D`, `Capsule2D`, `Circle2D`, `Ellipse2D`, `IntersectionPoint2D`, `Line2D`, `OORect2D`, `Ray2D`, `Triangle2D`) — each as `.h` / `.cpp` / `.inl`. `Dia/DiaMaths/Shape/Common/` contains `IntersectionClassify` and `IntersectionTests`. All were copied to `Dia/DiaGeometry2D/` during the system migration and are now duplicates. Three risks while they exist:

1. Callers can `#include <DiaMaths/Shape/2D/Circle2D.h>` and resolve to the wrong namespace (`Dia::Maths::Circle2D` vs `Dia::Geometry2D::Circle`), producing source files that compile against the stale type.
2. Drift — bug fixes applied to the DiaGeometry2D copy will not propagate to the DiaMaths copy. Eventually the two diverge silently.
3. Spec contradiction — the DiaGeometry2D system spec is `Done` and lists shapes as its responsibility; the DiaMaths system spec SD-001 says "no shapes" — yet the shapes literally still exist in `Dia/DiaMaths/`.

The fix is simple but requires a careful pre-deletion verification: nothing must still resolve to the DiaMaths-side copies.

## Acceptance Criteria

1. **Pre-deletion verification:** a search across the entire repo for `#include <DiaMaths/Shape/` and `Dia::Maths::Circle`, `Dia::Maths::AARect`, `Dia::Maths::OORect`, `Dia::Maths::Line2D`, `Dia::Maths::Ray2D`, `Dia::Maths::Triangle2D`, `Dia::Maths::Arc2D`, `Dia::Maths::Capsule2D`, `Dia::Maths::Ellipse2D`, `Dia::Maths::IntersectionPoint2D`, `Dia::Maths::IntersectionClassify`, `Dia::Maths::IntersectionTests` returns **zero hits** before any file is deleted. Any remaining call site is migrated to its `Dia::Geometry2D::` equivalent first.
2. **Delete `Dia/DiaMaths/Shape/2D/` directory contents** — all 10 shape `.h` / `.cpp` / `.inl` triples (30 files) and the architecture doc `dia.maths.shape.x2d.architecture.module.md`.
3. **Delete `Dia/DiaMaths/Shape/Common/` directory contents** — `IntersectionClassify.h/.cpp/.inl`, `IntersectionTests.h/.cpp`, and `dia.maths.shape.common.architecture.module.md`.
4. **Delete `Dia/DiaMaths/Shape/dia.maths.shape.architecture.module.md`** — the parent shape submodule doc.
5. **Delete the now-empty `Dia/DiaMaths/Shape/` directory** if no files remain after steps 2–4.
6. **Update `Dia/DiaMaths/Docs/dia.maths.architecture.module.md`** — remove `dia.maths.shape` from the `dependent_modules` list. (Per `diamaths.md` AI Review Q6, this update happens here as well as in the Quaternion feature.)
7. **Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters`** — remove all entries for the deleted files. Remove the `Shape` filter if empty.
8. **Build verifies clean:** `dia pipeline --target googletest` (Debug + Release) compiles successfully across all libraries and tests with zero warnings introduced by this change.
9. **Test verifies clean:** `dia run googletest` passes — no test depended on the DiaMaths-side shape headers.
10. **No new files are created.** This is a deletion-only feature aside from project-file modifications.

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Run pre-deletion verification grep | Search the full repo for `#include <DiaMaths/Shape/` and the listed `Dia::Maths::` shape symbols. Document the zero-hit result. If hits found, the cleanup is BLOCKED until each is migrated to its `Dia::Geometry2D::` equivalent. |
| 2 | Delete `Dia/DiaMaths/Shape/2D/` directory contents | 30 shape files + 1 architecture doc |
| 3 | Delete `Dia/DiaMaths/Shape/Common/` directory contents | 5 files + 1 architecture doc |
| 4 | Delete `Dia/DiaMaths/Shape/dia.maths.shape.architecture.module.md` | Parent submodule doc |
| 5 | Remove now-empty `Dia/DiaMaths/Shape/` directory | If no files remain |
| 6 | Update `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` | Remove `dia.maths.shape` from `dependent_modules` |
| 7 | Update `Dia/DiaMaths/DiaMaths.vcxproj` and `.vcxproj.filters` | Remove all deleted entries; remove Shape filter |
| 8 | Run `dia pipeline --target googletest` Debug + Release | Verify clean build |
| 9 | Run `dia run googletest` | Verify all tests pass |

## Verification (Tasks 8–9)

The cleanup is "done" only when:
- Both Debug and Release configs of `dia pipeline --target googletest` produce zero errors and zero warnings introduced by this change.
- All existing GoogleTests pass without modification.
- Re-running the verification grep from Task 1 still returns zero hits (sanity check that nothing was added back).

## Files

| File / Path | Action |
|-------------|--------|
| `Dia/DiaMaths/Shape/2D/AARect2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Arc2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Capsule2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Circle2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Ellipse2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/IntersectionPoint2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Line2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/OORect2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Ray2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/Triangle2D.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/2D/dia.maths.shape.x2d.architecture.module.md` | Delete |
| `Dia/DiaMaths/Shape/Common/IntersectionClassify.{h,cpp,inl}` | Delete |
| `Dia/DiaMaths/Shape/Common/IntersectionTests.{h,cpp}` | Delete |
| `Dia/DiaMaths/Shape/Common/dia.maths.shape.common.architecture.module.md` | Delete |
| `Dia/DiaMaths/Shape/dia.maths.shape.architecture.module.md` | Delete |
| `Dia/DiaMaths/Shape/` | Delete directory if empty |
| `Dia/DiaMaths/Docs/dia.maths.architecture.module.md` | Modify — remove `dia.maths.shape` from dependent_modules |
| `Dia/DiaMaths/DiaMaths.vcxproj` | Modify — remove deleted file entries |
| `Dia/DiaMaths/DiaMaths.vcxproj.filters` | Modify — remove deleted entries; remove Shape filter |

## Dependencies

| Dependency | What this feature uses |
|------------|----------------------|
| DiaGeometry2D — full shape suite | Verifies that all shape consumers can resolve to `Dia::Geometry2D::` types instead |
| Search/grep tooling | Pre-deletion verification step |

**Order:** Independent of the rest of the DiaMaths features (Vector3D::Cross, Quaternion, Matrix44, Matrix34, Transform3D). Can be implemented first, last, or in parallel — chooses on schedule, not on blocker. Recommended **last** in the DiaMaths batch so any 3D-features bug surfaced during their implementation has already validated the cleanup approach.

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | **Not applicable.** No new code. |
| PD-004 | No STL containers in public API | **Not applicable.** |
| PD-005 | x64 only | **Compliant.** Deletes only. |
| PD-006 | VS project files source of truth | **Compliant — directly relevant.** vcxproj/.filters cleaned up to match deletions. |
| PD-007 | C++20 required | **Compliant.** No code changes. |
| PD-008 | Directory.Build.props owns build settings | **Compliant.** No vcxproj overrides added. |
| AD-001 | Module YAML | **Compliant.** Architecture docs deleted (per `diageometry2d.md` AI Review Q10: delete, do not archive). Parent doc updated. |
| AD-002 | No STL public APIs | **Not applicable.** |
| AD-003 | Namespace | **Compliant.** Removes the `Dia::Maths::` shape symbols, which were the wrong namespace for these types. |
| SD-001 | DiaMaths is "pure linear algebra only" | **Compliant — directly satisfies.** This is the feature that locks SD-001 in. |
| SD-010 | Cleanup deletes Shape/2D, Shape/Common, and module docs | **Compliant — directly satisfies this decision.** |
| SD-011 | Static library, no STL in public API | **Compliant.** |
| SD-012 | New types follow existing patterns | **Not applicable.** No new types. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Pre-deletion grep | Is "zero hits" really achievable, or are there expected migrations to do first? | Verify on day-of. If migrations are needed, this feature is BLOCKED on those migrations (file as task 1.5). The DiaGeometry2D system spec is `Done` and the assumption is that all consumers were migrated as part of that system spec's tasks — the grep is the proof. |
| 2 | Verification grep symbols | The list of symbols to grep covers 10 shape types plus 2 intersection types. Is anything missed? | Cross-check against `Dia/DiaMaths/Shape/2D/` actual file list before running. The list above was assembled from the file glob; confirm no new types were added since. |
| 3 | Order in DiaMaths batch | Should this run first (before any 3D feature) or last? | Last, recommended. Reason: if a 3D feature accidentally depends on a `Dia::Maths::` shape (say, a test that wraps a `Circle2D`), running the cleanup first would surface that the wrong way. Running it last after the 3D features ship clean lets the cleanup itself be the only variable. |
| 4 | Architecture doc deletion | The DiaGeometry2D spec AI Review Q10 said "delete them; the code is the source of truth". Does that still hold? | Yes. Stale architecture docs are misleading. The DiaGeometry2D module has its own architecture doc covering the shapes; the DiaMaths shape docs are doubly stale. Delete. |
| 5 | DiaMaths parent module doc | After deletion, dependent_modules drops `dia.maths.shape` and adds `dia.maths.quaternion` (per Quaternion feature). Net change? | Yes. Both updates land. Order doesn't matter as long as both happen before the system spec is marked Done. |
| 6 | Empty Shape/ directory | Should it really be deleted as an empty folder, or kept as a marker? | Deleted. Empty folders are confusing in a repo — they look unfinished. The DiaGeometry2D module is the sole owner of "shapes" now. |
| 7 | Existing tests | Do GoogleTests reference any `Dia::Maths::` shape directly? | Verify with the same grep. The DiaGeometry2D migration moved tests too; the GoogleTests `Geometry2D/` folder uses `Dia::Geometry2D::` types. If any old test slipped through, migrate it first. |
| 8 | Rollback plan | What if `dia pipeline` fails after deletion? | Git revert. The deletions form one commit; reverting restores everything. The verification grep + Debug/Release pipeline both passing is the safety net before commit. |
| 9 | DiaGraphics 2D Transform | While we're here, should the trailing implementation note in `peppy-weaving-pond.md` (DiaGraphics/Misc/Transform.h duplicates DiaMaths::Transform2D) be folded into this cleanup? | No. Different module, different system spec, different consumers to audit. Keep separate. |
| 10 | Approval gate | Anything blocking spec Approval? | Nothing intrinsic. The cleanup is straightforward; the question is when to schedule the implementation, not whether the spec is sound. |

## Status

`Approved` — Steps 1–4 complete (interview, draft, binding decisions, AI review). Implementation can proceed at any time but recommended last in the DiaMaths batch.
