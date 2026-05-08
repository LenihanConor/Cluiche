# Research: Ideate — Visual Debuggers

**Input:** docs/research/visual_debug/explore.md

---

## Cross-Cutting Decisions

These decisions apply to all candidates and are resolved here so specs don't relitigate them.

| Decision | Resolution |
|----------|------------|
| **Release builds** | All debug draw code excluded in Release via `#ifdef DIA_DEBUG`. Visual debugger vcxprojs do not link in Release. `DebugFrameData::RequestDraw()` is a no-op outside `DIA_DEBUG`. A future "Tuning" profile may selectively enable some layers but is out of scope now. |
| **Draw order** | Explicit integer priority registered per layer at `DebugLayerManager::Register()` time, default 0. Manager sorts layers by priority before calling `Draw()` each frame; ties broken by registration order. Canonical tiers: 0 = spatial structures, 10 = simulation shapes, 20 = secondary overlays (arrows/constraints), 30 = goals/targets (IK target, animation cursor), 40 = state indicators (convergence, warnings). Game code may use any integer. |
| **Primitive budget** | Global budget (Candidate 7), configurable at construction. First-come-first-served across all layers in draw-priority order. `DroppedCount()` logged via DiaLogger on overflow each frame. No per-layer allocation. |
| **World scale** | A global `float debugScale` on `DebugLayerManager` (default 1.0) applied to all size/length parameters (arrow lengths, circle radii) at draw time. Visual debuggers read it from the manager before submitting primitives. |
| **Multiple instances** | Multiple instances of the same debugger type (e.g. two physics worlds) each register with a unique `StringCRC` layer name. Registration-time `DIA_ASSERT` if a name collides. Canonical layer name constants live in a shared `DebugLayerNames.h` header in `DiaVisualDebugger` to prevent accidental CRC collisions. |
| **Hot reload** | Deferred. Document in `DebugLayerManager` API that registered lambdas must not capture references to hot-reloadable modules without manual deregister/reregister. |
| **Canonical colour palette** | A `DebugColourPalette` header in `DiaVisualDebugger` defines RGBA constants all debuggers must use: white = dynamic/active, grey = static/sleeping/inactive, green = healthy/converged, yellow = warning/best-effort, red = error/failed/torn, cyan = target/goal, magenta = pinned/constrained, orange = capped/limit-hit, dark blue = deep sleep. Ad-hoc colours in existing debuggers are replaced in the Candidate 2 options pass. |
| **Layer name collision** | `DIA_ASSERT` at `Register()` time if a `StringCRC` layer name is already registered. Canonical names defined as `StringCRC` constants in `DebugLayerNames.h`. |
| **Test strategy** | Count + type verification per sub-layer (not golden-value snapshots). Seven test types per debugger: (1) toggle isolation — one `showX` flag at a time, verify expected primitive types only; (2) all-on minimum count; (3) disabled emits nothing; (4) sub-layer isolation via manager commands; (5) no simulation mutation — world state unchanged after Draw(); (6) options round-trip; (7) overflow — exceed budget, verify DroppedCount() > 0 and no crash. |
| **ImGui / imgui-sfml** | Approved as new external dependency. ImGui (MIT) in `External/imgui/`, `imgui-sfml` bridge added to `DiaSFML`. |

---

## Candidates

### Candidate 1: DiaVisualDebugger — DebugLayerManager + IVisualDebugger Infrastructure
**Home module/system:** New `DiaVisualDebugger` static library (separate vcxproj)  
**Size:** M  
**Description:** A foundational infrastructure module containing:

- `IVisualDebugger` interface: `Draw(FrameData&)`, `SetEnabled(bool)`, `IsEnabled()`, `GetLayerName()`
- `DebugLayerManager`: registry of named layers, each wrapping one `IVisualDebugger`. Sorted by priority before each `Draw()` call (see draw order decision above). Holds global `debugScale` factor. Registration-time `DIA_ASSERT` on name collision.
- `DebugColourPalette.h`: canonical RGBA constants (see colour palette decision above)
- `DebugLayerNames.h`: canonical `StringCRC` layer name constants for all Dia debuggers
- Sub-layer state: each registered debugger exposes named sub-layers (`physics.shapes`, `physics.velocity`, etc.) toggleable independently. Sub-layer state broadcast via `debug.layer.state` so editor panel renders a checkbox tree.
- DiaAPI commands: `debug.layer.enable/disable/list`, `debug.sublayer.enable/disable`, `debug.pick` (no-op stub, seam for scene editor), `debug.scale <float>`
- Picking seams: `SetSelectedEntityId(uint32_t)` / `GetSelectedEntityId()` stubs; `ScreenToWorld` callback registration hook (undocumented until scene editor)
- Guarded by `#ifdef DIA_DEBUG` throughout

Existing debugger classes adapted at registration time via lambda: `manager.Register(StringCRC("physics"), priority, [&](FrameData& fd){ physicsDebugger.Draw(world, fd); })` — debugger class APIs unchanged. Uses `DynamicArrayC` (PD-004), `StringCRC` (PD-001).  
**Primary value:** Every future visual debugger gets priority-ordered draw, sub-layer toggle, global scale, editor panel binding, and overflow reporting for free. Single registration call replaces ad-hoc `SetEnabled()` on each debugger.

---

### Candidate 2: VisualDebuggerOptions Pass — Existing Debuggers
**Home module/system:** `DiaRig2DVisualDebugger`, `DiaRigidBody2DVisualDebugger`, `DiaSoftBody2DVisualDebugger`  
**Size:** S  
**Description:** A configurability pass across all three existing (or designed) debuggers, replacing the current minimal options with comprehensive `VisualDebuggerOptions` structs. Each flag maps directly to a sub-layer name registered with `DebugLayerManager`. All flags default to `true` for backwards compatibility.

**DiaRig2DVisualDebugger** additions:

| Flag | Controls |
|------|----------|
| `showBones` | Line segments between joints |
| `showJoints` | Circle at each joint |
| `showBoneNames` | Text label at joint (requires TextPrimitive — Candidate 6) |
| `showDirectionArrows` | Arrow on each bone showing local +X |
| `showRestPose` | Bind/rest pose drawn as grey ghost |
| `showRootIndicator` | Highlight root joint distinctly |
| `boneColour`, `rootColour`, `leafColour` | Per-role colour overrides (fall back to DebugColourPalette) |
| `jointRadius` | Circle size scale |

**DiaRigidBody2DVisualDebugger** additions (current has only `velocityArrowScale`, `velocityArrowMaxLen`):

| Flag | Controls |
|------|----------|
| `showShapes` | Collision shape outlines |
| `showAABB` | Broad-phase bounding boxes |
| `showVelocityArrows` | Velocity vectors |
| `showContactNormals` | Contact point normals |
| `showConstraints` | Constraint anchor-to-anchor lines |
| `showSleepingBodies` | Toggle sleeping bodies (often noisy) |
| `showStaticBodies` | Toggle static bodies (often clutter) |
| `contactNormalScale` | Length scale for contact normal lines |
| `colourByBodyType` | Standard colour coding vs. flat colour |

**DiaSoftBody2DVisualDebugger** additions (designed but not yet built — get options right from day one):

| Flag | Controls |
|------|----------|
| `showParticles` | Particle circles |
| `showStructuralConstraints` | White structural constraint lines |
| `showShearConstraints` | Cyan shear constraint lines |
| `showBendConstraints` | Blue bend constraint lines |
| `showAnchorLinks` | Yellow anchor-to-world lines |
| `showVelocityArrows` | Velocity vectors |
| `showTornParticles` | Orange torn state marker |
| `showPinnedParticles` | Magenta pinned state marker |
| `velocityArrowCapMarker` | Tip marker when arrow is capped |
| `particleRadius` | Circle size scale |

**Primary value:** Transforms blunt on/off toggles into surgical per-feature visibility. A developer debugging contacts doesn't need velocity arrows cluttering the view. Aligns all debuggers to the sub-layer model required by Candidate 1.

---

### Candidate 3: DiaIK2DVisualDebugger
**Home module/system:** New `DiaIK2DVisualDebugger` static library (separate vcxproj, follows existing pattern)  
**Size:** S  
**Description:** A standalone visual debugger for the `DiaIK2D` module. Draws each registered `IKChain` using world transforms from `IKSolver::GetWorldTransforms()` (requires a new `GetWorldTransforms()` accessor — currently private). `Draw()` signature: `Draw(const IKSolver&, const Vector2D& target, const PoleVector* poleVector, FrameData&)` — target and pole vector are passed per call since the solver doesn't store them between frames.

`VisualDebuggerOptions`:

| Flag | Controls |
|------|----------|
| `showBones` | Line segments between joints (suppress if DiaRig2D layer also active to avoid double-draw) |
| `showJoints` | Circle at each joint |
| `showTarget` | Cyan circle at the IK target position |
| `showPoleVector` | Ray from chain root in pole vector direction |
| `showJointLimits` | Arc2D showing min/max angle constraint per joint |
| `showConvergence` | Colour indicator: green = converged, yellow = best-effort, red = failed |
| `showChainId` | Text label at chain root (requires TextPrimitive — Candidate 6) |
| `suppressBonesIfRigActive` | Bool — automatically hides `showBones` when the Rig2D layer is enabled, preventing double-draw |

**What is genuinely new vs. DiaRig2DVisualDebugger:** target marker, pole vector ray, joint constraint arcs, convergence indicator. Bone lines are shared concept but the suppress flag handles the overlap cleanly.  
**Primary value:** Pole vector direction and joint limits are impossible to reason about without visualisation. Makes foot IK, hand grab, and look-at targets immediately inspectable.

---

### Candidate 4: DiaGeometry2DVisualDebugger
**Home module/system:** New `DiaGeometry2DVisualDebugger` static library  
**Size:** M  
**Description:** A visual debugger for `DiaGeometry2D` with two distinct draw modes. `Draw()` is overloaded for each:

**Shape draw mode** — renders any of the 13 primitive shape types inline in world space. Useful for visualising collision query regions, intersection test inputs, and trigger volumes. Each shape type maps to existing `DebugPrimitive` types: Circle → Circle2D, AARect/OORect → Rect2D or Line2D outline, Capsule → two circles + two lines, ConvexPolygon → Line2D per edge, Ray/Line/Arc/Sector → direct Ray2D/Arc2D mappings, Triangle → Triangle2D.

**Spatial structure draw mode** — renders internal cell/node boundaries of `SpatialGrid`, `Quadtree`, and `BVH`. Colour-coded by occupancy: empty = dark grey, occupied = white, over-capacity = red. Shows the structure of the broad-phase at the current frame.

`VisualDebuggerOptions`:

| Flag | Controls |
|------|----------|
| `showShapeOutlines` | Individual shape draw mode |
| `showSpatialGrid` | SpatialGrid cell boundaries |
| `showQuadtree` | Quadtree node boundaries |
| `showBVH` | BVH node boundaries |
| `showOccupancy` | Colour cells by occupancy level |
| `showIntersectionContacts` | Contact points and normals from intersection results |
| `showEmptyCells` | Toggle empty cells (often clutter in sparse scenes) |

**Primary value:** Spatial partitioning and collision query regions are invisible by default. BVH imbalance, broad-phase misses, and oversized query regions require this to diagnose.

---

### Candidate 5: DiaAnimation2DVisualDebugger
**Home module/system:** New `DiaAnimation2DVisualDebugger` static library  
**Size:** M  
**Description:** A visual debugger for the `DiaAnimation2D` module (spec approved, not yet built). **Blocked until DiaAnimation2D is implemented** — this is a forward commitment to be specced when DiaAnimation2D ships. Draws: spring chain particle positions and rest-pose offsets (circles + lines), clip playback cursor as a fraction bar at the skeleton root, blend stack weight bars (horizontal line segments scaled by weight), and IK target influence markers (small squares at IK target positions).

`VisualDebuggerOptions`:

| Flag | Controls |
|------|----------|
| `showSpringChains` | Spring particle positions and rest-pose offsets |
| `showClipCursor` | Playback position fraction bar |
| `showBlendWeights` | Blend stack weight bars |
| `showIKTargetInfluence` | IK target influence markers |
| `showLayerNames` | Text label per blend layer (requires TextPrimitive — Candidate 6) |

**Primary value:** Spring secondary motion and blend stack debugging are opaque without visualisation. Closing this gap is essential once DiaAnimation2D ships.

---

### Candidate 6: TextPrimitive — World-Space Text in DebugPrimitive
**Home module/system:** `DiaGraphics` — `DebugPrimitive` tagged union + DiaSFML visitor  
**Size:** S  
**Description:** Extend the `DebugPrimitive` tagged union with a `Text2D` variant: world-space position, a fixed-length char buffer (e.g. `char text[64]`), font size (float), and RGBA colour. DiaSFML's `DebugFrameDataVisitor` renders it using `sf::Text` with SFML's built-in default font — no external font resource system required. The char buffer is fixed-length (PD-004: no STL `std::string` in the union). `kDebugPrimitiveCapacity` remains configurable (Candidate 7). This unlocks bone name labels, chain IDs, entity IDs, body state text, and layer names drawn in world space across all debuggers.  
**Primary value:** Without this, any label in a visual debugger either requires ImGui (screen-space only) or is impossible. World-space text is essential for bone names, entity IDs, and IK chain identifiers rendered at their position in the scene.

---

### Candidate 7: Configurable DebugPrimitive Budget
**Home module/system:** `DiaGraphics` — `DebugFrameData`  
**Size:** S  
**Description:** Make the `DebugFrameData` primitive capacity configurable at construction time (currently hard-coded to 1024). Add a constructor parameter `uint32_t capacity` defaulting to 1024 for backwards compatibility. Add `bool IsOverCapacity() const` and `uint32_t DroppedCount() const` so callers know when primitives are being silently dropped. The `DebugLayerManager` (Candidate 1) reads `DroppedCount()` and broadcasts an overflow warning in the `debug.layer.state` payload so the editor panel can surface it. With all debuggers active simultaneously in a complex scene, 1024 is easily exhausted.  
**Primary value:** Makes invisible primitive dropping visible and diagnosable; lets large scenes increase the budget without an engine change. First-come-first-served across layers in draw-priority order — highest-priority layers never get starved by lower-priority ones because the manager calls `Draw()` in priority order and stops submitting when full.

---

### Candidate 8: Editor Debug Layer Panel (CluicheEditor Plugin)
**Home module/system:** New `DiaVisualDebuggerLayerEditorPlugin` alongside existing `DiaApplicationEditor` plugins  
**Size:** S  
**Description:** A CluicheEditor panel plugin that subscribes to the `debug.layer.state` data topic (broadcast by `DebugLayerManager`) and renders a **tree of checkboxes**: one row per registered debugger with expandable sub-layers beneath (e.g. `Physics ▶ Shapes, AABB, Velocity, Contacts, Constraints`). Clicking any node sends `debug.layer.enable/disable` or `debug.sublayer.enable/disable` DiaAPI commands over WebSocket. Also shows a per-layer primitive count and a red overflow badge when `DroppedCount() > 0` (from Candidate 7). Implemented as a React component in the CEF UI layer with a C++ `IEditorPlugin` backing class. Depends on Candidate 1 (DebugLayerManager) for the command and subscription protocol.  
**Primary value:** The editor becomes the primary control surface for all debug layers — no in-game keybindings needed for every new debugger. The sub-layer tree maps exactly to `VisualDebuggerOptions` flags, giving fine-grained control without opening source code.

---

### Candidate 9: DiaVisualDebuggerConsole (ImGui-based in-game debug console)
**Home module/system:** New `DiaVisualDebuggerConsole` static library + `imgui-sfml` bridge in `DiaSFML`. ImGui lives in `External/imgui/`.  
**Size:** M  
**Description:** An ImGui-backed in-game debug console rendered directly into the game viewport as an immediate-mode overlay. Toggled by a configurable keypress (e.g. backtick). Four panels:

- **Layer toggle tree** — mirrors the editor panel (Candidate 8): checkbox tree of all `DebugLayerManager` layers and sub-layers, driven by the same DiaAPI commands. Works without the editor connected.
- **Metrics bar** — FPS, frame time per ProcessingUnit (Main/Render/Sim), dropped primitive count with overflow indicator.
- **Command input line** — type any registered DiaAPI command and execute it in-game. All current and future DiaAPI commands are reachable without recompilation.
- **Log tail** — last N lines from DiaLogger, colour-coded by severity (info = white, warning = yellow, error = red).

ImGui renders via `imgui-sfml` (well-maintained open-source bridge: two `.cpp` files added to `DiaSFML`). No custom font system required — ImGui ships its own embedded font. **Supersedes the minimal Debug HUD concept** — if DiaVisualDebuggerConsole is built, a simpler SFML-text HUD is unnecessary.  
**Primary value:** Complete in-game debug control surface without needing the editor connected. The command input line makes DiaVisualDebuggerConsole future-proof — any new DiaAPI command is immediately accessible in-game. Matches the standard developers know from Unreal (`~` console), Unity (development build overlay), and most commercial engines.

---

### Candidate 10: Entity Picking — Deferred (planned for scene editor)
**Home module/system:** Future scene editor research  
**Size:** L (grows to XL with property inspector)  
**Status:** Deferred. Picking requires decisions about entity ID convention (PD-003), camera coordinate transform API, and editor property inspector protocol — all of which the scene editor research should own. Building picking now would mean making those decisions in the wrong context and retrofitting them later.

**Seams left in this pass so picking slots in cleanly:**

| Seam | Candidate | What to do now |
|------|-----------|----------------|
| Entity ID on primitives | 7 (DebugPrimitive) | Reserve `uint32_t entityId = 0` in the struct (0 = untagged); no breaking change later |
| Selected entity state | 1 (DebugLayerManager) | Add `SetSelectedEntityId(uint32_t)` / `GetSelectedEntityId()` stub — draws nothing yet |
| `debug.pick` command | 1 (DebugLayerManager) | Register as a no-op DiaAPI command; editor can call it without a protocol change later |
| ScreenToWorld callback | 1 (DebugLayerManager) | Document the registration hook in the API; leave the slot but don't require it |

**Primary value (when built):** Transforms the debugger from a scene-wide overlay into an interactive inspector. Deferred until scene editor research defines the entity model.

---

### Candidate 11: Debug State Rewind — Deferred (global simulation recording problem)
**Home module/system:** Future `DiaReplay` / `DiaSimCapture` research  
**Size:** XL  
**Status:** Deferred. Rewind is not a debugger feature — it is a simulation state recording system that the debugger would consume. A primitive-only ring buffer built here would be at the wrong layer and would be replaced when a proper `DiaReplay` system is designed. The right design records simulation state (physics bodies, bone transforms, particle positions) and the visual debugger plays back from that recorded state, not from a snapshot of already-rendered primitives.

**Seam left in this pass:**
- `DebugFrameData` must remain trivially copyable — document this as an explicit requirement so no non-copyable member is added without knowing why. A future rewind system needs to snapshot it per frame.

**Primary value (when built):** Enables scrubbing back to the exact frame a collision glitch or animation pop occurred. Deferred until a global replay/simulation capture system is designed.

---

## Coverage Map

The 11 candidates span the full design space from explore.md:

| Design axis | Candidates |
|-------------|------------|
| Shared interface / manager + sub-layers | 1 (DiaVisualDebugger), 8 (editor panel), 7 (budget) |
| Configurability of existing debuggers | 2 (options pass on Rig2D, RigidBody2D, SoftBody2D) |
| New in-game simulation debuggers | 3 (IK2D), 4 (Geometry2D), 5 (Animation2D) |
| World-space text rendering | 6 (TextPrimitive) |
| Editor control surface | 8 (layer panel plugin) |
| In-game control surface | 9 (DiaVisualDebuggerConsole, supersedes minimal HUD) |
| Advanced / complex features | 10 (picking), 11 (rewind) |

**Scope range:** S (2, 3, 6, 7, 8) → M (1, 4, 5, 9) → L (10) → XL (11)

**Dependency chain:**
- Candidates 2, 3, 4, 6, 7 are fully independent today
- Candidates 8 and 9 depend on Candidate 1 (DebugLayerManager)
- Candidate 5 (Animation2D) is blocked on DiaAnimation2D being implemented
- Candidate 3 (IK2D) requires a `GetWorldTransforms()` accessor added to `IKSolver`
- Candidates 10 and 11 depend on Candidate 1 and significant new infrastructure

**Supersession:** Candidate 9 (DiaVisualDebuggerConsole) supersedes any minimal HUD approach. Candidate 6 (TextPrimitive) is required for world-space labels even if Candidate 9 (ImGui) handles screen-space text — they serve different coordinate spaces.

**Deferred:** Candidate 10 (Entity Picking) is deferred to scene editor research. Seams reserved in Candidates 1 and 7. Candidate 11 (Rewind) is deferred to a future `DiaReplay` / simulation capture system. Seam: `DebugFrameData` must stay trivially copyable.

**Natural build order for active candidates:** 6 → 7 → 2 → 1 → 3 → 4 → 8 → 9 → 5 (when Animation2D ships)
