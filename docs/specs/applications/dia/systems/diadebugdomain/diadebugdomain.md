# System Spec: DiaDebugDomain

## Parent Application
@docs/specs/applications/dia/dia.md

**Gameplay Domains:** tools, core

## Purpose

DiaDebugDomain is the redesigned visual debugging infrastructure for the Dia engine. It extends the existing `DebugLayerManager` — which already supports per-drawer priority, stage tags, a `DebugServer` broadcast seam, and a picking seam — by adding a structured group hierarchy above the drawer layer, replacing the flat 64-entry drawer cap, replacing the ImGui console with an Ultralight HTML panel, and establishing a formal module contract. The redesign is built around four deliverables:

- **`IDebugDomain`** — a new abstract interface that all visual debugger modules implement. Each domain owns its world-space drawers, exposes structured JSON state, and handles commands from the panel. Replaces per-drawer manual registration.
- **`DiaDebugDomainRegistry`** — a grouped domain registry replacing the flat `DynamicArrayC<LayerEntry, 64>`. No hard cap; groups by `IDebugDomain::GetGroup()`.
- **`DiaDebugPanel`** — an Ultralight-rendered HTML debug console (Alpine.js + DaisyUI + Tailwind) positioned on the left side of the screen. Features 8 group accordions, domain cards with per-drawer toggles + live stats rows + sliders, entity lock, and a command input strip. Replaces `DiaVisualDebuggerConsole` (ImGui).
- **`DiaXxxVisualDebugger` contract** — a formal AC checklist and `dia check debugger-contract` rule that all current and future visual debugger modules must satisfy.

The redesign covers 21 domains across 8 groups (Core Debug, Physics, Animation, Navigation, Rendering, Spatial/Geometry, Entity, AI/Behavior), migrates 14 existing debugger modules to the new interface, extracts two misplaced modules from `Adaptors/` into proper isolation, and retires the ImGui console.

**Mockup:** @docs/research/visual_debugger_redesign/mockup.html — approved UX; serves as the visual acceptance gate for `DiaDebugPanel`.

**Dependency chain:**
```
DiaDebugDomain → DiaVisualDebugger  (IVisualDebugger, DebugLayerManager, DebugColourPalette)
DiaDebugDomain → DiaUIUltralight    (DiaDebugPanel rendering)
DiaDebugDomain → DiaCore            (StringCRC, ColourRGBA, JsonWriter, JsonValue)
DiaXxxVisualDebugger → DiaDebugDomain (IDebugDomain)
DiaXxx (any system) ⊥ DiaXxxVisualDebugger  (zero dependency — SD-002)
```

## Responsibilities

- Define the `IDebugDomain` abstract interface in `DiaVisualDebugger/Domain/` (see Public Interfaces)
- Provide `DiaDebugDomainRegistry` for registering and querying domains by group
- Provide `DiaDebugPanel` — the Ultralight HTML page (`debug-panel.html`) implementing the approved mockup layout
- Wire `GetJSONState()` → JSON → Ultralight JS and `JS OnCommand()` → `IDebugDomain::OnCommand()` in `VisualDebuggerModule`
- Preserve `debug.layer.enable` / `debug.layer.list` / `debug.layer.disable` console commands unchanged
- Increase `DebugLayerManager::kMaxLayers` from 64 to at least 128 to accommodate the full domain drawer inventory across all migrated modules
- Migrate all 14 existing `IVisualDebugger`-registered visual debuggers to implement `IDebugDomain`
- Extract `DiaEntitySpatialVisualDebugger` and `DiaScalarFieldVisualDebugger` from their current `Adaptors/` locations into standalone module directories
- Define and publish the `DiaXxxVisualDebugger` contract — module isolation rules, interface completeness, `DebugColourPalette` compliance, scale-awareness, JSON minimum state, required command handlers, mandatory test shapes
- Add `dia check debugger-contract` validation rule covering all contract ACs
- Retire `DiaVisualDebuggerConsole` (ImGui) after all domains are migrated and `DiaDebugPanel` is verified
- Remove `DrawImGui()` from `IVisualDebugger`
- Provide `dia.diadebugdomain.architecture.module.md` YAML module documentation
- Provide module entries in `Cluiche.sln` for any new modules introduced

## Non-Responsibilities

> **Recommended spin-out:** The three DiaUIUltralight prerequisites (1) `CallJSFunction`, (2) keyboard injection, (3) input focus arbitration are a natural `DiaUIUltralight` feature spec (`/spec-feature ultralight-game-input-bridge` or similar). DiaDebugDomain should block on that spec's completion. Other in-game UI features (DiaChatPlugin, future HUD panels) would also benefit.

- Building the 9 new `DiaXxxVisualDebugger` modules (Steering, Pathfinding, FlowField, StateMachine, Rules, HTN, AIBudget, Blackboard, Mailbox) — these are separate system specs in the backlog
- Entity-centric view (lock-to-entity across all domains) — deferred after initial panel is live (Open Design Question 2)
- `DebugSnapshot<T>` double-buffered cross-PU data layer — evaluating before migration; may remain const-ref convention (Open Design Question 3)
- World-space drawing infrastructure changes — `IDebugDraw`, `IFixedPrimitiveBuffer`, draw call pipeline unchanged
- Editor-side (CluicheEditor) tooling for visual debuggers — separate concern
- Any gameplay system that is not a visual debugger (ArenaTestStageModule, stage-specific debug panels)

## Public Interfaces

### IDebugDomain

Lives in `Dia/DiaDebugDraw/Domain/IDebugDomain.h`.

```cpp
namespace Dia::VisualDebugger {

    // One per visual debugger module. The domain owns its IVisualDebugger drawers,
    // exposes structured JSON state, and handles panel commands.
    class IDebugDomain {
    public:
        virtual ~IDebugDomain() = default;

        // Identity
        virtual Dia::Core::StringCRC GetDomainId()      const = 0;
        virtual const char*          GetDisplayName()   const = 0;
        virtual const char*          GetDescription()   const = 0; // ≤80 chars
        virtual Dia::Core::StringCRC GetGroup()         const = 0; // e.g. "Physics"
        virtual Dia::Core::ColourRGBA GetAccentColour() const = 0; // panel card header tint

        // Whether this domain has world-space drawers.
        // Panel-only domains (StateMachine, Rules, UtilityAI, HTN, Blackboard, Mailbox)
        // override to return false; callers skip Register/GetDrawer* when false.
        virtual bool HasWorldDrawers() const { return true; }

        // Lifecycle — bulk-registers / unregisters all drawers in one call.
        // Default no-op for panel-only domains (HasWorldDrawers() == false).
        virtual void Register(DebugLayerManager& mgr)   {}
        virtual void Unregister(DebugLayerManager& mgr) {}

        // Data bridge to DiaDebugPanel
        // Called per-frame (or on demand from JS); writes at minimum:
        //   { "drawers": [{"name": "Shapes", "enabled": true}, ...], "stats": {} }
        virtual void GetJSONState(Dia::Core::JsonWriter& writer) = 0;

        // Commands from the panel (JSON → C++)
        // Required handlers: "toggle" {drawer: name}, "setScale" {key: name, value: float}
        virtual void OnCommand(Dia::Core::StringCRC cmd,
                               const Dia::Core::JsonValue& args) = 0;

        // World-space drawers — IVisualDebugger under the hood.
        // Default impls return 0/nullptr; only called when HasWorldDrawers() is true.
        virtual int              GetDrawerCount() const { return 0; }
        virtual IVisualDebugger* GetDrawer(int index)   { return nullptr; }
    };

} // namespace Dia::VisualDebugger
```

### DiaDebugDomainRegistry

Lives in `Dia/DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h`.

```cpp
namespace Dia::VisualDebugger {

    class DiaDebugDomainRegistry {
    public:
        void Register(IDebugDomain& domain);
        void Unregister(IDebugDomain& domain);

        // Find a domain by its ID (returns nullptr if not registered)
        IDebugDomain* FindDomain(Dia::Core::StringCRC domainId) const;

        // Visit all domains in a given group (groupId = e.g. "Physics")
        template<typename Fn>
        void VisitGroup(Dia::Core::StringCRC groupId, Fn&& fn) const;

        // Visit every registered domain
        template<typename Fn>
        void VisitAll(Fn&& fn) const;

        int GetDomainCount() const;
    };

} // namespace Dia::VisualDebugger
```

### DiaXxxVisualDebugger Contract

All current and future visual debugger modules must satisfy these ACs.

**Module isolation rules:**
1. Each visual debugger system = one `DiaXxxVisualDebugger` module (one `.vcxproj`, one module YAML)
2. `DiaXxx` (the owned system) has **zero** `#include` or link dependency on `DiaXxxVisualDebugger`
3. `DiaXxxVisualDebugger` may depend on: `DiaXxx` + `DiaVisualDebugger` + `DiaCore` + draw infrastructure only

**Interface completeness:**
4. Implements `IDebugDomain` — all pure virtual methods
5. `GetDescription()` string is ≤80 characters

**World-space draw rules:**
6. World-space draw colours from `DebugColourPalette` only — no hardcoded `ColourRGBA` literals in `Draw()` implementations. Panel accent tints returned by `GetAccentColour()` use `DebugGroupAccents` named constants (defined in the group accent table below; published as `Dia/DiaDebugDraw/Domain/DebugGroupAccents.h`); these are named constants, not inline literals, and are exempt from the `DebugColourPalette`-only rule.
7. All world-space sizes, radii, and lengths multiplied by `IDebugContext::GetDebugScale()`. Verified by the scale-sensitivity test shape (AC-15); not statically checkable by `dia check`.
8. No `ImGui::GetBackgroundDrawList()` or any other ImGui call in a visual debugger module

**Cross-PU data access:**
9. Read-only access to sim-owned data is via `const T&` (accepted convention) or `DebugSnapshot<T>` if double-buffering is required. No mutable shared state.

**JSON state:**
10. `GetJSONState()` emits at minimum `{ "drawers": [{name, enabled}], "stats": {} }`. Domains may include additional domain-specific fields (e.g. score tables, state lists, plan cursors, rule fire reports) consumed by domain-specific view code in `debug-panel.html`. The per-domain extended schema is defined in each domain's migration spec and is the normative contract between C++ and the panel's JS.

**Command handling:**
11. `OnCommand("toggle", {drawer: name})` — enables/disables the named drawer
12. `OnCommand("setScale", {key: name, value: float})` — updates a named scale parameter

**Cross-PU thread safety:** `OnCommand` arrives on the Render PU (panel JS → JS bridge → C++) while drawers execute on the Sim PU. Implementations must route mutations through an `std::atomic` flag or a small lock-free command queue drained by `VisualDebuggerModule::DoUpdate` on the Sim PU — not a raw member write across PU boundaries.

**Tests:**
13. `Tests/GoogleTests/DiaXxxVisualDebugger/TestXxxVisualDebugger.cpp` exists
14. Tests use `RecordingDebugVisitor` or mock `IDebugDraw`
15. Mandatory test shapes: enable/disable gate for each drawer, each drawer type emits correct primitive type, scale sensitivity (changing `GetDebugScale()` changes output measurements), `GetJSONState()` round-trip, `OnCommand("toggle", ...)` round-trip

**Panel card layout compliance:**
16. The domain's card section in `debug-panel.html` must not override the standard panel spacing from `docs/research/visual_debugger_redesign/mockup.html`: group header `padding: 5px 10px`, domain header `padding: 4px 8px`, domain card body `padding: 6px 10px 8px 10px`, `margin-bottom: 3px` between adjacent cards, drawer checkbox rows `gap: 5px 10px`. Accent color via `var(--accent)` CSS variable only — no hardcoded hex in domain-card HTML. Domain-specific content sections inside `domain-body` may use custom internal spacing but must not alter the outer values.

**Domain group accents (reference):**

These 8 values are published as `DebugGroupAccents` static constants in `Dia/DiaDebugDraw/Domain/DebugGroupAccents.h`. `GetAccentColour()` implementations return one of these — never an inline `ColourRGBA` literal.

| Group | StringCRC key | Accent colour | `DebugGroupAccents` constant |
|-------|---------------|---------------|------------------------------|
| Core Debug | `"CoreDebug"` | `#6b7280` | `DebugGroupAccents::kCoreDebug` |
| Physics | `"Physics"` | `#f59e0b` | `DebugGroupAccents::kPhysics` |
| Animation | `"Animation"` | `#10b981` | `DebugGroupAccents::kAnimation` |
| Navigation | `"Navigation"` | `#3b82f6` | `DebugGroupAccents::kNavigation` |
| Rendering | `"Rendering"` | `#8b5cf6` | `DebugGroupAccents::kRendering` |
| Spatial/Geometry | `"Spatial"` | `#06b6d4` | `DebugGroupAccents::kSpatial` |
| Entity | `"Entity"` | `#eab308` | `DebugGroupAccents::kEntity` |
| AI/Behavior | `"AIBehavior"` | `#ef4444` | `DebugGroupAccents::kAIBehavior` |

### Domain Inventory (21 domains, 8 groups)

| Group | Domain | Module | Status |
|-------|--------|--------|--------|
| Core Debug | Coord2D | DiaCoord2DVisualDebugger | Migrate |
| Core Debug | Coord3D | DiaCoord3DVisualDebugger | Migrate |
| Core Debug | AssetRuntime | DiaAssetRuntimeVisualDebugger | Migrate |
| Physics | RigidBody2D | DiaRigidBody2DVisualDebugger | Migrate (reference) |
| Physics | SoftBody2D | DiaSoftBody2DVisualDebugger | Migrate |
| Animation | Rig2D | DiaRig2DVisualDebugger | Migrate |
| Animation | IK2D | DiaIK2DVisualDebugger | Migrate |
| Animation | Animation2D | DiaAnimation2DVisualDebugger | Migrate |
| Navigation | Pathfinding | DiaPathfindingVisualDebugger | New (separate spec) |
| Navigation | FlowField | DiaFlowFieldVisualDebugger | New (separate spec) |
| Navigation | Steering | DiaSteeringVisualDebugger | New (separate spec) |
| Rendering | Mesh3D | DiaMesh3DVisualDebugger | Migrate |
| Rendering | Lighting3D | DiaLighting3DVisualDebugger | Migrate |
| Rendering | Scene2D | DiaScene2DVisualDebugger | Migrate |
| Spatial/Geometry | Geometry2D | DiaGeometry2DVisualDebugger | Migrate |
| Spatial/Geometry | EntitySpatial | DiaEntitySpatialVisualDebugger | Extract from Adaptors/ |
| Spatial/Geometry | ScalarField | DiaScalarFieldVisualDebugger | Extract from Adaptors/ |
| Entity | Entity | DiaEntityVisualDebugger | Migrate (reference) |
| AI/Behavior | UtilityAI | DiaUtilityAIVisualDebugger | Migrate |
| AI/Behavior | StateMachine | DiaStateMachineVisualDebugger | New (separate spec) |
| AI/Behavior | Rules | DiaRulesVisualDebugger | New (separate spec) |

*New Navigation and AI/Behavior domains are separate backlog specs; not in scope here.*

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| IDebugDomain Interface | New `IDebugDomain` abstract interface in `DiaVisualDebugger/Domain/`. All 10 pure virtual methods defined. Replaces bare `IVisualDebugger*` registration. | [idebugdomain-interface.md](idebugdomain-interface.md) | Draft |
| DiaDebugDomainRegistry | Grouped domain registry — `Register/Unregister(IDebugDomain&)`, `FindDomain(StringCRC)`, `VisitGroup(StringCRC, Fn)`, `VisitAll(Fn)`. Replaces flat 64-entry `LayerEntry` array. | [debug-domain-registry.md](debug-domain-registry.md) | Draft |
| DiaDebugPanel | Ultralight HTML debug console — `debug-panel.html` (Alpine.js + DaisyUI + Tailwind), left-side panel, 8 group accordions, domain cards with drawer toggles + stats rows + parameter sliders, entity lock, search, and command input. **Blocked on DiaUIUltralight keyboard + focus prereq** (search, command strip, and entity-lock text entry require `InjectKeyEvent` + focus arbitration). Phase 1 acceptance gate (no keyboard prereq): group accordions, drawer toggles, stats rows, sliders, and output log verified against `docs/research/visual_debugger_redesign/mockup.html`. Phase 2 (after keyboard prereq): search, command strip, entity-lock field. | [debug-panel.md](debug-panel.md) | Draft |
| JS Command Bridge | Per-frame `GetJSONState() → JSON → Ultralight JS`. Panel-initiated `OnCommand() → IDebugDomain`. Wired in `VisualDebuggerModule`. `debug.layer.*` console commands preserved. | [js-command-bridge.md](js-command-bridge.md) | Draft |
| DiaXxxVisualDebugger Contract | Formal 16-AC checklist published in this spec. `dia check debugger-contract` validates module isolation (ACs 1–3), interface completeness (ACs 4–5), ImGui-free code (AC-8), JSON minimum structure (AC-10), command handler presence (ACs 11–12), and test file existence (ACs 13–14). Scale-awareness (AC-7) and palette compliance (AC-6) are verified by the mandatory test shapes (AC-15), not static analysis. | [debugger-contract.md](debugger-contract.md) | Draft |
| Domain Migration | Migrate all 14 existing debuggers (Coord2D, Coord3D, AssetRuntime, RigidBody2D, SoftBody2D, Rig2D, IK2D, Animation2D, Mesh3D, Lighting3D, Scene2D, Geometry2D, Entity, UtilityAI) to implement `IDebugDomain`. RigidBody2D + Entity first as reference implementations. Each migration must: (a) preserve the drawer's current `stageTag` in `DebugLayerManager::Register`; (b) define the domain's extended `GetJSONState()` schema beyond the AC-10 minimum; (c) author the corresponding domain card HTML template section in `debug-panel.html`. | [domain-migration.md](domain-migration.md) | Draft |
| Module Extraction | Extract `DiaEntitySpatialVisualDebugger` and `DiaScalarFieldVisualDebugger` from `Adaptors/` subdirectories into proper standalone module directories. Fixes module isolation violation. | [module-extraction.md](module-extraction.md) | Draft |
| Console Retirement | Delete `DiaVisualDebuggerConsole` (ImGui). Remove `DrawImGui()` from `IVisualDebugger`. Occurs after all domains migrated and `DiaDebugPanel` verified. | [console-retirement.md](console-retirement.md) | Draft |

## Dependencies on Other Systems

**Required:**
- **DiaVisualDebugger** — `IVisualDebugger`, `DebugLayerManager`, `IDebugDraw`, `IDebugContext` (for `GetDebugScale()`), `DebugColourPalette`; this system extends and refactors DiaVisualDebugger
- **DiaUIUltralight** — CPU bitmap compositing and mouse injection are already wired. **Blocked on the Game Input Bridge feature** (`docs/specs/applications/dia/systems/diauiultralight/game-input-bridge.md`) which delivers: (1) `CallJSFunction` C++→JS push for per-frame domain state; (2) keyboard injection for the panel's search, command strip, and entity-lock field; (3) `EInputRouting` mode stack so game input is suppressed while a panel text field is focused.
- **DiaCore** — `StringCRC`, `ColourRGBA`, `JsonWriter`, `JsonValue`, `DynamicArrayC`

**Downstream (consumers of IDebugDomain):**
- All 14 migrated `DiaXxxVisualDebugger` modules
- All future `DiaXxxVisualDebugger` modules (Navigation × 3, AI/Behavior × 6+ in backlog)

**Explicitly excluded:**
- **ImGui** — no new ImGui usage; existing ImGui usages removed as part of migration
- **DiaEditor / CluicheEditor** — `DiaDebugPanel` is an in-game Ultralight overlay, not a CluicheEditor plugin

## Out of Scope

- The 9 new visual debugger modules in the backlog (Steering, Pathfinding, FlowField, StateMachine, Rules, HTN, AIBudget, Blackboard, Mailbox) — separate specs
- Entity-centric view (lock to an entity, all domains filter) — deferred
- `DebugSnapshot<T>` double-buffer infrastructure — evaluate before domain migration; may remain const-ref convention
- CluicheEditor integration — DiaDebugPanel is a standalone in-game overlay
- ArenaTestStageModule debug layer — stage-specific, not a generic domain
- Hot-reload of `debug-panel.html` — Ultralight reload at runtime is a tooling convenience, not a spec requirement

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Ultralight HTML for the debug panel, not reimplemented ImGui | DiaUIUltralight is fully wired. The approved mockup (`mockup.html`) uses Alpine.js + DaisyUI — exactly what Ultralight renders. Group accordions, search, stats rows, and sliders are native HTML; they would require significant custom ImGui widget work. | DiaDebugPanel, JS Command Bridge | Accepted | Yes |
| SD-002 | `DiaXxx` (the system) has zero dependency on `DiaXxxVisualDebugger` | Module isolation: the system must not pay for debug rendering infrastructure and must not force a dependency on `DiaVisualDebugger` or `DiaUIUltralight` in production builds. `DiaXxxVisualDebugger → DiaXxx` is the only permitted direction. | All debugger modules | Accepted | Yes |
| SD-003 | `IDebugDomain` owns its drawers; no external registration of bare `IVisualDebugger*` | `Register(DebugLayerManager&)` / `Unregister()` in a single call per domain. Eliminates per-drawer lifecycle management scattered across callers and prevents registration/unregistration mismatches. | IDebugDomain Interface | Accepted | Yes |
| SD-004 | JSON bridge (`GetJSONState` + `OnCommand`) as the only data path to `DiaDebugPanel` | No `DrawImGui()` or panel-draw methods in `IDebugDomain`. `VisualDebuggerModule` calls `GetJSONState()` per frame and pushes to the panel via `CallJSFunction` (C++→JS push). Panel initiates commands via `window.app.OnCommand(id, cmd, args)`. **Requires `UltralightUISystem::CallJSFunction` prerequisite** — this is a no-op in the current Ultralight backend and must be implemented before the JS Command Bridge feature can be built. Decouples renderer choice from domain data. | JS Command Bridge, all debugger modules | Accepted | Yes |
| SD-005 | `DebugColourPalette` is the only colour source for world-space drawers; hardcoded `ColourRGBA` literals forbidden | Visual consistency across all domains. Consistent palette is a contract requirement enforced by `dia check debugger-contract`. | All debugger modules | Accepted | Yes |
| SD-006 | All world-space sizes, radii, and positions multiplied by `IDebugContext::GetDebugScale()` | Scale-aware debug rendering — consistent appearance at any zoom or DPI. Enforced by `dia check`. | All debugger modules | Accepted | Yes |
| SD-007 | Panel positioned on the left side; game viewport on the right | Matches industry standard (Unreal GameplayDebugger, CDPR Cyberpunk). Confirmed in mockup review by user. | DiaDebugPanel | Accepted | Yes |
| SD-008 | `debug.layer.*` console commands preserved with no API change | Command compatibility — existing debugging scripts and muscle memory remain valid after the migration. The panel adds a visual layer; it does not replace the command interface. | JS Command Bridge | Accepted | Yes |
| SD-009 | `DiaVisualDebuggerConsole` (ImGui) retired after all domains are migrated; `DrawImGui()` removed from `IVisualDebugger` | Running two panel systems simultaneously creates confusion and maintenance cost. Retirement is gated on all 14 migrations passing contract validation and `DiaDebugPanel` verified against the mockup. | Console Retirement | Accepted | Yes |
| SD-010 | RigidBody2D + Entity are the two reference implementations for domain migration | These cover the two migration patterns: world-space physics debugger (many drawers, stats row, scale slider) and entity overlay debugger (panel-only, command handling). All subsequent migrations follow their pattern. | Domain Migration | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | All domain IDs, group IDs, and command keys in `IDebugDomain` use `StringCRC`. `DiaDebugDomainRegistry` lookup is by `StringCRC`. |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | `VisualDebuggerModule` (which wires the panel and registry) lives inside the PU/Phase/Module lifecycle. No standalone singletons for domain management. |
| PD-004 | Platform | No STL containers in public APIs | `IDebugDomain` virtual methods use DiaCore containers (`DynamicArrayC`) for any output parameters. `DiaDebugDomainRegistry::VisitAll/VisitGroup` use function templates, not `std::vector` return. |
| PD-005 | Platform | x64 only | All new `.vcxproj` files target x64 exclusively. |
| PD-006 | Platform | Visual Studio project files are source of truth | New debugger modules need `.vcxproj` + `.vcxproj.filters` entries. Use `dia docs vcxproj-add`. |
| PD-007 | Platform | C++20 required | Compiled under `/std:c++20`. Template visitor pattern in `DiaDebugDomainRegistry` uses C++20 concepts. |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | New `.vcxproj` files must NOT override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| PD-009 | Platform | Generated output under `Cluiche/out/` | Any panel HTML/CSS/JS build artifacts or debug logs go under `Cluiche/out/<AppName>/debug/`. |
| AD-001 | Dia App | Module YAML frontmatter | Each new or extracted module (`DiaEntitySpatialVisualDebugger`, `DiaScalarFieldVisualDebugger`, `DiaDebugPanel` wiring module) needs its own `dia.*.architecture.module.md`. |
| AD-002 | Dia App | No STL in public APIs | Reinforces PD-004. Internal implementation of `DiaDebugDomainRegistry` may use `std::unordered_map`; public interface may not. |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All new code in `Dia::VisualDebugger::` or `Dia::Debug::` as appropriate. |
| AD-004 | Dia App | ProcessingUnit/Phase/Module architecture | Reinforces PD-002. JS bridge wiring lives in `VisualDebuggerModule::Init/Shutdown`. |

## Open Design Questions

1. **Panel as in-game overlay or resize-viewport side panel?** The approved mockup assumes the game viewport is on the right and the panel occupies the left ~520px. Two options: (a) the panel is a compositor overlay that renders on top (game viewport unchanged, panel partially covers it), or (b) the game viewport is narrowed and the panel gets a fixed-width strip. Option (b) matches the mockup more faithfully but requires `VisualDebuggerModule` to push a viewport rectangle override to the camera/renderer on panel open. Decide before implementing `DiaDebugPanel` and the JS bridge wiring.

2. **Entity-centric view in scope for initial build?** The mockup shows an entity ID input and "Lock to Entity" button. The implementation path for entity lock is the existing picking seam (`IDebugContext::GetSelectedEntityId` / `SetSelectedEntityId` + `EntityPickingDrawer`) — entity selection arrives from the Sim PU's picking logic, not from a typed ID. However, wiring entity *filtering* across all 21 domains (so each domain only shows data for the locked entity) requires adding `SetFocusEntity(EntityHandle)` (or equivalent) to `IDebugDomain` and updating every migration. Decide before domain migration starts: initial build or phase 2.

3. **Cross-PU data safety: const-ref convention accepted or `DebugSnapshot<T>` required?** Current debugger modules read sim-thread-owned data from the render thread via `const T&` (no synchronisation). For most physics and geometry domains this is de facto safe under the current single-frame-lag rendering model, but it is technically a data race. Options: (a) accept as convention and document it clearly in the contract, (b) introduce `DebugSnapshot<T>` double-buffering for any domain that reads cross-PU state. Option (b) is correct but increases migration scope significantly. Decide before domain migration starts.

4. **Input focus arbitration granularity:** When a panel text field (search, command strip, entity lock) is focused, `app.OnInputFocusChanged(true)` tells `VisualDebuggerModule` to suppress game input. Two options: (a) suppress all keyboard input to the game while any panel field is focused (simple, prevents accidental game actions); (b) suppress only keys that conflict with game bindings (requires knowing the binding set). Option (a) is recommended. Decide before the DiaUIUltralight keyboard prereq is implemented.

## Status

**Status:** Done

**Plan:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.plan.md
