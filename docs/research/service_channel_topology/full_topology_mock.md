# Full Stream Topology Mock — Revised (Hybrid Model)

**Purpose:** Show the complete manifest when every static cross-PU coupling is resolved using the consolidated stream model. Validates that the per-PU-pair composite approach + ServiceStream holds at scale.

## Design Decisions (from research conversation)

1. **FrameStream:** Composite per PU-pair. One struct carries all per-frame data for that direction. New features extend the composite, never add streams.
2. **EventStream:** Per PU-pair, frame-batched. Events collected during producer tick, flushed at frame boundary. Consumer gets exactly "last frame's batch." Types are distinguished within the envelope.
3. **ServiceStream:** Collected-then-committed. All publishers register handles during DoStart, framework validates all declared services have handles, then commits — subscribers become live only after commit.
4. **No debug/non-debug split on FrameStream.** Strip at the producer in Release, not the transport.
5. **Unified suffix:** Everything is a `Stream`. Prefix tells you the frequency (Service = once, Frame = every tick, Event = batched discrete).

## Stream Model — Frequency Spectrum

| Primitive | Frequency | Topology | Timing | Commit Model |
|-----------|-----------|----------|--------|-------------|
| **ServiceStream** | Once (scope lifetime) | 1 per handle | Collected-then-committed at scope start | All providers must register → framework commits → consumers go live |
| **FrameStream** | Every tick | 1 per PU-pair direction | Latest-wins per frame | Write at end of producer tick, read at start of consumer tick |
| **EventStream** | Batched discrete | 1 per PU-pair direction | Flushed at producer frame boundary | Consumer gets exactly previous frame's batch |

## Consolidated Stream Inventory

| Stream ID | Kind | Payload Type | From | To | Shape | Notes |
|-----------|------|-------------|------|-----|-------|-------|
| MainToSim | EventStream | MainToSimEvent | MainPU | SimPU | existing (consolidated) | Input events + stage load commands |
| SimToMain | EventStream | SimToMainEvent | SimPU | MainPU | existing (consolidated) | UI commands |
| SimToRender | FrameStream | SimToRenderFrame | SimPU | RenderPU | existing (consolidated) | FrameData + DebugDrawList (Shape B folded in) |
| MainToSim | FrameStream | MainToSimFrame | MainPU | SimPU | existing (consolidated) | UIDataBuffer |
| MainToRender | FrameStream | MainToRenderFrame | MainPU | RenderPU | **C** (new) | StageHUD + AutomationStatus (Shape C composited) |
| MainToRender | EventStream | MainToRenderEvent | MainPU | RenderPU | **C** (new) | StageLifecycle events |
| Canvas | ServiceStream | ICanvas | MainPU | — | **A** | Stable handle |
| TextureHandler | ServiceStream | ITextureHandler | MainPU | — | **A** | Stable handle |

**Total: 8 streams** (vs 11 in fine-grained model)

### Growth Rule

> New cross-PU data? **Extend the existing composite for that PU-pair direction.** Only add a new stream if it's a new PU-pair direction or a new ServiceStream handle.

Ceiling for 5 PUs: ~20 directional pairs × 2 (frame + event) + ~10 ServiceStreams = **~50 max, hard bounded.**

---

## Composite Payload Types

### SimToRenderFrame (FrameStream, SimPU → RenderPU)

```cpp
// Existing FrameData already does this via inheritance:
class SimToRenderFrame : public DebugFrameData,   // debug geometry (Shape B folded in)
                         public UIFrameData,       // UI render data
                         public EntityFrameData {  // entity positions/states
    // Future: add PhysicsDebugData, AnimationDebugData, etc.
};
// Note: this IS the existing FrameData. No rename needed. Just folding
// DebugDrawList (Shape B) into DebugFrameData instead of a separate stream.
```

### MainToRenderFrame (FrameStream, MainPU → RenderPU) — NEW

```cpp
struct MainToRenderFrame {
    StageHUDState       stageHUD;         // Shape C: current HUD overlay state
    AutomationStatus    automationStatus; // Shape C: heartbeat flag
    // Future: editor overlay state, profiler overlay, etc.
};
```

### MainToSimEvent (EventStream, MainPU → SimPU) — CONSOLIDATION

```cpp
struct MainToSimEvent {
    enum class Kind : uint8_t { kInput, kStageLoad };
    Kind kind;
    union {
        InputEvent      input;
        StageLoadEvent  stageLoad;
    };
};
// Or: type-erased envelope with StringCRC tag (existing Event<T> pattern)
```

### SimToMainEvent (EventStream, SimPU → MainPU) — CONSOLIDATION

```cpp
struct SimToMainEvent {
    enum class Kind : uint8_t { kUICommand };
    Kind kind;
    union {
        UICommand uiCommand;
    };
};
```

### MainToRenderEvent (EventStream, MainPU → RenderPU) — NEW

```cpp
struct MainToRenderEvent {
    enum class Kind : uint8_t { kStageLifecycle };
    Kind kind;
    union {
        StageLifecycleEvent stageLifecycle;
    };
};
```

---

## Mock: `cluiche_main.diaapp` (Global)

```json
{
  "version": 2,
  "stages": ["Boot", "DummyStage", "RigidBody2DStage"],
  "boot_stage": "Boot",

  "streams": [
    {
      "id": "MainToSim",
      "kind": "EventStream",
      "payload_type": "MainToSimEvent",
      "from": "MainPU",
      "to": "SimPU"
    },
    {
      "id": "SimToMain",
      "kind": "EventStream",
      "payload_type": "SimToMainEvent",
      "from": "SimPU",
      "to": "MainPU"
    },
    {
      "id": "SimToRender",
      "kind": "FrameStream",
      "payload_type": "SimToRenderFrame",
      "from": "SimPU",
      "to": "RenderPU",
      "multi_writer": true
    },
    {
      "id": "MainToSimFrame",
      "kind": "FrameStream",
      "payload_type": "MainToSimFrame",
      "from": "MainPU",
      "to": "SimPU"
    },
    {
      "id": "MainToRender",
      "kind": "FrameStream",
      "payload_type": "MainToRenderFrame",
      "from": "MainPU",
      "to": "RenderPU"
    },
    {
      "id": "MainToRenderEvents",
      "kind": "EventStream",
      "payload_type": "MainToRenderEvent",
      "from": "MainPU",
      "to": "RenderPU"
    },
    {
      "id": "Canvas",
      "kind": "ServiceStream",
      "payload_type": "ICanvas",
      "from": "MainPU"
    },
    {
      "id": "TextureHandler",
      "kind": "ServiceStream",
      "payload_type": "ITextureHandler",
      "from": "MainPU"
    }
  ],

  "processing_units": [
    {
      "instance_id": "MainPU",
      "type": "MainProcessingUnit",
      "frequency_hz": 30,
      "modules": [
        {
          "instance_id": "ObservationModule",
          "type": "ObservationModule",
          "channels": []
        },
        {
          "instance_id": "ProfilerModule",
          "type": "ProfilerModule",
          "channels": []
        },
        {
          "instance_id": "JobSystemModule",
          "type": "JobSystemModule",
          "channels": []
        },
        {
          "instance_id": "KernelModule",
          "type": "KernelModule",
          "channels": [
            { "id": "MainToSim",       "role": "writes" },
            { "id": "Canvas",          "role": "provides" },
            { "id": "TextureHandler",  "role": "provides" }
          ]
        },
        {
          "instance_id": "UIModule",
          "type": "UIModule",
          "channels": [
            { "id": "SimToMain",       "role": "reads" },
            { "id": "MainToSimFrame",  "role": "writes" }
          ]
        },
        {
          "instance_id": "AssetServiceModule",
          "type": "AssetServiceModule",
          "channels": [
            { "id": "TextureHandler",  "role": "consumes" },
            { "id": "MainToSim",       "role": "writes" }
          ]
        },
        {
          "instance_id": "DebugServerHostModule",
          "type": "DebugServerHostModule",
          "channels": []
        },
        {
          "instance_id": "AutomationModule",
          "type": "AutomationModule",
          "channels": [
            { "id": "MainToRender",       "role": "writes" },
            { "id": "MainToRenderEvents", "role": "writes" }
          ]
        }
      ]
    },
    {
      "instance_id": "SimPU",
      "type": "SimProcessingUnit",
      "frequency_hz": 30,
      "dedicated_thread": true,
      "modules": [
        {
          "instance_id": "TimeServerModule",
          "type": "TimeServerModule",
          "channels": []
        },
        {
          "instance_id": "InputStreamModule",
          "type": "InputStreamModule",
          "channels": [
            { "id": "MainToSim", "role": "reads" }
          ]
        },
        {
          "instance_id": "LoadingScreenModule",
          "type": "LoadingScreenModule",
          "channels": [
            { "id": "MainToSimFrame", "role": "reads" },
            { "id": "SimToRender",    "role": "writes" }
          ]
        }
      ]
    },
    {
      "instance_id": "RenderPU",
      "type": "RenderProcessingUnit",
      "frequency_hz": 60,
      "dedicated_thread": true,
      "modules": [
        {
          "instance_id": "DebugUI",
          "type": "BgfxImguiDebugUIModule",
          "channels": []
        },
        {
          "instance_id": "BootMenu",
          "type": "ImguiBootMenuModule",
          "channels": []
        },
        {
          "instance_id": "TestStageHUDModule",
          "type": "TestStageHUDModule",
          "channels": [
            { "id": "MainToRenderEvents", "role": "reads" },
            { "id": "MainToRender",       "role": "reads" }
          ]
        },
        {
          "instance_id": "VisualDebuggerConsoleModule",
          "type": "VisualDebuggerConsoleModule",
          "channels": [
            { "id": "SimToRender", "role": "reads" }
          ]
        },
        {
          "instance_id": "RenderModule",
          "type": "RenderModule",
          "channels": [
            { "id": "SimToRender",     "role": "reads" },
            { "id": "Canvas",          "role": "consumes" },
            { "id": "TextureHandler",  "role": "consumes" }
          ]
        }
      ]
    }
  ]
}
```

---

## Mock: `dummy_stage.diaapp`

```json
{
  "version": 2,
  "processing_units": [
    {
      "instance_id": "MainPU",
      "modules": [
        {
          "instance_id": "DummyUIPageModule",
          "type": "DummyUIPageModule",
          "channels": []
        }
      ]
    },
    {
      "instance_id": "SimPU",
      "modules": [
        {
          "instance_id": "DummyLevelModule",
          "type": "DummyLevelModule",
          "channels": [
            { "id": "MainToSimFrame",  "role": "reads" },
            { "id": "SimToRender",     "role": "writes" },
            { "id": "SimToMain",       "role": "writes" },
            { "id": "TextureHandler",  "role": "consumes" }
          ]
        },
        {
          "instance_id": "EntityModule",
          "type": "EntityModule",
          "channels": []
        }
      ]
    }
  ]
}
```

---

## Mock: `rigidbody2d_stage.diaapp`

```json
{
  "version": 2,
  "processing_units": [
    {
      "instance_id": "MainPU",
      "modules": [
        {
          "instance_id": "Physics2DModule",
          "type": "Physics2DModule",
          "channels": []
        },
        {
          "instance_id": "RigidBody2DTestModule",
          "type": "RigidBody2DTestModule",
          "channels": []
        }
      ]
    },
    {
      "instance_id": "SimPU",
      "modules": [
        {
          "instance_id": "VisualDebuggerModule",
          "type": "VisualDebuggerModule",
          "channels": [
            { "id": "SimToRender", "role": "writes" }
          ]
        }
      ]
    }
  ]
}
```

---

## Role Vocabulary

| Role | Used with | Meaning |
|------|-----------|---------|
| `reads` | FrameStream, EventStream | Consumes data/events from this stream |
| `writes` | FrameStream, EventStream | Produces data/events to this stream |
| `provides` | ServiceStream | Publishes a handle (exactly one per ServiceStream) |
| `consumes` | ServiceStream | Uses the provided handle after commit |

---

## Comparison: Before vs After

| Metric | Fine-grained (original mock) | Hybrid (this mock) |
|--------|-------|------|
| Total streams | 11 | **8** |
| FrameStreams | 5 | **3** (one per PU-pair direction) |
| EventStreams | 4 | **3** (one per PU-pair direction) |
| ServiceStreams | 2 | **2** (unchanged) |
| Max channels per module | 4 | **4** (unchanged) |
| Growth when adding a new per-frame feature | +1 stream | **+1 field in composite** |
| Growth when adding a new event type | +1 stream | **+1 variant in envelope** |
| Growth when adding a new service handle | +1 stream | **+1 stream** (correct — new handle = new stream) |

---

## ServiceStream Commit Lifecycle

```
Scope start (App start or Stage load):
  ┌─────────────────────────────────────────────────────────┐
  │ 1. All modules DoStart()                                │
  │    - Providers call ServiceStreamWriter::Register(handle)│
  │    - Handles stored but NOT yet accessible              │
  ├─────────────────────────────────────────────────────────┤
  │ 2. Framework COMMIT gate                                │
  │    - Validates: every declared ServiceStream has a      │
  │      registered handle                                  │
  │    - If any missing → Start() fails                     │
  │    - If all present → flip committed flag               │
  ├─────────────────────────────────────────────────────────┤
  │ 3. Consumers go live                                    │
  │    - ServiceStreamReader::Get() now returns the handle  │
  │    - Before commit: Get() would assert                  │
  └─────────────────────────────────────────────────────────┘

Scope end (Stage unload):
  - Stage-scoped ServiceStreams reset (committed flag cleared)
  - Global ServiceStreams unaffected
```

---

## EventStream Frame-Batch Lifecycle

```
Producer frame (e.g. MainPU tick N):
  ┌─────────────────────────────────────────────────────────┐
  │ Modules call Send() throughout the tick                 │
  │ Events accumulate in ring buffer                        │
  ├─────────────────────────────────────────────────────────┤
  │ End of tick: framework calls Flush()                    │
  │ Marks batch boundary (tick N complete)                  │
  └─────────────────────────────────────────────────────────┘

Consumer frame (e.g. SimPU tick M):
  ┌─────────────────────────────────────────────────────────┐
  │ Consume() returns events up to last flush only          │
  │ = exactly "what happened during producer's last frame"  │
  │ Deterministic: replay flush boundaries → same behavior  │
  └─────────────────────────────────────────────────────────┘
```

---

## Conclusion

The hybrid model (composite per PU-pair + ServiceStream) gives hard growth bounds. The manifest stays at 8 streams for this project regardless of how many features we add. New data extends composites; only genuinely new PU-pair directions or infrastructure handles create new streams. The topology is stable.
