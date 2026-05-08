# Research: Explore — DiaGraphics FrameData Debug Scalability

**Session date:** 2026-05-02
**Folder:** docs/research/diagraph_framedat/

## Problem Space Overview

`DebugFrameData` is the debug-geometry layer of `FrameData`, the per-frame data packet that flows from the simulation thread to the render thread. Each frame it collects draw requests for debug primitives, then a visitor (`DebugFrameRendererVisitor`) walks the stored items and submits them to SFML. Currently the system stores circles and lines in two separate `DynamicArrayC<T, 16>` fields — one per concrete type. That is fine for two shapes with a known upper bound of 16 each, but the design scales neither in *kind* (adding a new shape is invasive) nor in *count* (16 is arbitrary and shared across all callers).

The core tension is between two legitimate goals: **zero heap allocation at frame time** (critical on the hot path; frames are copied across a stream every tick) and **open extensibility** (the engine should be able to add a rectangle, arc, or point primitive without touching `DebugFrameData`, `DebugFrameDataVisitor`, and every visitor implementation). The existing design maximises the first goal at the total expense of the second.

The problem sits at the intersection of two classic game-engine design challenges: the **type-erasure vs. static dispatch** tradeoff for polymorphic value types, and the **fixed vs. dynamic buffer** tradeoff for per-frame scratch data. Both must be solved without introducing STL containers into the public API (PD-004) and without heap allocation per frame.

## Existing Approaches

- **Per-type fixed arrays (current)** — one `DynamicArrayC<ShapeT, N>` per concrete type; fully static, zero allocation, but requires code changes per new shape.
- **Pointer array + polymorphism** — store `DebugFrameDataBase*` pointers; fully open but requires heap allocation or a separate object pool and complicates copy semantics across the frame stream.
- **Type-erased value store (small-buffer / union)** — store items in a flat byte buffer as a tagged union or `std::variant`-style structure; open to new types added at compile time, single buffer, copy-safe, no heap.
- **Command buffer / bytecode** — encode draw commands as a packed byte stream with a type tag + payload; extremely compact, zero per-item overhead, but complex to iterate and requires careful alignment handling.
- **Segregated pool per type, registered at compile time** — each shape type registers itself into a compile-time table; the container holds a fixed slot per registered type. Extensible but requires a registration mechanism.
- **Single heterogeneous ring buffer** — all shapes share one flat buffer, items are laid out as `[tag][data]`; simple, cache-friendly, bounded total memory, but iteration requires dispatch on tag and alignment padding is non-trivial.
- **Intrusive linked free-list in a slab** — a slab allocator per frame; items are allocated from the slab, linked in insertion order; zero heap, O(1) add, O(n) iterate. Copy semantics require copying the whole slab.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Memory model | Static (compile-time size) / Dynamic (heap) / Slab (fixed block, runtime layout) | Dynamic violates hot-path and copy goals; slab is a middle ground |
| Type extensibility | Closed (per-type field) / Open (type-erased store) | Current is closed; open requires a tag/dispatch mechanism |
| Copy cost | Cheap value copy / Deep copy of slab | FrameData is copied to the render stream every tick — copy must be O(n) or cheaper |
| Visitor compatibility | Keep existing visitor pattern / Replace with typed callbacks / Remove visitor entirely | Visitor is the consumer contract; changes cascade to DiaSFML |
| Insertion ordering | Per-type batches (current) / Global insertion order | Global order matters if layering (e.g. draw lines over circles); current batching loses order |
| Capacity policy | Per-type fixed caps / Single shared cap / Dynamic overflow | Per-type caps waste memory when one type dominates; shared cap is more efficient |
| Shape taxonomy | 2D only / 2D + 3D / Typed vs. generic colour+geometry | Cluiche is 2D today; 3D is speculative; keep 2D focus |

## Known Tradeoffs

- **Static dispatch is fast but closed.** `DynamicArrayC<T, N>` gives perfect cache locality and trivial copy semantics, but every new shape type requires a new field and new `RequestDraw()` overload.
- **Type erasure is open but costly.** Pointer-based polymorphism introduces indirection, complicates frame copies, and risks heap allocation unless paired with a pool or arena.
- **Union/variant is the sweet spot but adds dispatch overhead.** A tagged union of all known shapes eliminates per-type fields while staying stack-friendly; the cost is a `switch` on tag at render time rather than a virtual call — typically faster.
- **Shared capacity trades waste for simplicity.** A single `DynamicArrayC<DebugPrimitive, N>` where `DebugPrimitive` is a tagged union uses a total cap of N regardless of shape mix — more flexible than two caps of 16 each.
- **Global insertion order requires a single array.** If draw order ever matters (overlay primitives), per-type batches are wrong by design.
- **Copy semantics must remain trivial.** `FrameData` is copied into the stream via `InsertCopyOfDataToStream`. Any design that stores pointers-into-slab breaks this unless the copy explicitly rebuilds the slab.

## Known Pitfalls (C++ / game engine context)

- **Alignment in byte-buffer schemes.** Packing different structs into a raw byte array requires manual alignment; easy to get wrong silently on x64.
- **`std::variant` in public API.** `std::variant` is STL — violates PD-004. A hand-rolled tagged union or a thin wrapper is needed.
- **Virtual destructor cost.** `DebugFrameDataBase` has a `virtual ~DebugFrameDataBase()`. Storing base-class values in a flat array loses the vtable; storing by pointer re-introduces heap concerns.
- **Over-engineering for debug data.** Debug primitives are discarded every frame and never saved to disk. High-complexity solutions (type registries, bytecode) may be overkill; a tagged union of ~5 shapes is entirely sufficient.
- **Visitor interface changes cascade.** Every concrete `DebugFrameDataVisitor` (currently `DebugFrameRendererVisitor` in DiaSFML and test recording visitors) must add a `Visit()` overload for every new shape. A single-dispatch `Visit(DebugPrimitive)` would isolate the blast radius.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaGraphics | Owns `DebugFrameData`, `DebugFrameDataVisitor`, all shape types — primary change site |
| DiaCore | Provides `DynamicArrayC<T,N>` — the candidate backing store; also has union/bitflag patterns to look at for tagged-union idioms |
| DiaSFML | Owns `DebugFrameRendererVisitor` — consumer; must be updated if visitor interface changes |
| DiaRig2DVisualDebugger | Producer; calls `RequestDraw()` for bones — benefits from a point or segment primitive |
| CluicheTest / SimProcessingUnit | Producer; currently the only non-engine caller |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-004 No STL in public APIs | `std::variant`, `std::vector`, `std::any` all off the table for `DebugFrameData`'s public surface; hand-rolled tagged union or DiaCore container required |
| PD-007 C++20 | `std::variant` and `std::span` are available internally if not exposed; `constexpr` tagged unions are cleaner in C++20 |
| PD-001 StringCRC | Not directly relevant to primitive storage, but shape type tags could use CRC IDs if a runtime registry approach is chosen |
| PD-006 VS project files | Any new shape headers must be added to `DiaGraphics.vcxproj` and `.filters` |

## Open Questions for Ideation

- Should the capacity be per-type or shared across all primitives? A shared cap of e.g. 64 is likely more flexible than two caps of 16.
- Is global insertion order ever required, or is per-type batching acceptable for debug rendering?
- How many new primitive types are realistically needed in the near term? (Rectangle, point, arc, polygon, text label?)
- Should the visitor interface stay as-is (one `Visit()` per type) or collapse to a single `Visit(DebugPrimitive)` with internal dispatch?
- Is `DebugFrameDataBase` / virtual dispatch still needed, or can all shapes be value types in a union?
- Should `DebugFrameData` remain a separate base class of `FrameData`, or should debug primitives fold into a more general `DrawCommand` system shared with entity sprites?
