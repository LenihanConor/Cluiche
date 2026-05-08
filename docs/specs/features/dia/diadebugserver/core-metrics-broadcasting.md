# Feature Spec: Core Metrics Broadcasting

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **Core Metrics Broadcasting** | (this document) |

## Problem Statement

Broadcasts core performance metrics (FPS, frame time, memory usage) to all connected editors every 500ms without requiring subscription.

## Acceptance Criteria

- [ ] Broadcast FPS, frame time, memory usage every 500ms
- [ ] No subscription required (always sent to all connections)
- [ ] JSON message format with timestamp
- [ ] Configurable broadcast interval (default 500ms)
- [ ] Minimal performance impact (<0.1ms per broadcast)
- [ ] Skip broadcast if no editors connected (optimization)
- [ ] Gather metrics from ProcessingUnit (FPS, frame time)
- [ ] Gather system memory metrics (used/available MB)

## Design

### Core Metrics Data

**Struct:**
```cpp
struct CoreMetrics {
    float fps;
    float frameTimeMs;
    float memoryUsedMb;
    float memoryAvailableMb;
};
```

### Broadcast Timer

**DebugServerModule Members:**
```cpp
class DebugServerModule {
private:
    float mMetricsBroadcastInterval;  // Default: 0.5f (500ms)
    float mMetricsTimer;              // Accumulator
};
```

### DoUpdate (Broadcasting)

**DebugServerModule::DoUpdate:**
```cpp
void DebugServerModule::DoUpdate() {
    if (!mServer || !mServer->IsRunning()) return;
    
    mServer->Update();
    
    // Broadcast core metrics
    mMetricsTimer += GetDeltaTime();
    if (mMetricsTimer >= mMetricsBroadcastInterval) {
        BroadcastCoreMetrics();
        mMetricsTimer = 0.0f;
    }
}
```

### BroadcastCoreMetrics

**Implementation:**
```cpp
void DebugServerModule::BroadcastCoreMetrics() {
    // Gather metrics
    CoreMetrics metrics;
    metrics.fps = GetProcessingUnit()->GetFPS();
    metrics.frameTimeMs = GetProcessingUnit()->GetFrameTime() * 1000.0f;
    metrics.memoryUsedMb = GetMemoryUsed() / (1024.0f * 1024.0f);
    metrics.memoryAvailableMb = GetMemoryAvailable() / (1024.0f * 1024.0f);
    
    // Serialize to JSON
    Json::Value json;
    json["type"] = "core_metrics";
    json["timestamp"] = GetTimestamp();
    json["payload"]["fps"] = metrics.fps;
    json["payload"]["frame_time_ms"] = metrics.frameTimeMs;
    json["payload"]["memory_used_mb"] = metrics.memoryUsedMb;
    json["payload"]["memory_available_mb"] = metrics.memoryAvailableMb;
    
    std::string jsonStr = Json::FastWriter().write(json);
    
    // Broadcast to all editors
    mServer->BroadcastText(jsonStr.c_str());
}
```

### JSON Format

**Example:**
```json
{
  "type": "core_metrics",
  "timestamp": 1234567890,
  "payload": {
    "fps": 60.0,
    "frame_time_ms": 16.67,
    "memory_used_mb": 512,
    "memory_available_mb": 2048
  }
}
```

## Implementation Files

- `Dia/DiaDebugServer/DebugServerModule.cpp` - BroadcastCoreMetrics

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-002 | Broadcast core metrics always | ✅ **Compliant** - No subscription needed |
| DiaDebugServer | DDS-006 | Core metrics broadcast every 500ms | ✅ **Compliant** - 500ms default |
| DiaDebugServer | DDS-010 | JSON serialization | ✅ **Compliant** - Uses Json::Value |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 1:** Broadcast interval configurable at startup only (via .diaapp config), not runtime. Default 500ms.
- **Decision 2:** Skip broadcast if `GetConnectionCount() == 0` (check before serialization).

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Performance | Impact of 500ms broadcasts? | Negligible - JSON serialization ~100μs | ✅ Configurable at startup (Decision 1) |
| 2 | No Connections | Should we skip broadcasts if no editors? | Yes - check connection count before serializing | ✅ Skip if zero connections (Decision 2) |

## Status

`Done`
