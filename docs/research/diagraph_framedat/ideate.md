# Research: Ideate — DiaGraphics FrameData Debug Scalability

**Input:** docs/research/diagraph_framedat/explore.md

## Candidates

### Candidate 1: Tagged Union Primitive (`DebugPrimitive`)
**Home module/system:** DiaGraphics — replaces `DebugFrameDataCircle2D` / `DebugFrameDataLine2D` with a single `DebugPrimitive` value type
**Size:** S
**Description:**
Define a `DebugPrimitive` struct containing a `Type` enum tag and a `union` of per-shape payloads (circle, line, rect, point, etc.). `DebugFrameData` holds one `DynamicArrayC<DebugPrimitive, N>` — a single shared buffer. `RequestDraw()` becomes a set of static factory helpers or overloads that construct the right tag+payload. The visitor collapses to a single `Visit(const DebugPrimitive&)` or keeps per-type overloads dispatched internally via a `switch`. Adding a new shape is: add an enum value, add a union member, add one case in the switch — no changes to `DebugFrameData` itself.

The capacity N becomes a single tunable constant (e.g. `kDebugPrimitiveCapacity = 64`) shared across all primitive types, so a frame heavy in lines doesn't starve circles.

**Primary value:** Eliminates the per-type field explosion and the 16-per-type cap in one change; adding new shapes never touches `DebugFrameData` again.

---

### Candidate 2: Increase Caps + Add Missing Shapes (Minimal Fix)
**Home module/system:** DiaGraphics — surgical change to existing fields
**Size:** S
**Description:**
Keep the existing per-type `DynamicArrayC` architecture but raise the caps from 16 to a larger constant (e.g. 64 or 128) and add the missing concrete types (rectangle, point, arc) each as a new field + `RequestDraw()` overload + `Visit()` virtual. No architectural change — purely additive.

This unblocks the immediate problem cheaply but doubles down on the root design flaw: every future shape still requires touching four files (`DebugFrameData.h/.cpp`, `DebugFrameDataVisitor.h`, `DebugFrameRendererVisitor`). The total struct size grows with each new type regardless of whether that type is used in a given frame.

**Primary value:** Zero risk, zero learning curve — done in an afternoon, but the scalability problem is deferred, not solved.

---

### Candidate 3: Pointer Array + Frame-Scoped Slab Allocator
**Home module/system:** DiaGraphics — new `DebugFrameSlab` allocator alongside `DebugFrameData`
**Size:** M
**Description:**
`DebugFrameData` stores a `DynamicArrayC<DebugFrameDataBase*, N>` of pointers and a fixed-size byte slab (e.g. 4 KB). Each `RequestDraw()` placement-news the shape into the slab, appends its pointer to the array. The visitor iterates pointers and calls `AcceptVisitor()` polymorphically as today. `ClearDebugBuffer()` resets the slab head pointer and empties the array.

Copy semantics are the hard part: `CopyDebugBuffer()` must walk the pointer array, re-placement-new each object into the destination slab, and fix up pointers. This is doable but fragile — the copy is no longer a trivial memcpy-like operation. Also keeps virtual dispatch and `DebugFrameDataBase` hierarchy intact.

**Primary value:** Fully open to new shapes without changing the container; maintains visitor polymorphism; avoids heap. Cost is copy complexity and slab size tuning.

---

### Candidate 4: Single Typed Callback Dispatcher (No Visitor Pattern)
**Home module/system:** DiaGraphics — replace `DebugFrameDataVisitor` with a `DebugPrimitiveRenderer` function-pointer or `std::function` table
**Size:** M
**Description:**
Drop the visitor pattern entirely. `DebugFrameData` stores a tagged-union buffer (same as Candidate 1) and exposes an `Iterate(callback)` template method. The caller (DiaSFML renderer) provides a lambda or functor that switches on the tag. This removes the abstract `DebugFrameDataVisitor` class and all its required virtual overrides — consumers no longer need to implement a pure-virtual interface just to render debug geometry.

The downside: loses the formal visitor contract, making it harder to enforce that all shape types are handled. Also `std::function` is STL — a hand-rolled function pointer table or template-based approach is needed to stay PD-004 compliant.

**Primary value:** Simplifies the consumer side; no more abstract class inheritance just to render circles and lines.

---

### Candidate 5: Separate Debug Draw Queue (Decoupled from FrameData)
**Home module/system:** New thin `DebugDraw` system within DiaGraphics, consumed separately from `FrameData`
**Size:** M
**Description:**
Pull debug drawing entirely out of `FrameData`. Instead of `FrameData` inheriting from `DebugFrameData`, a standalone `DebugDrawQueue` lives alongside the frame stream. Producers push to a thread-local or per-ProcessingUnit queue; the render thread drains it each frame independently. Uses a tagged-union buffer internally (Candidate 1 approach).

This solves the "TODO: should be in order" comment in `DebugFrameData.cpp` and decouples debug geometry lifetime from the simulation frame. The cost is a new cross-thread transport mechanism (second stream or shared ring buffer with mutex) and more significant architectural change.

**Primary value:** Fully decouples debug rendering from the simulation frame pipeline; enables debug overlays that survive multiple frames (useful for path visualisation, persistent markers).

---

### Candidate 6: Compile-Time Type Registry via Template Specialisation
**Home module/system:** DiaGraphics — meta-programming layer over `DebugFrameData`
**Size:** L
**Description:**
Use C++20 concepts and template specialisation to build a type registry at compile time. `DebugFrameData` is templated on a type list; each type in the list gets a `DynamicArrayC<T, N>` slot automatically. Adding a new shape means adding it to the type list — no manual field declarations. The visitor is generated from the type list via fold expressions.

Elegant but significantly over-engineered for a debug utility. Template error messages will be painful; build times increase; the type list needs to be centralised. Realistically this buys nothing over the tagged union approach (Candidate 1) while adding substantial complexity.

**Primary value:** Maximum compile-time safety and zero runtime tag dispatch — but complexity cost far exceeds the benefit for debug rendering.

---

### Candidate 7: Tagged Union + Ordered Single Buffer + Kept Visitor Interface
**Home module/system:** DiaGraphics — combines Candidate 1's union with a refined visitor contract
**Size:** S
**Description:**
Same as Candidate 1 (tagged union, single `DynamicArrayC<DebugPrimitive, N>`) but deliberately preserves the existing `DebugFrameDataVisitor` interface structure. The visitor gains a `Visit(const DebugPrimitive&)` overload; `DebugFrameData::AcceptVisitor()` calls it once per item in insertion order. The per-type `Visit(Circle2D)` and `Visit(Line2D)` overloads remain as *optional* — the tagged-union visitor can delegate to them internally, or implementors can override `Visit(DebugPrimitive)` directly.

This is the most conservative extensible design: existing visitors (`DebugFrameRendererVisitor`) require minimal changes, insertion order is preserved (fixing the TODO), and the per-type cap problem is solved.

**Primary value:** Solves both scalability problems (shape count and primitive count) with minimal blast radius on the visitor consumer side; preserves backward compatibility with existing visitor implementations.

---

## Coverage Map

The seven candidates span the full range of the design axes identified in explore.md:

| Axis | Coverage |
|------|---------|
| Memory model | Static (1, 2, 7), Slab (3), Static+decoupled (5), Template-generated (6) |
| Type extensibility | Closed+bigger (2), Open via union (1, 7), Open via pointer+slab (3), Open via decoupling (5) |
| Copy cost | Trivial (1, 2, 7), Complex slab-rebuild (3), Separate transport (5) |
| Visitor compatibility | Preserved (2, 3, 7), Extended (1), Replaced (4), Decoupled (5) |
| Insertion ordering | Per-type batches (2, 3), Global order (1, 4, 5, 7) |
| Scope | Minimal fix (2), Targeted redesign (1, 7), Moderate refactor (3, 4), Architecture change (5, 6) |

Candidates 1 and 7 are closely related — 7 is the "conservative" variant of 1 that keeps the visitor interface intact. Together they represent the most likely sweet spot: open extensibility, trivial copy, no heap, and minimal change to existing consumers.
