# Feature Spec: Live Data Visualization

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEditor | @docs/specs/systems/dia/diaeditor.md |
| Feature | **Live Data Visualization** | (this document) |

## Problem Statement

Developers need real-time visibility into running game performance — FPS per ProcessingUnit and memory consumption — directly in the Game Connection panel. Currently `DebugServerModule::BroadcastCoreMetrics()` sends FPS and frameTimeMs as 0.0f because ProcessingUnit doesn't expose frame timing and there's no metrics collection system. A lightweight metrics display in the existing Game Connection panel gives a quick "is the game healthy?" glance without needing a dedicated performance editor.

## Solution Overview

Two layers:

**Game side (DiaApplication):** A `MetricsCollectorModule` that lives in the main ProcessingUnit. Sub-PUs receive a pointer to it via `ProcessingUnit::SetMetricsCollector()` and call `ReportFrame()` at the end of each update tick. The module also queries process memory. `DebugServerModule` reads the latest snapshot and broadcasts it in the existing `core_metrics` message.

**Editor side:** The Game Connection panel UI (already exists) gains a summary bar, an FPS line chart (one line per PU), and a memory line chart. Data arrives via the existing `GameConnectionManager` raw message channel. No new panels, plugins, or C++ visualization framework needed.

---

## Acceptance Criteria

- [ ] `MetricsCollectorModule` collects per-PU FPS and frame time via injected `ReportFrame()` calls
- [ ] `MetricsCollectorModule` queries process memory usage (Windows API)
- [ ] `MetricsCollectorModule` tracks uptime from first update
- [ ] `ProcessingUnit::SetMetricsCollector()` / `GetMetricsCollector()` for injection
- [ ] Each PU calls `ReportFrame(puId, puName, deltaTimeMs)` at end of its update tick
- [ ] `CoreMetricsPayload` extended with per-PU metrics array
- [ ] `DebugServerModule::BroadcastCoreMetrics()` reads from MetricsCollectorModule instead of hardcoded zeros
- [ ] Game Connection panel shows summary bar: FPS (main PU), Memory (MB), Uptime
- [ ] Game Connection panel shows FPS line chart with one line per PU, ~60 sample window
- [ ] Game Connection panel shows Memory line chart, single line, ~60 sample window
- [ ] Charts render from `core_metrics` messages received via GameConnectionManager

---

## Design

### MetricsCollectorModule (Game Side)

```cpp
// Dia/DiaApplication/Metrics/MetricsCollectorModule.h
namespace Dia::Application
{
    struct PUMetrics
    {
        Dia::Core::StringCRC id;
        char name[64];
        float fps;
        float frameTimeMs;
    };

    struct MetricsSnapshot
    {
        static const unsigned int kMaxProcessingUnits = 8;
        PUMetrics puMetrics[kMaxProcessingUnits];
        unsigned int puCount;
        float memoryUsedMB;
        float uptimeSeconds;
    };

    class MetricsCollectorModule : public Module
    {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        MetricsCollectorModule(ProcessingUnit* pu);

        // Called by each PU at end of its update tick.
        void ReportFrame(const Dia::Core::StringCRC& puId,
                         const char* puName,
                         float deltaTimeMs);

        const MetricsSnapshot& GetSnapshot() const;

    protected:
        StateObject::OpertionResponse DoStart(const IStartData*) override;
        void DoUpdate() override;
        void DoStop() override;

    private:
        void QueryMemory();

        MetricsSnapshot mSnapshot;
        float mUptimeAccumulator;
    };
}
```

**DoUpdate:**
- Queries process memory via `GetProcessMemoryInfo` (Windows) / stub on other platforms
- Increments uptime
- The per-PU data is already populated by `ReportFrame()` calls earlier in the frame

**ReportFrame:**
- Finds existing entry by puId, or adds new one if space
- Name string comes from `puId.AsChar()` (StringCRC stores original string)
- FPS smoothed via exponential moving average: `fps = alpha * instantFps + (1 - alpha) * prevFps` where alpha ~0.1
- `frameTimeMs = deltaTimeMs` (raw, not smoothed)

### ProcessingUnit Injection

```cpp
// Added to Dia/DiaApplication/ApplicationProcessingUnit.h
void SetMetricsCollector(MetricsCollectorModule* collector);
MetricsCollectorModule* GetMetricsCollector() const;
```

The PU stores a raw pointer. At the end of its internal update tick, if the pointer is non-null, it calls `mMetricsCollector->ReportFrame(GetUniqueId(), GetName(), frameDeltaMs)`. This requires exposing frame delta — either via a new member or by computing it at the call site.

### Extended CoreMetricsPayload

```cpp
// Dia/DiaDebugProtocol/MessageStructs.h — extended
struct PUMetricsPayload
{
    char name[64];
    float fps;
    float frameTimeMs;
};

struct CoreMetricsPayload
{
    float fps;              // Main PU FPS (convenience)
    float frameTimeMs;      // Main PU frame time (convenience)
    float memoryUsedMb;
    float memoryAvailableMb;
    float uptimeSeconds;

    static const unsigned int kMaxProcessingUnits = 8;
    PUMetricsPayload puMetrics[kMaxProcessingUnits];
    unsigned int puCount;
};
```

### DebugServerModule Integration

`DebugServerModule::BroadcastCoreMetrics()` changes from hardcoded zeros to:

```cpp
MetricsCollectorModule* metrics = /* obtained via PU */;
if (metrics)
{
    const MetricsSnapshot& snap = metrics->GetSnapshot();
    payload.fps = snap.puCount > 0 ? snap.puMetrics[0].fps : 0.0f;
    payload.frameTimeMs = snap.puCount > 0 ? snap.puMetrics[0].frameTimeMs : 0.0f;
    payload.memoryUsedMb = snap.memoryUsedMB;
    payload.uptimeSeconds = snap.uptimeSeconds;
    payload.puCount = snap.puCount;
    for (unsigned int i = 0; i < snap.puCount; ++i)
    {
        strncpy_s(payload.puMetrics[i].name, 64, snap.puMetrics[i].name, _TRUNCATE);
        payload.puMetrics[i].fps = snap.puMetrics[i].fps;
        payload.puMetrics[i].frameTimeMs = snap.puMetrics[i].frameTimeMs;
    }
}
```

### Serialization (Wire Format)

```json
{
    "type": "core_metrics",
    "timestamp": 1234567890,
    "payload": {
        "fps": 60.0,
        "frame_time_ms": 16.6,
        "memory_used_mb": 128.5,
        "memory_available_mb": 7800.0,
        "uptime_seconds": 342.5,
        "processing_units": [
            { "name": "MainPU", "fps": 60.0, "frame_time_ms": 16.6 },
            { "name": "RenderPU", "fps": 60.0, "frame_time_ms": 14.2 },
            { "name": "SimPU", "fps": 30.0, "frame_time_ms": 33.1 }
        ]
    }
}
```

### Game Connection Panel UI (Editor Side)

The existing Game Connection panel gains a metrics section shown when connected. No new panel or plugin — this extends `GameConnectionEditorPlugin`'s UI.

**Summary Bar:**
| FPS (Main) | Memory | Uptime |
|---|---|---|
| 60 | 128.5 MB | 05:42 |

- FPS: main PU only (first entry in processing_units array)
- Memory: `memory_used_mb` formatted
- Uptime: `uptime_seconds` formatted as mm:ss

**FPS Chart:**
- Line chart, one line per ProcessingUnit, legend labels them by name
- ~60 sample ring buffer (client-side), appended on each `core_metrics` message
- Y axis: FPS, X axis: sample index (no timestamp labels needed)

**Memory Chart:**
- Single line chart, total `memory_used_mb`
- Same ~60 sample window
- Y axis: MB

Both charts use plain vanilla JS with HTML5 canvas — the Game Connection panel is a self-contained HTML file with no React/build pipeline. Keep it lightweight and consistent with the existing panel code.

---

## Implementation Files

**New files:**
- `Dia/DiaApplication/Metrics/MetricsCollectorModule.h`
- `Dia/DiaApplication/Metrics/MetricsCollectorModule.cpp`
- `Cluiche/Tests/GoogleTests/Application/TestMetricsCollector.cpp`

**Modified files:**
- `Dia/DiaApplication/ApplicationProcessingUnit.h` — `SetMetricsCollector` / `GetMetricsCollector`
- `Dia/DiaApplication/ApplicationProcessingUnit.cpp` — member init, ReportFrame call site
- `Dia/DiaDebugProtocol/MessageStructs.h` — extended `CoreMetricsPayload`
- `Dia/DiaDebugProtocol/Serialization.h` — serialize per-PU array
- `Dia/DiaDebugServer/DebugServerModule.cpp` — read from MetricsCollectorModule
- `Dia/DiaApplication/DiaApplication.vcxproj` + `.filters` — new Metrics files
- `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` — new test file
- Game Connection panel HTML/JS — summary bar + charts

---

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — PU IDs are StringCRC, module has kUniqueId |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — MetricsCollectorModule is a Module in the main PU |
| Platform | PD-004 | No STL in public APIs | **Compliant** — MetricsSnapshot uses fixed arrays, `const char*` |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — new files added to vcxproj |
| Dia | AD-002 | No STL in public APIs | **Compliant** — reinforces PD-004 |
| Dia | AD-003 | Namespace convention `Dia::<Module>::` | **Compliant** — `Dia::Application::` for metrics module |
| Dia | AD-004 | PU/Phase/Module for apps | **Compliant** — MetricsCollectorModule participates in PU lifecycle |
| DiaEditor | SED-004 | WebSocket protocol uses JSON | **Compliant** — metrics travel as JSON in core_metrics message |
| DiaEditor | SED-010 | Use DiaDebugProtocol for wire types | **Compliant** — extends existing CoreMetricsPayload |

**All binding decisions: COMPLIANT**

---

## Open Questions

**Resolved:**
- ~~Separate panel vs. Game Connection panel?~~ Game Connection panel — keeps it concise
- ~~PU tree hierarchy?~~ Deferred to a dedicated PU editor
- ~~Singleton vs. injection for MetricsCollector?~~ Injection via `SetMetricsCollector()`
- ~~IVisualization framework?~~ Not needed — direct HTML/JS in Game Connection panel
- ~~Frame time chart?~~ Dropped — FPS line per PU + summary bar frame time covers it

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Smoothing | Should FPS be smoothed or instantaneous? | Exponential moving average | EMA — smoothed for readability |
| 2 | Broadcast rate | Keep 500ms broadcast interval or change? | 500ms is fine for ~60 sample / 30s window | Keep 500ms |
| 3 | Memory | Process working set or private bytes? | Working set (matches current impl) | Working set — already implemented via GetProcessMemoryInfo |
| 4 | PU naming | Where does PU name string come from? | Need to add a name accessor to ProcessingUnit if missing | Use `GetUniqueId().AsChar()` — StringCRC stores the original string. No new accessor needed. |
| 5 | Chart library | Use recharts or plain canvas? | Depends on existing bundle — plain canvas if no React | Plain canvas/vanilla JS — Game Connection panel is plain HTML, no React pipeline |

---

## Status

`Done` - Implemented and tested
