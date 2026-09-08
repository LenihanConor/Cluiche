**Spec:** Backlog item #10 — Eliminate remaining cross-PU statics
**Target:** `VisualDebuggerModule::sLayerManager` / `GetStaticLayerManager()`
**Status:** Done

---

## Summary

Remove `VisualDebuggerModule::sLayerManager` static by:
- **Phase A:** SimPU consumers (`Geometry2DTestStageModule`) → `ModuleRef<VisualDebuggerModule>`
- **Phase B:** Cross-PU consumers (RenderPU: `AssetRuntimeVisualDebuggerModule`, `VisualDebuggerConsoleModule`; MainPU: `Physics2DModule`) → event-based registration via an EventStream + layer metadata in the existing `SimToRender` FrameStream.

This also fixes a latent **data race**: RenderPU/MainPU modules currently mutate a SimPU-owned `DebugLayerManager` from different threads with no synchronisation.

---

## Phase A — SimPU consumers to ModuleRef

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| A1 | `Geometry2DTestStageModule` → `ModuleRef<VisualDebuggerModule>` | Compiles, Geometry2D stage runs | Not Started | haiku | Replace `GetStaticLayerManager()` calls with `mVisualDebuggerRef->GetLayerManager()`. Same PU (SimPU). Add manifest dependency. |

---

## Phase B — Cross-PU consumers via streams

### Design

**Problem:** RenderPU modules (`AssetRuntimeVisualDebuggerModule`, `VisualDebuggerConsoleModule`) and MainPU module (`Physics2DModule`) call `Register()`/`Unregister()` on a SimPU-owned `DebugLayerManager` without synchronisation.

**Solution:**
1. Cross-PU modules send **registration requests** as events to SimPU via a new `DebugLayerRequest` EventStream (from both RenderPU and MainPU → SimPU).
2. `VisualDebuggerModule` (SimPU) processes these events, owns all registration, and is the single writer.
3. `VisualDebuggerModule` publishes a **layer metadata snapshot** (compact array of layer names + enabled state + stageTag) via the existing `SimToRender` FrameStream each frame.
4. `VisualDebuggerConsoleModule` (RenderPU) reads layer metadata from the FrameStream instead of calling `GetStaticLayerManager()`.

**`Physics2DModule` special case:** Physics should be on SimPU (currently misconfigured on MainPU in the RigidBody2D stage manifest). Move it to SimPU as part of this work — then it uses `ModuleRef` like `Geometry2DTestStageModule`. No stream needed for Physics.

### Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| B1 | Move `Physics2DModule` from MainPU to SimPU in `rigidbody2d_stage.diaapp` | Stage runs, circles simulate | Not Started | sonnet | Also add manifest dependency on `VisualDebuggerModule`. Change Physics2DModule code to use `ModuleRef<VisualDebuggerModule>` for layer registration. May need `VisualDebuggerModule` added to `rigidbody2d_stage.diaapp` SimPU or confirm it's already inherited from global manifest. |
| B2 | Define `DebugLayerRequest` event type | Compiles | Not Started | haiku | New header `CluicheGameBaseline/Types/DebugLayerRequest.h`. Fields: `enum Action { kRegister, kUnregister }`, `StringCRC layerName`, `StringCRC stageTag`, `int priority`, `IVisualDebugger* drawer` (pointer is safe — lifecycle owned by the registering module which outlives the event). |
| B3 | Declare `DebugLayerRequest` EventStream in `cluiche_main.diaapp` | Manifest validates | Not Started | haiku | `{ "id": "DebugLayerCommand", "kind": "EventStream", "payload_type": "DebugLayerRequest", "overflow": "drop-oldest", "from": "RenderPU", "to": "SimPU" }`. Note: also need from MainPU if Physics stays there — but since B1 moves Physics to SimPU, only RenderPU→SimPU needed. |
| B4 | `VisualDebuggerModule` processes `DebugLayerCommand` events | Unit test: register via event, layer appears in manager | Not Started | sonnet | Add `EventStreamReader<DebugLayerRequest>` to `VisualDebuggerModule`. In `DoUpdate`, drain events and call `mLayerManager.Register()`/`Unregister()` accordingly. |
| B5 | Define `DebugLayerMetadata` snapshot type | Compiles | Not Started | haiku | Compact struct: `struct DebugLayerMeta { StringCRC name; StringCRC stageTag; bool enabled; int priority; }`. Array of max 16 entries in a `DebugLayerSnapshot` struct. |
| B6 | Publish `DebugLayerSnapshot` in `SimToRender` FrameStream | Compiles | Not Started | sonnet | `VisualDebuggerModule` already writes to `SimToRender` (FrameData). Embed `DebugLayerSnapshot` in the frame data it publishes, or publish a separate FrameStream. Simplest: add a `DebugLayerSnapshot` field to `DebugFrameData` if it exists, or to the `FrameData` struct the module already writes. |
| B7 | Migrate `AssetRuntimeVisualDebuggerModule` (RenderPU) | Asset debug layer registers on stage entry | Not Started | sonnet | Replace `GetStaticLayerManager()->Register()` with sending `DebugLayerRequest{kRegister, ...}` via `EventStreamWriter<DebugLayerRequest>`. Replace `Unregister()` in DoStop with `kUnregister` event. Add channel to manifest. |
| B8 | Migrate `VisualDebuggerConsoleModule` (RenderPU) | Console renders layer tabs | Not Started | sonnet | Replace `GetStaticLayerManager()` read with `DebugLayerSnapshot` from `SimToRender` FrameStream. The `Console.Render()` call needs the layer list — pass the snapshot instead of a `DebugLayerManager*`. May need a small API change in `DiaVisualDebuggerConsole`. |
| B9 | Delete `sLayerManager` / `GetStaticLayerManager()` from `VisualDebuggerModule` | Compiles, no callers remain | Not Started | haiku | Remove static member, accessor, and set/clear in DoStart. Keep `mLayerManager` instance member (still used internally). |
| B10 | Build + run full test suite | All tests pass, all stages render debug layers | Not Started | sonnet | `dia run googletest` + `dia run cluichetest` (manual visual check of debug overlays in Geometry2D + RigidBody2D + AssetRuntime stages). |

---

## Key Decisions

- **Physics2DModule moves to SimPU** — it's a simulation module, not a main-thread module. Current MainPU placement is a historical artifact. Once on SimPU it can use ModuleRef directly, no stream needed.
- **EventStream for registration** (not direct ModuleRef) because RenderPU modules need to register/unregister debug drawers, and they can't hold a ModuleRef to a SimPU module.
- **Layer metadata in FrameStream** (not ServiceStream) because the layer list can change per-frame (layers toggled, registered/unregistered on stage transitions). FrameStream is the correct primitive for per-frame mutable state.
- **Single EventStream direction: Render→Sim** because after moving Physics to SimPU, only RenderPU modules need cross-PU registration. If a future MainPU module needs it, add a second EventStream or make this one accept from multiple sources.

---

## Risks

- **`DiaVisualDebuggerConsole::Render()` API change (B8):** Currently takes `DebugLayerManager&`. Needs to accept `DebugLayerSnapshot` or a read-only view. This touches Dia library code — flag the API change.
- **Pointer in event payload (B2):** `IVisualDebugger* drawer` pointer travels from RenderPU to SimPU. Safe because the owning module's lifecycle outlives the frame tick that processes the event — but must be documented as a contract.
- **Physics2DModule PU move (B1):** Moving Physics to SimPU changes its tick threading. Verify that `DiaRigidBody2D` world step is thread-safe when called from SimPU's dedicated thread (it should be — physics simulation is inherently single-threaded per world).
