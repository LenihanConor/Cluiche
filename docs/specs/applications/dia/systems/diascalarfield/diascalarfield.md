# System Spec: DiaScalarField

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** ai, pathfinding, spatial

## Purpose

DiaScalarField is a topology-agnostic, grid-sized float field primitive for the Dia engine. It stores a scalar value per cell and provides propagation (diffusion/decay), write shapes, gradient queries, spatial queries, and weighted field combination. It knows nothing about factions, AI, or game rules — it is a pure spatial data structure.

Primary engine consumer is game-level influence maps — faction strength, danger, and resource fields wired up in game code. DiaFlowField remains a standalone system; both are parallel spatial primitives.

Grid topology (square vs hex) and propagation policy are template parameters. This eliminates virtual dispatch on the hot propagation path. Virtual escape hatches are provided for editor tooling where performance is not critical.

**Dependency chain:**
`DiaScalarField → DiaCore (containers, StringCRC, DIA_ASSERT, DIA_LOG_*)`
`DiaScalarField + DiaRules → RulesPropagationPolicy (optional, adaptor module)`
`DiaScalarField + DiaVisualDebugger → ScalarFieldOverlay (optional, adaptor module)`

## Responsibilities

- Define `CFieldTopology` concept — requires `ForEachNeighbour(CellIndex, callback)` and `GetCellCount()` on the topology type
- Provide `SquareFieldTopology` — rectangular grid, 4 or 8-connected; constexpr neighbour offsets, stateless (EBO applies)
- Provide `HexFieldTopology` — hexagonal grid, axial coordinates, 6-connected; constexpr neighbour offsets, stateless (EBO applies)
- Provide `DiaScalarField<Topology, Policy>` — double-buffered float array; templated to inline topology and policy on the propagation hot path
- Provide `UniformDecayPolicy` — configurable diffusion factor (per-neighbour) + decay rate (per-tick); default policy, no dependencies
- Provide `RulesPropagationPolicy` (optional header-only adaptor in `DiaScalarField/Adaptors/`) — bridges DiaRules condition evaluation to per-cell propagation modifiers; evaluates only dirty cells; requires DiaRules; consumers add DiaRules to their own project dependencies
- Provide blocked cell bitset mask — binary barrier; propagation stops entirely; branchless; zero-cost alongside float array
- Provide static modifier float map — pre-baked per-cell float multiplier (e.g. terrain costs); written at load or on terrain change; multiplied into propagation; free at runtime
- Provide double-buffering — propagate reads from buffer A, writes to buffer B, swap; fixes propagation correctness; enables concurrent reads while write buffer is being built
- Provide value clamping — configurable `[min, max]` per field; applied after propagation; prevents unbounded accumulation
- Provide write shape API — point write, radial write (center + radius + falloff curve), box write; all write into the pending write buffer
- Provide gradient query — sample 4/6 neighbours, compute central difference, return normalized `Vector2`; the direction of steepest ascent
- Provide spatial query API — `FindLocalMaxima(region)`, `FindCellsAboveThreshold(value, region)`; results via `DynamicArrayC<CellIndex>`
- Provide multi-field weighted combination — `DiaScalarField::Combine(result, {{&fieldA, 0.6f}, {&fieldB, -0.4f}})` static utility; negative weights supported
- Provide `ScalarFieldOverlay` (optional adaptor) — DiaVisualDebugger colour-gradient overlay; toggleable per field instance; configurable min/max colour mapping; requires DiaVisualDebugger
- Emit `DIA_LOG_INFO` on field construction and on topology mismatch assertions
- Provide test utilities under `DiaScalarField/Testing/` — `AssertCellValue`, `AssertGradientDirection`, `MockPropagationPolicy`; shipped with library, consumer opt-in via include
- Use DiaCore containers exclusively in all public APIs (AD-002 / PD-004)
- Provide `dia.scalarfield.architecture.module.md` YAML module documentation
- Provide `DiaScalarField.vcxproj` static library registered in `Cluiche.sln` under `domain/gameplay/core`

## Non-Responsibilities

- Faction wiring — who writes to the field and what the values mean is game code's concern
- Influence map semantics — DiaScalarField is the primitive; the "influence map" is the game-level application of it
- Per-entity AI decisions — gradient output is a steering input; decision logic lives in DiaUtilityAI, DiaRules, or game code
- Concurrent writes from multiple threads — single-writer model; entities enqueue write requests, one flush pass applies them before propagation
- Pathfinding graph queries (`GetNeighbours`, `IsPassable`) — DiaPathfinding owns its own grid types; `SquareFieldTopology` and `SquarePathGrid` are separate types with separate responsibilities
- Visual rendering beyond debug overlay — no render pass; `ScalarFieldOverlay` is a debug tool only

## Public Interfaces

### CFieldTopology Concept

```cpp
namespace Dia::ScalarField {
    template<typename T>
    concept CFieldTopology = requires(const T& t, CellIndex c) {
        { t.GetCellCount() }                          -> std::convertible_to<int>;
        { t.ForEachNeighbour(c, [](CellIndex){}) }    -> std::same_as<void>;
    };
}
```

### CellIndex

```cpp
namespace Dia::ScalarField {
    struct CellIndex {
        int x;  // col (square) or q (hex axial)
        int y;  // row (square) or r (hex axial)
        bool operator==(const CellIndex&) const = default;
    };
}
```

### SquareFieldTopology / HexFieldTopology

```cpp
namespace Dia::ScalarField {
    enum class SquareConnectivity { k4Connected, k8Connected };

    struct SquareFieldTopology {
        SquareFieldTopology(int width, int height,
                            SquareConnectivity c = SquareConnectivity::k8Connected);
        int  GetCellCount() const;
        void ForEachNeighbour(CellIndex cell, auto&& callback) const;
        int  GetWidth()  const;
        int  GetHeight() const;
    };

    struct HexFieldTopology {
        explicit HexFieldTopology(int radius);
        int  GetCellCount() const;
        void ForEachNeighbour(CellIndex cell, auto&& callback) const;
        int  GetRadius() const;
    };
}
```

### UniformDecayPolicy

```cpp
namespace Dia::ScalarField {
    struct UniformDecayParams {
        float diffusionFactor = 0.8f;   // multiplier applied to each neighbour contribution
        float decayRate       = 0.05f;  // subtracted from each cell per tick before diffusion
    };

    struct UniformDecayPolicy {
        explicit UniformDecayPolicy(UniformDecayParams params = {});
        float ComputeCell(CellIndex cell, float current,
                          float staticModifier,
                          float neighbourSum) const;
    };
}
```

### DiaScalarField

```cpp
namespace Dia::ScalarField {
    enum class FalloffCurve { kLinear, kQuadratic, kInverse };

    template<CFieldTopology Topology, typename Policy = UniformDecayPolicy>
    class DiaScalarField {
    public:
        DiaScalarField(Topology topology, Policy policy = {});

        // --- Barrier / modifier map ---
        void SetBlocked(CellIndex cell, bool blocked);
        bool IsBlocked(CellIndex cell) const;
        void SetStaticModifier(CellIndex cell, float modifier);  // pre-baked multiplier [0,1]

        // --- Value clamp ---
        void SetClampRange(float minValue, float maxValue);

        // --- Write shapes (queued; flushed before Tick) ---
        void WritePoint(CellIndex cell, float value);
        void WriteRadial(CellIndex center, float radius, float peakValue, FalloffCurve curve);
        void WriteBox(CellIndex topLeft, int width, int height, float value);

        // --- Propagation ---
        void Tick();   // flush pending writes, propagate, swap buffers, clamp

        // --- Read ---
        float GetValue(CellIndex cell) const;

        // --- Gradient query ---
        // Returns the normalized direction of steepest ascent at cell.
        // Returns zero vector if all neighbours are equal or cell is at boundary.
        Dia::Maths::Vector2 GetGradient(CellIndex cell) const;

        // --- Spatial queries ---
        void FindLocalMaxima(CellIndex regionTopLeft, int width, int height,
                             Dia::Core::DynamicArrayC<CellIndex>& outCells) const;
        void FindCellsAboveThreshold(float threshold,
                                     CellIndex regionTopLeft, int width, int height,
                                     Dia::Core::DynamicArrayC<CellIndex>& outCells) const;

        // --- Multi-field combination ---
        struct WeightedField {
            const DiaScalarField* field;
            float weight;  // negative weight = subtract
        };
        static void Combine(DiaScalarField& result,
                            Dia::Core::DynamicArrayC<WeightedField> inputs);

        // --- Inspection ---
        int GetCellCount() const;
        const Topology& GetTopology() const;
    };
}
```

### Type Aliases

```cpp
namespace Dia::ScalarField {
    using SquareScalarField = DiaScalarField<SquareFieldTopology, UniformDecayPolicy>;
    using HexScalarField    = DiaScalarField<HexFieldTopology,    UniformDecayPolicy>;
}
```

### ScalarFieldOverlay (optional adaptor — requires DiaVisualDebugger)

```cpp
namespace Dia::ScalarField {
    struct OverlayColourMap {
        Dia::Core::Colour lowColour  = Dia::Core::Colour::Blue();
        Dia::Core::Colour highColour = Dia::Core::Colour::Red();
        float             minValue   = 0.0f;
        float             maxValue   = 1.0f;
    };

    // Attach to a field; call Draw() each frame from the visual debugger layer.
    template<CFieldTopology Topology, typename Policy>
    class ScalarFieldOverlay {
    public:
        ScalarFieldOverlay(const DiaScalarField<Topology, Policy>& field,
                           Dia::Core::StringCRC layerName,
                           OverlayColourMap colourMap = {});

        void SetEnabled(bool enabled);
        void Draw(Dia::Graphics::ICanvas& canvas) const;
    };
}
```

### Log Channel

```cpp
namespace Dia::ScalarField {
    static constexpr Dia::Core::StringCRC kLogChannel{"ScalarField"};
}
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| CFieldTopology Concept | C++20 concept constraining topology types to `ForEachNeighbour` + `GetCellCount`. Inlines neighbour iteration on the propagation hot path. | inline | Draft |
| SquareFieldTopology | Rectangular grid, 4 or 8-connected. Stateless struct (EBO). Constexpr neighbour offsets. | inline | Draft |
| HexFieldTopology | Hexagonal grid, axial coordinates, 6-connected. Stateless struct (EBO). Constexpr neighbour offsets. | inline | Draft |
| UniformDecayPolicy | Configurable diffusion factor + decay rate per tick. No dependencies. Default policy. | inline | Draft |
| RulesPropagationPolicy | Bridges DiaRules condition tree to per-cell propagation modifiers. Dirty-cell gated. Header-only adaptor in `DiaScalarField/Adaptors/`. | inline | Draft |
| Blocked Cell Mask | Per-cell bitset; propagation stops entirely at blocked cells. Branchless. | inline | Draft |
| Static Modifier Map | Per-cell float multiplier pre-baked at load/terrain-change. Multiplied into propagation. Shared terrain cost source for DiaPathfinding. | inline | Draft |
| Double Buffering | Propagate reads buffer A, writes buffer B, swap. Fixes propagation correctness. Enables concurrent reads. | inline | Draft |
| Value Clamping | Configurable [min, max] per field. Applied post-propagation. Prevents unbounded accumulation. | inline | Draft |
| Write Shape API | Point, radial (center + radius + falloff curve), box. All queued; flushed before Tick(). | inline | Draft |
| Gradient Query | Central difference on 4/6 neighbours → normalized Vector2. Direction of steepest ascent. | inline | Draft |
| Spatial Query API | `FindLocalMaxima` + `FindCellsAboveThreshold` over a region. Results via `DynamicArrayC<CellIndex>`. | inline | Draft |
| Multi-Field Combine | Static `Combine()` with weighted inputs. Negative weights supported for subtraction. | inline | Draft |
| ScalarFieldOverlay | DiaVisualDebugger colour-gradient overlay. Toggleable per instance. Configurable colour map. Optional adaptor. | inline | Draft |
| Test Utilities | `DiaScalarField/Testing/` — `AssertCellValue`, `AssertGradientDirection`, `MockPropagationPolicy`. Ships with library. | inline | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaCore** — `DynamicArrayC` (spatial query results, write queue), `StringCRC` (log channel), `DIA_ASSERT`, `DIA_LOG_*`
- **DiaMaths** — `Vector2` (gradient query output only)

**Optional adaptors (separate compilation units, not in `DiaScalarField.vcxproj`):**
- **DiaRules** — `RulesPropagationPolicy` adaptor; only included by consumers that need rule-driven propagation
- **DiaVisualDebugger** — `ScalarFieldOverlay` adaptor; only included by consumers that want debug overlays

**Explicitly excluded:**
- **DiaGeometry2D** — `DiaScalarField` owns its own topology types; avoids coupling to the geometry module and keeps the field self-contained
- **DiaPathfinding** — terrain cost sharing is one-directional (modifier map → `IPathCostProvider`); pathfinding has no compile-time dependency on DiaScalarField
- **DiaBlackboard** — callers read blackboard slots to determine write targets; the field itself has no blackboard dependency
- **DiaApplicationFlow** — `Tick()` is called from a Module on SimPU but has no compile-time dependency on the phase system

**Dependents (consumers):**
- Game-level influence maps — faction strength, danger, resource fields wired in game code
- `DiaSteering` — consumes gradient output as a new `FieldSeek` / `FieldFlee` steering behaviour input
- `DiaFlowField` — remains standalone; parallel spatial primitive, no dependency on DiaScalarField

## Out of Scope

- Concurrent writes from multiple threads — single-writer flush model only in v1
- Faction semantics — who writes, what values mean, and how AI reads them is game code
- NavMesh or non-grid spatial structures
- 3D scalar volumes — 2D grids only in v1
- Persistence / serialization — fields are reconstructed at runtime from writes

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SFD-001 | Template on `<Topology, Policy>` for the propagation hot path | `ForEachNeighbour` is called for every cell every tick. Vtable dispatch here is measurable overhead on large grids. Concepts + templates inline the call; compiler generates a specialised `Tick()` per topology+policy pair. Same flexibility as virtual, zero runtime cost on the hot path. | DiaScalarField::Tick | Accepted | Yes |
| SFD-002 | Virtual escape hatch (`IDiaScalarField`) provided for editor tooling | Editor panels need to store heterogeneous field references (square + hex in the same overlay list). Type erasure via a thin virtual wrapper over the templated types allows this without baking virtual into the hot path. | ScalarFieldOverlay, editor tooling | Accepted | Yes |
| SFD-003 | Blocked cells are a first-class bitset, not a rule | Blocked = binary barrier (propagation stops entirely). Rules = continuous float modifier. Different data structures, different costs. Collapsing blocked into rules would pay rule evaluation overhead for a null check. Mirrors `IsPassable` in DiaPathfinding. | Blocked Cell Mask | Accepted | Yes |
| SFD-004 | Static modifier map is a pre-baked float array, not a rules evaluation | Terrain costs (swamp, road) are static — known at load time or on terrain change events. Pre-baking them into a float array makes runtime propagation pure float multiply, cache-friendly, zero branch. Rules are reserved for genuinely dynamic per-tick conditions. | Static Modifier Map | Accepted | Yes |
| SFD-005 | Single-writer flush; no concurrent writes in v1 | Atomic float accumulation causes false-sharing cache thrash under contention (50 units writing clustered cells → ~5–10× slower throughput). Single-writer flush is plain sequential float math — fast and correct. Influence fields don't need sub-tick write visibility; 1-tick delay is imperceptible. | Write Shape API | Accepted | Yes |
| SFD-006 | DiaFlowField stays standalone; no DijkstraFillPolicy in DiaScalarField | FlowField owns `FlowFieldCache` (named dirty-flag invalidation per sector) which has no place in a generic scalar field. The concept shapes differ (`CFlowFieldGraph` vs `CFieldTopology`). Coupling buys shared float array storage — not worth the added dependency on a working shipped system. Both are parallel spatial primitives. | DiaFlowField relationship | Accepted | Yes |
| SFD-007 | Static modifier map is a potential shared terrain cost source for DiaPathfinding | Both systems need terrain cost per cell. `IPathCostProvider` implementations may read from a DiaScalarField's modifier map at load time, eliminating duplicate terrain cost storage. This is a wiring concern for game/terrain code — DiaScalarField has no compile-time dependency on DiaPathfinding. | Static Modifier Map | Accepted | No |
| SFD-008 | `RulesPropagationPolicy` evaluates only dirty cells (propagation wavefront) | Naive per-cell-per-tick rule evaluation on a 200×200 grid = ~40k × 50ns = 2ms/tick. Dirty-cell gating reduces active cells to the boundary wavefront (typically 50–200 cells on a settled field). Pre-baked static modifiers cover the majority of terrain effects without rules. Rules are the escape hatch for genuinely dynamic per-tick conditions. | RulesPropagationPolicy | Accepted | Yes |
| SFD-009 | `RulesPropagationPolicy` is a header-only adaptor in `DiaScalarField/Adaptors/` | Keeping it header-only means `DiaScalarField.vcxproj` carries no hard link dependency on DiaRules. Consumers who need rule-driven propagation include the header and add DiaRules to their own project dependencies. Same pattern as test utilities. | RulesPropagationPolicy | Accepted | Yes |
| SFD-010 | Test utilities ship inside `DiaScalarField/Testing/` | Platform-wide pattern (DiaStateMachine, DiaBlackboard, DiaPathfinding, DiaCommand). | Test Utilities | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Log channel key is `StringCRC`. Overlay layer names are `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `DiaScalarField::Tick()` is called from a Module on SimPU. No compile-time dependency on DiaApplicationFlow. |
| PD-004 | Platform | No STL containers in public APIs | Spatial query outputs, write queues, and weighted-combine input lists use `DiaCore::DynamicArrayC`. Internal propagation buffers may use `std::vector`. |
| PD-005 | Platform | x64 only | `DiaScalarField.vcxproj` targets x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaScalarField.vcxproj` and `.vcxproj.filters` maintained manually. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. `CFieldTopology` concept requires C++20. |
| PD-008 | Platform | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaScalarField.vcxproj` must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under Cluiche/out/ | Any log output goes under `Cluiche/out/<AppName>/`. |
| AD-001 | Dia App | Module system with YAML frontmatter | Create `dia.scalarfield.architecture.module.md` with public API, responsibilities, and dependency declarations. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004. Internal float buffers may use `std::vector`. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::ScalarField::` namespace. |

## Resolved Design Questions

1. **DiaFlowField relationship** — DiaFlowField stays standalone. No migration. Both are parallel spatial primitives with different concerns (FlowField owns cache + dirty-flag invalidation; scalar field owns diffusion + influence). Resolved: SFD-006.

2. **`RulesPropagationPolicy` placement** — Header-only adaptor in `DiaScalarField/Adaptors/RulesPropagationPolicy.h`. No `.vcxproj` dependency on DiaRules. Consumers opt in explicitly. Resolved: SFD-009.

3. **Gradient query at field boundaries** — Missing neighbours treated as value 0. This produces an inward-pointing gradient component at boundary cells — units following a danger gradient are naturally steered away from map edges. Implementors should not treat this as a bug.

## Status

**Status:** `Done`

**Plan:** @docs/specs/applications/dia/systems/diascalarfield/diascalarfield.plan.md
