# Research Summary — DiaGraphics FrameData Debug Scalability

**Session folder:** docs/research/diagraph_framedat/
**Date:** 2026-05-02

## One-Line Answer

Replace the two per-type `DynamicArrayC<T, 16>` buffers in `DebugFrameData` with a single `DynamicArrayC<DebugPrimitive, N>` where `DebugPrimitive` is a hand-rolled tagged union, giving open shape extensibility and a shared capacity with trivial copy semantics.

## Journey

1. **Explored:** The system has two hard-coded per-type buffers (circle, line) capped at 16 each; adding any new shape requires touching four files and every visitor implementation. The core tension is between zero-allocation frame copies and open extensibility.
2. **Ideated:** 7 candidates generated, ranging from a minimal cap increase (S) to a fully decoupled debug draw queue (M) to a compile-time type registry (L).
3. **Evaluated:** Tagged union variants (#1 and #7) scored highest — trivial copy, no heap, single buffer, PD-004 compliant. #1 preferred over #7 because it avoids leaving legacy per-type Visit() overloads behind.
4. **Chose:** Candidate 1, with Candidate 5 (Separate Debug Draw Queue) explicitly parked as the follow-on when editor/persistent debug tooling is needed.

## Chosen Work Item

**Name:** Tagged Union Primitive (`DebugPrimitive`)
**Home module:** DiaGraphics — `Frame/` subsystem
**Suggested spec type:** Feature
**Estimated size:** S (~1 day)

## Key Insights from Exploration

- The per-type buffer design is closed by construction — every new shape requires changes in four places; the tagged union makes `DebugFrameData` itself closed to modification.
- `FrameData` is copied into the render stream every tick, so copy semantics are non-negotiable — pointer-based designs (slab, polymorphic arrays) are a trap here.
- The existing `TODO` in `DebugFrameData::AcceptVisitor()` ("should be in insertion order") is fixed for free by a single buffer.
- `std::variant` would be the natural C++20 tool here but is STL and violates PD-004 — a hand-rolled tagged union is required.
- Candidate 5 (decoupled debug queue) is the right long-term architecture for editor/tooling debug but introduces a new cross-thread transport; not warranted until CluicheEditor becomes real.
- The visitor collapse (one `Visit(DebugPrimitive)` vs. per-type overloads) is a one-hour cost that pays off permanently — no dual-dispatch ambiguity for future contributors.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| #7 Tagged Union + Preserved Visitor | Same as #1 but leaves legacy Visit() overloads as dead weight |
| #5 Separate Debug Draw Queue | Correct long-term direction; deferred until editor/persistent debug needs arise |
| #3 Pointer Array + Slab | Complex copy-fixup semantics; no benefit over value union |
| #4 Callback Dispatcher | Loses compile-time shape-coverage enforcement |
| #2 Increase Caps + Add Shapes | Defers problem without solving it |
| #6 Compile-Time Type Registry | Over-engineered; template complexity for zero runtime gain |

## References

- docs/research/diagraph_framedat/explore.md
- docs/research/diagraph_framedat/ideate.md
- docs/research/diagraph_framedat/choose.md
