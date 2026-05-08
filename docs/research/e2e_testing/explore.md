# Research: Explore — E2E Testing

**Session date:** 2026-05-05
**Folder:** docs/research/e2e_testing/

## Problem Space Overview

End-to-end testing for game engines differs fundamentally from web/service testing. There are no HTTP endpoints to hit — instead you have real-time loops, frame-dependent state, visual output, and multi-threaded execution. The challenge is observing and asserting on a live running application without perturbing it (Heisenberg problem) while keeping the solution lightweight enough that it doesn't become a maintenance burden.

For Cluiche specifically, we need to test two distinct application types: CluicheTest (a real-time game with physics, rendering, and input) and CluicheEditor (a tool with CEF-based UI, plugin architecture, and asset pipelines). Both share the Dia engine's ProcessingUnit/Phase/Module infrastructure, which gives us architectural hooks, but they exercise very different code paths.

The goal is a solution where AI or a developer can define what to test, then automated scripts drive the applications through scenarios while an observation layer captures traces and validates outcomes — all with minimal runtime overhead and using free tooling.

## Existing Approaches

- **Input record/replay** — Record real input frames, replay deterministically, compare output. Used by AAA studios (Unreal's automation system, Unity's test framework). Brittle to timing changes but excellent for regression detection.
- **Command-driven testing** — Send commands via IPC/RPC to a running application (load level, spawn entity, advance N frames, assert state). More flexible than replay but requires a command vocabulary.
- **Visual regression** — Capture screenshots at known frames, compare against golden baselines. Catches rendering bugs but generates false positives from intentional changes.
- **State-based assertion** — Serialize game state (entity positions, component values) at checkpoints and compare against expected values. More stable than visual but misses rendering issues.
- **Headless execution** — Run game logic without rendering for faster test cycles. Useful for physics/AI but can't catch graphics bugs.
- **Hybrid harness** — External test runner (Python/pytest) orchestrates a running application via debug protocol, combining command-driven control with state/visual capture.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Orchestration** | Python pytest / C++ test harness / Shell scripts | Python gives best ecosystem (assertions, reporting, parallel) |
| **Communication** | WebSocket (existing) / Named pipes / Shared memory / Stdout | WebSocket already built (DiaDebugServer) |
| **Control model** | Input replay / Command-driven / Hybrid | Command-driven is more maintainable; replay for regression |
| **Observation** | State serialization / Screenshots / Metrics / Logs | Mix of all; state for logic, screenshots for visuals |
| **Scenario definition** | Code / JSON/YAML config / Level-per-scenario | Config-driven is most flexible for AI/manual authoring |
| **Execution model** | Headless / Windowed-hidden / Windowed-visible | Windowed needed for screenshot capture; headless for speed |
| **Validation** | Exact match / Tolerance / Perceptual hash / Rule-based | State = exact; visual = perceptual; metrics = tolerance |
| **Granularity** | Per-frame / Per-phase / Per-scenario / Per-level | Per-scenario is right level for e2e; per-frame for replay |

## Known Tradeoffs

- **Determinism vs. real-world fidelity** — Fixed timestep gives reproducibility but hides timing bugs; variable timestep is realistic but makes assertions harder
- **Intrusiveness vs. observability** — More hooks = better testing but more coupling to engine internals
- **Test speed vs. coverage** — Headless is fast but misses rendering; full windowed is slow but comprehensive
- **Brittleness vs. precision** — Exact pixel comparison catches everything but breaks on font rendering changes; loose comparison misses subtle bugs
- **Maintenance cost vs. confidence** — More scenarios = more confidence but more upkeep when features change
- **Generic framework vs. bespoke solution** — Generic is reusable but over-engineered; bespoke is faster to build but harder to extend

## Known Pitfalls (C++ / game engine context)

- **Non-deterministic threading** — Multi-threaded ProcessingUnits can execute in different orders between runs; must either force ordering in test mode or tolerate variance
- **GPU-dependent rendering** — Screenshots differ across GPU vendors/drivers; perceptual comparison needed
- **Startup time** — Game applications have initialization overhead; batch scenarios per launch where possible
- **Resource cleanup** — Crashes during tests can leave orphan processes, locked files, GPU resources
- **Frame timing** — vsync, sleep granularity, and OS scheduling mean "wait 60 frames" isn't deterministic in wall-clock time
- **CEF complexity** — Editor's Chromium UI adds process management complexity; CEF has its own test infrastructure
- **Build dependency** — Tests depend on successful build + deploy; DiaCLI pipeline must succeed first

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaDebugServer** | WebSocket server already running in both apps; supports subscribe/command/metrics — primary test communication channel |
| **DiaDebugProtocol** | Protobuf message format with COMMAND_REQUEST/RESPONSE, DATA_UPDATE, CORE_METRICS, LOG_BATCH — ready-made test protocol |
| **DiaAPI (CommandRegistry)** | Register test-specific commands (load_scenario, dump_state, assert, screenshot) — extensible command vocabulary |
| **FrameStream\<EventData\>** | Input recording/playback infrastructure — can serialize input sequences for replay |
| **StateSerializer** | JSON state serialization already built — can dump application state at checkpoints |
| **MetricsCollectorModule** | FPS, frame time, memory already captured — performance assertion ready |
| **DiaLogger (ISink)** | Structured logging with custom sinks — can capture all logs during test for assertion |
| **LevelFactory** | Register/create levels by StringCRC — test scenarios as levels or level parameters |
| **DiaVisualDebugger** | Debug draw layers — can verify spatial correctness without full render comparison |
| **DiaWebSocket** | WebSocket client/server — Python test client connects trivially |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Test scenarios and commands should use StringCRC identifiers, not raw strings |
| PD-002 ProcessingUnit/Phase/Module | Test infrastructure registers as Modules within existing PU lifecycle; phases provide natural checkpoint boundaries |
| PD-003 Component system | Entity state assertions operate on IComponent/IComponentObject — need serializable component state |
| PD-004 No STL in public APIs | Any engine-side test utilities must use DiaCore containers in their public API |
| PD-005 x64 Windows only | No cross-platform concerns; can use Windows-specific tools (ETW, WinAPI screenshots) |
| PD-007 C++20 | Can use concepts, std::span, structured bindings in test infrastructure code |
| PD-008 Directory.Build.props | Test output goes to standard `bin/` paths; no custom output directories |
| PD-009 Generated output under out/ | Test results, screenshots, traces go to `Cluiche/out/<AppName>/` |

## Open Questions for Ideation

- Should the test harness be a new Dia module (e.g., `DiaTestHarness`) or purely external (Python scripts connecting to existing DebugServer)?
- Can CluicheEditor tests reuse the same WebSocket protocol, or does CEF require separate automation (e.g., Chrome DevTools Protocol)?
- Should input replay serialize to JSON (human-readable, editable) or binary (compact, fast)?
- How do we handle test scenario definition — YAML config files that describe steps, or code-defined scenarios?
- What's the minimum viable "observation" for v1 — just crash/assert/log checking, or do we need state dumps and screenshots from day one?
- Should tests run the full render pipeline or support a "headless" mode that skips GPU?
- How do we handle the multi-stage requirement (e.g., test DiaRigidBody2D scenarios separately from rendering scenarios)?
- Can we leverage `dia run` CLI's existing pipeline (build + deploy + launch) as the test launcher, or do we need a separate test runner?
