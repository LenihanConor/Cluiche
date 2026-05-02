# Research: Choice — DiaGraphics FrameData Debug Scalability

**Date:** 2026-05-02
**Chosen candidate:** Candidate 1 — Tagged Union Primitive (`DebugPrimitive`)

## Rationale

The tagged union approach solves both root problems — shape extensibility and the per-type cap of 16 — with a single architectural change. One `DynamicArrayC<DebugPrimitive, N>` replaces the per-type fields; adding a new shape is an enum value + union member + one switch case, with no changes to `DebugFrameData` itself. Copy semantics remain trivial (all value types), which is critical since `FrameData` is copied into the render stream every tick. The visitor collapses to a single `Visit(const DebugPrimitive&)` dispatch, removing the dual-path ambiguity that the preserved-visitor variant (#7) would have left behind.

Candidate 5 (Separate Debug Draw Queue) was ruled out for now, not discarded. It becomes the right next step once CluicheEditor or persistent debug overlay needs arise.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| #7 Tagged Union + Preserved Visitor | Same core design as #1 but leaves legacy per-type Visit() overloads as dead weight; the extra hour to do #1 cleanly is worth it |
| #5 Separate Debug Draw Queue | Right idea, wrong time — needed when editor/persistent debug tooling becomes real; planned as the follow-on |
| #3 Pointer Array + Slab | Complex copy semantics (pointer fixup into destination slab) for no gain over a value union |
| #4 Callback Dispatcher | Loses compile-time enforcement that all shape types are handled |
| #2 Increase Caps + Add Shapes | Defers the problem without solving it |
| #6 Compile-Time Type Registry | Over-engineered; adds template complexity for zero runtime benefit over a switch |

## Pre-Spec Commitments

- `DebugPrimitive` must be a plain value type (no virtuals, no pointers) — copy must be trivially correct
- Single shared capacity constant (e.g. `kDebugPrimitiveCapacity`) replacing the two separate 16-caps
- `DebugFrameDataVisitor` collapses to a single `Visit(const DebugPrimitive&)` — no per-type overloads
- No STL in public API (PD-004): tagged union hand-rolled, not `std::variant`
- Existing concrete shape data (Circle2D, Line2D) preserved as union members — no data loss
- `DebugFrameRendererVisitor` in DiaSFML updated to switch on tag — one rewrite, clean interface
- Insertion order preserved (single buffer, items stored in push order) — fixes the existing TODO in `DebugFrameData.cpp`

## Next Step

Run `/spec-feature` with this candidate as input.
Suggested parent system: DiaGraphics — DebugFrameData subsystem.
Future follow-on: Candidate 5 (Separate Debug Draw Queue) when editor/persistent debug tooling is needed.
