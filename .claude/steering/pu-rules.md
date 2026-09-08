---
include: conditional
---

# Processing Unit Rules

## The Three PUs

CluicheTest runs three processing units. Each has a narrow, well-defined role.

| PU | Thread | Role |
|---|---|---|
| **MainPU** | Main (no dedicated thread) | Window lifecycle and kernel services only |
| **SimPU** | Dedicated (30 Hz) | All game/test logic — the workhorse |
| **RenderPU** | Dedicated (30 Hz) | Render-required work only |

---

## MainPU — Kernel Only

MainPU is the thinnest PU. It exists to own the window and expose kernel services to the other PUs.

**Allowed:**
- Window creation, event pump, OS lifecycle
- Kernel service providers: `KernelCanvas`, `KernelTextureHandler`, `KernelMeshHandler`
- Shutdown sequencing

**Not allowed:**
- Game logic of any kind
- UI rendering
- Simulation state
- Phase transition decisions

**Guidance:** If you are adding something to MainPU that is not a kernel service, it belongs on SimPU instead.

---

## SimPU — The Workhorse

SimPU owns all logic, state, and decisions. It is the authoritative source for what the application is doing and what RenderPU should draw.

**Allowed:**
- Game logic, test logic, state machines, AI, physics
- Phase transition decisions — `TransitionTo()` is called here, not from RenderPU
- Asset management, loading, registry updates
- Preparing render data and writing it to FrameStreams (`SimToRender`, `SimToRender3D`)
- Providing services to RenderPU via ServiceStream (e.g. `DebugLayerManager`)
- Reading the `RenderFence` from RenderPU for frame pacing

**Not allowed:**
- ImGui calls (no `NewFrame`, `EndFrame`, or `Begin`/`End` window calls)
- Direct draw calls

**Guidance:** If RenderPU needs to trigger an application decision (e.g. navigate to a stage), that intent is written into a stream and SimPU executes it. RenderPU never calls `TransitionTo()` or `ExecuteCommandJson` for navigation.

---

## RenderPU — Render-Required Work Only

RenderPU exists only for work that must happen on the render thread. If something can run on SimPU, it should.

**Allowed:**
- ImGui frame lifecycle (`NewFrame`, `EndFrame`, `Render`)
- ImGui UI panels that display data — consuming FrameStream or ServiceStream data prepared by SimPU
- Draw calls, canvas operations
- Writing the `RenderFence` back to SimPU (frame pacing only)

**Not allowed:**
- Game or test logic
- Application flow decisions — no `TransitionTo()`, no `ExecuteCommandJson` for navigation
- Writing mutable state that SimPU also reads (outside of the fence stream)
- Phase transitions of any kind

**Guidance:** RenderPU is a consumer. It reads streams prepared by SimPU and draws them. It has one write channel back: the `RenderFence`.

---

## Cross-PU Data Flow

```
MainPU ──(KernelCanvas, KernelTextureHandler, KernelMeshHandler)──► SimPU & RenderPU
                                                                          │
SimPU ──(SimToRender, SimToRender3D)──────────────────────────────────► RenderPU
SimPU ──(DebugLayerManager ServiceStream)─────────────────────────────► RenderPU
                                                                          │
RenderPU ──(RenderFence)──────────────────────────────────────────────► SimPU
RenderPU ──(NavigationRequest stream — to be added)───────────────────► SimPU
```

All cross-PU communication goes through FrameStreams or ServiceStreams. No direct cross-thread calls, no shared mutable singletons, no `ExecuteCommandJson` for navigation from RenderPU.

---

## Module Affinity Declaration

Every module must declare `kAllowedPUs` matching its intended PU. The engine checks this at startup and will assert on mis-placement.

```cpp
// Render-only module
static constexpr Dia::Application::ProcessingUnitAffinity kAllowedPUs = kRender;

// Sim-only module  
static constexpr Dia::Application::ProcessingUnitAffinity kAllowedPUs = kSim;

// Main-only module
static constexpr Dia::Application::ProcessingUnitAffinity kAllowedPUs = kMain;
```

A module that declares `kRender` must not issue application flow decisions. A module that declares `kSim` must not make ImGui calls.

---

## Red Flags

These patterns are always wrong:

| Pattern | Problem | Fix |
|---|---|---|
| `ExecuteCommandJson("navigate_to")` in a RenderPU module | Navigation decision on render thread | Write a typed request into a `RenderToSimRequest` stream; SimPU reads and acts |
| `TransitionTo()` called from RenderPU | Same as above | Same fix |
| ImGui `NewFrame`/`Begin` in a SimPU module | ImGui is render-thread only | Move to a RenderPU display module that reads a FrameStream |
| Shared mutable singleton read/written across PUs without a stream | Data race | Use FrameStream (snapshot) or ServiceStream (live, mutex-protected) |
| Game logic in MainPU | MainPU is kernel-only | Move to SimPU |
