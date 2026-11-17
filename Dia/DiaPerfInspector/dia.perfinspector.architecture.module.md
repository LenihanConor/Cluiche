---
schema: dia.module.v1
module_id: dia.perfinspector
name: DiaPerfInspector
layer: tools/inspector
path: Dia/DiaPerfInspector
status: active
dependent_modules:
  - dia.editor
  - dia.core
  - dia.json
  - dia.observation
public_api:
  headers:
    - DiaPerfInspector/DiaPerfInspectorPlugin.h
  namespaces:
    - Dia::Editor
  entry_points:
    - REGISTER_EDITOR_PLUGIN(DiaPerfInspectorPlugin, "DiaPerfInspector")
responsibilities:
  - Subscribe the perf.timing game topic and forward it to the UI as perf_inspector.timing
  - Relay perf.window, perf.frame and perf.jobs queries, and the perf.pause / perf.resume / perf.resume_all commands, to the connected game and push replies as perf_inspector.query_result
  - Serve the Inspector UI via dia://plugins/diaperfinspector/index.html (Frames tab: lanes, module table, frame detail, trips, chain, latency and self-cost; Workers tab: job heat strip, steps, owner share, pool health; Chrome-trace export)
non_responsibilities:
  - Lane maths, decoding and chart rendering (pure TypeScript in UI/src/model)
  - Owning the GameConnectionManager service (fetched from PluginServiceLocator)
  - Persisting any state to disk
---

# DiaPerfInspector

CluicheEditor Inspector-tier plugin showing live frame and module timing of a running Dia application over DiaDebugServer. Game-side data comes from CluicheGameBaseline (`PerfWindowCommands`, `PerfTimingPublisher`) over the `$perfcoarse` and `$perfjobs` streams. Trips, markers, latency, the live chain and deferred-step summaries have no game-side feed yet; the UI shows them as unavailable (plan: `docs/specs/applications/dia/systems/diaobservation/perf-inspector.plan.md`, rows 3b, 6b, 10b).
