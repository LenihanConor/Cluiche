# Feature Spec: Metric Assertions

## Parent System
@docs/specs/systems/dia/diaautomation.md

## Builds On
@docs/specs/features/dia/diacli/dia-orchestrate.md

## Research
@docs/research/e2e_testing/design-decisions.md (Section 15)

## Traceability

| Level | Spec | Decision IDs honored |
|-------|------|---------------------|
| Platform | @docs/specs/platform/Cluiche.md | PD-001, PD-004, PD-007 |
| Application | @docs/specs/applications/dia.md | AD-002, AD-003 |
| System (DiaAutomation) | @docs/specs/systems/dia/diaautomation.md | SD-AUT-005, SD-AUT-007 |
| System (DiaMetrics) | @docs/specs/systems/dia/diametrics.md | (MetricRegistry API) |

## Problem Statement

Pytest scenarios need to assert on live DiaMetrics values (frame duration, physics step count, queue depth, etc.) without writing C++ checkpoints for simple numeric checks. Today there's no remote path to query a metric value — DiaMetrics is C++-only with no command surface. Scenarios that want "frame time < 33ms" or "entity count == 5" must register a C++ checkpoint, even though the data is already in the MetricRegistry.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC1 | `dia.automation.get_metric` command registered with DiaAPI (JSON path). Params: `{"name": "<metric_name>"}`. Returns `{"value": <number>, "type": "<counter\|gauge\|histogram>"}`. | Unit test |
| AC2 | Returns `{"success": false, "error": "metric not found: '<name>'"}` for unknown metrics. | Unit test |
| AC3 | Queries live MetricRegistry value at call time (not cached/stale). | Unit test |
| AC4 | For histogram metrics, returns `{"value": {"count": N, "sum": F, "min": F, "max": F, "mean": F}}`. | Unit test |
| AC5 | pytest fixture `assert_metric(name, op, threshold)` available in scenarios. Operators: `<`, `>`, `<=`, `>=`, `==`, `!=`. | Unit test (mocked client) |
| AC6 | `assert_metric` raises `pytest.fail` with a descriptive message on failure: `"Metric 'dia.frame.duration_ms' = 45.2, expected < 33"`. | Unit test |
| AC7 | `assert_metric` sends `dia.automation.get_metric` via the `dia_client` fixture. | Code review |
| AC8 | Any `dia.automation.*` command (including `get_metric`) resets the heartbeat timer (SD-AUT-007). | Existing behaviour — no new work |
| AC9 | Existing tests pass, no behavioural change to CluicheTest. | Build + test verification |

## Design

### C++ Side: Command Registration

Added to `AutomationService::RegisterCommands()`:

```cpp
Dia::API::CommandInfoJson getMetricCmd;
getMetricCmd.name = Dia::Core::StringCRC("dia.automation.get_metric");
getMetricCmd.description = "Query a live metric value by name";
getMetricCmd.category = Dia::Core::StringCRC("dia.automation");
getMetricCmd.owner = "DiaAutomation";
getMetricCmd.callback = [](const Json::Value& params) -> Json::Value {
    const char* name = params["name"].asCString();
    Dia::Core::StringCRC metricId(name);

    auto* registry = Dia::Metrics::MetricRegistry::GetInstance();
    if (!registry->Has(metricId))
    {
        // Framework wraps in {success: false, error: ...}
        throw std::runtime_error(std::string("metric not found: '") + name + "'");
    }

    Json::Value result;
    auto type = registry->GetType(metricId);
    result["type"] = MetricTypeToString(type);

    if (type == Dia::Metrics::MetricType::kHistogram)
    {
        auto snapshot = registry->GetHistogramSnapshot(metricId);
        Json::Value value;
        value["count"] = snapshot.count;
        value["sum"] = snapshot.sum;
        value["min"] = snapshot.min;
        value["max"] = snapshot.max;
        value["mean"] = snapshot.mean;
        result["value"] = value;
    }
    else
    {
        result["value"] = registry->GetValue(metricId);
    }

    return result;
};
Dia::API::RegisterCommandJson(getMetricCmd);
```

### Python Side: Fixture

Added to `Tools/orchestrator/plugin.py`:

```python
@pytest.fixture
def assert_metric(dia_client):
    """Fixture that returns an assertion helper for live metric values."""
    def _assert(name: str, op: str, threshold: float):
        result = dia_client.send_command("dia.automation.get_metric", {"name": name})
        value = result["value"]

        # For histograms, use mean by default
        if isinstance(value, dict):
            value = value["mean"]

        ops = {
            "<": lambda a, b: a < b,
            ">": lambda a, b: a > b,
            "<=": lambda a, b: a <= b,
            ">=": lambda a, b: a >= b,
            "==": lambda a, b: a == b,
            "!=": lambda a, b: a != b,
        }
        if op not in ops:
            pytest.fail(f"Unknown operator: '{op}'")
        if not ops[op](value, threshold):
            pytest.fail(f"Metric '{name}' = {value}, expected {op} {threshold}")

    return _assert
```

### Scenario Usage

```python
def test_frame_time(dia_client, assert_metric):
    dia_client.navigate_to("DummyStage")
    import time; time.sleep(2.0)  # let it stabilize
    assert_metric("dia.frame.duration_ms", "<", 50)  # generous for Debug

def test_entity_count(dia_client, assert_metric):
    dia_client.navigate_to("EntityTestStage")
    import time; time.sleep(1.0)
    assert_metric("cluichetest.entity.count", "==", 10)
```

### Out of Scope

- **Metric subscriptions / streaming** — polling via command is sufficient for assertions
- **Metric history / time series** — just the current value at call time
- **Custom aggregation** — histogram returns fixed fields (count, sum, min, max, mean)
- **Metric creation from Python** — read-only

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaAutomation/AutomationService.cpp` | Add `dia.automation.get_metric` command registration |
| `Tools/orchestrator/plugin.py` | Add `assert_metric` fixture |
| `Cluiche/Tests/GoogleTests/Automation/TestMetricCommand.cpp` | New test file for AC1-AC4 |
| `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj` | Add test file |
| `docs/specs/systems/dia/diaautomation.md` | Add feature row |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Register `dia.automation.get_metric` in AutomationService::RegisterCommands() | AC1-AC4 unit tests pass | Todo | sonnet | |
| 2 | Add `assert_metric` fixture to `Tools/orchestrator/plugin.py` | AC5-AC7 unit tests pass (mocked) | Todo | sonnet | |
| 3 | Run `dia run googletest`. Verify existing tests pass. | AC9 | Todo | haiku | Verification gate |
| 4 | Add feature row to `diaautomation.md`. Commit. | Doc only | Todo | haiku | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | Metric names are StringCRC on the C++ side. Python passes string, C++ constructs StringCRC. |
| PD-004 | No STL in public APIs | Command uses DiaAPI JSON path (Json::Value). No new public API with STL containers. |
| PD-007 | C++20 required | Standard C++ features. |
| AD-002 | No STL in public APIs | See PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | Command registered within `Dia::Automation::` scope. |
| SD-AUT-005 | Consistent {success, data/error} envelope | Response follows the envelope. Error via exception (framework catches and wraps). |
| SD-AUT-007 | Any automation command resets heartbeat | `get_metric` is a `dia.automation.*` command — heartbeat resets on dispatch (existing behaviour). |

## Open Questions

None.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | MetricRegistry access | Does MetricRegistry have `Has()` and `GetValue()` methods today? | Yes — MetricRegistry is a singleton with `Has(StringCRC)`, `GetValue(StringCRC) -> double`, `GetType(StringCRC)`, and histogram snapshot accessors. Implemented as part of DiaMetrics (Done). |
| 2 | Thread safety | `get_metric` command arrives on DebugServer's thread. Is MetricRegistry thread-safe for reads? | Yes — MetricRegistry uses atomic operations for counters/gauges. Histogram snapshots take a brief lock. Safe for concurrent reads from any thread. |
| 3 | Histogram default | `assert_metric` uses `mean` for histograms by default. Should the fixture accept a `field` parameter? | Good-enough default. If a scenario needs `max` or `min`, it can call `dia_client.send_command("dia.automation.get_metric", ...)` directly and inspect the full response. No extra parameter needed now. |

## Status

`Approved` (2026-05-21) — Steps 1–5 complete.
