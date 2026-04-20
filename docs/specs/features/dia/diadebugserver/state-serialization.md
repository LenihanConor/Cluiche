# Feature Spec: State Serialization

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaDebugServer | @docs/specs/systems/dia/diadebugserver.md |
| Feature | **State Serialization** | (this document) |

## Problem Statement

Serializes ProcessingUnit, Phase, and Module state to JSON for sending to editors.

## Acceptance Criteria

- [ ] Serialize ProcessingUnit state (current phase, active modules, FPS)
- [ ] Serialize Phase state (name, state enum)
- [ ] Serialize Module state (name, config)
- [ ] JSON output format matches protocol
- [ ] Handle null/empty states gracefully
- [ ] Include timestamp in serialized output

## Design

**StateSerializer:**
```cpp
namespace Dia::DebugServer {
    class StateSerializer {
    public:
        static Json::Value SerializeProcessingUnitState(const ProcessingUnit* pu);
        static Json::Value SerializePhaseState(const Phase* phase);
        static Json::Value SerializeModuleState(const Module* module);
        static Json::Value SerializeCoreMetrics(const CoreMetrics& metrics);
        static Json::Value SerializePhaseTransition(const StringCRC& fromPhase, 
                                                    const StringCRC& toPhase,
                                                    uint64_t timestamp);
    };
}
```

**Example Output:**
```json
{
  "pu_id": "MainProcessingUnit",
  "current_phase": "UpdatePhase",
  "active_modules": ["RenderModule", "PhysicsModule"],
  "fps": 60.0,
  "timestamp": 1234567890
}
```

## Implementation Files

- `Dia/DiaDebugServer/StateSerializer.h` - Serialization interface
- `Dia/DiaDebugServer/StateSerializer.cpp` - JSON serialization implementation

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| DiaDebugServer | DDS-010 | JSON serialization | ✅ **Compliant** - Uses Json::Value |

**All binding decisions: COMPLIANT ✅**

## Open Questions

**Resolved:**
- **Decision 4:** No serialization caching for Phase 5 - serialize on-demand. Add caching in Phase 6+ if profiling shows bottleneck.
- **Decision 5:** Hybrid depth - ProcessingUnit → Phase (full), Module (summary by default, full on explicit get_module_details request).
- **Decision 6:** Summary only for module config (type, instance_id, enabled state). Full config requires separate get_module_details command.

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Performance | Should serialization happen on every request or cached? | Cache with dirty flag; invalidate on state change | ✅ No caching for Phase 5 (Decision 4) |
| 2 | Depth | How deep should nested objects be serialized? | One level deep; references by ID beyond that | ✅ Hybrid: PU→Phase full, Module summary (Decision 5) |
| 3 | Module Config | Full config or summary? | Summary (key fields only) for list view; full on demand | ✅ Summary only (Decision 6) |

## Status

`Done`
