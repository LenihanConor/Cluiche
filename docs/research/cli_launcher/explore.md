# Research: Explore — CLI Launcher / Dia Console

**Session date:** 2026-09-04
**Folder:** docs/research/cli_launcher/
**Seed input:** `Dia_Console_Architecture_and_Build_Specification (1).docx` (user-supplied v0.1 architecture spec, ~780 lines) + user's stated priorities: (1) understand all CLI commands, (2) run tests on CLI commands, (3) **most important** — launch exes with command lines, visually and more easily than raw `dia` invocations.

## Problem Space Overview

`dia_cli` (DiaCLI) has grown into a ~20-top-level-command Click application covering build/run/launch, testing, static analysis, scaffolding, docs generation, environment setup, and asset pipelines (Appendix B of the seed doc counts ~50 subcommands). Discoverability is entirely `--help`-driven: a developer has to already know a command exists, or grep the CLI source, to find it. There is no visual surface for browsing commands, filling in arguments, or re-running a previous invocation without retyping it.

The seed doc frames this as "Dia Console" — a full replaceable-UI-adapter architecture: a data-driven command/project/execution/results model shared between the existing argparse-style CLI and a new Textual TUI, with strict UI/business separation (R1/R7), generic generated forms (R8), and a phased 7-stage rollout ending in full registry migration of every command.

This is one instance of a well-known category: **developer command-launcher / task-runner UIs** — tools that sit on top of an existing CLI or task graph and add discovery, parameterized re-run, and live progress, without becoming a second source of truth for command behavior. The category ranges from tiny (`fzf`-piped command history, `make` target pickers) to heavy (Bazel's `buildbuddy`, Nx Console, VS Code task runners, Jenkins/GitHub Actions "Run workflow" forms).

## Existing Approaches

- **Fuzzy command palettes over an existing CLI** — `fzf`-based wrappers that parse `--help` or a static list and let you arrow-select + fill placeholders (e.g. shell functions piping `command --help` through `fzf`). Cheap, no execution model, no structured results.
- **Task-runner TUIs** — `k9s` (Kubernetes), `lazydocker`, `lazygit`: domain object tree on the left, action panel on the right, live log tail at the bottom. Close in shape to the seed doc's mockup. All are read-mostly with a handful of mutating actions, not ~50 arbitrary subcommands with rich argument sets.
- **IDE task-runner integrations** — VS Code "tasks.json" + Run/Debug panel, JetBrains Run Configurations, Nx Console: forms are generated from declared task metadata (already partially true for us — see below), history/rerun is a first-class feature, output goes to a structured "Problems" pane in addition to a raw terminal.
- **Self-describing CLI reflection** — tools like `click-repl`, `trogon` (Textual + Click integration that auto-generates a TUI form from a Click app's own command tree, no external descriptor file), or argparse introspection. Directly relevant: **Click already carries most of the "command descriptor" the seed doc wants hand-authored in YAML** (name, help, options, types, defaults, required-ness, groups/subcommands).
- **CI "Run workflow" forms** — GitHub Actions manual dispatch inputs, Jenkins parameterized builds: minimal, generated purely from a workflow's declared `inputs:`, no execution-service abstraction layer.

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Descriptor source | (a) Hand-authored YAML per command (seed doc's plan) · (b) Reflected from existing Click `cli()` objects · (c) Hybrid: reflect + optional YAML overlay for grouping/labels/presets | (b)/(c) avoid a second source of truth that can drift from the real command; seed doc assumed argparse and no existing structured output, neither of which is true here |
| Execution transport | (a) In-process Python call into the Click command · (b) Subprocess spawning `dia <args>` and parsing stdout · (c) Tail the existing NDJSON event log DiaCLI already writes | (c) is nearly free — `OutputContext` already emits schema-versioned (`dia.output.v1`) structured events to `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`; a launcher can tail that file instead of inventing an `ExecutionEvent`/`ExecutionHandle` abstraction from scratch |
| UI technology | (a) Terminal UI (Textual, per seed doc) · (b) Local web UI (Flask/FastAPI + HTML, in the vein of existing Webix/VisJS debug tooling) · (c) Native GUI (Tk/PySide) · (d) CluicheEditor plugin (CEF panel) | (d) has a bootstrapping problem: CluicheEditor itself must be built via `dia pipeline` before you can use it to launch things — bad fit for a tool whose job includes "build things." (a)/(b) work before anything else is built |
| Scope of v1 | (a) Full registry covering all ~50 subcommands (seed doc's Definition of Done) · (b) Small curated set matching the user's stated priority (run/launch/pipeline/test) with a fallback "raw command" mode for the rest | (b) matches "most important: launch exes with command lines" and de-risks the bet; (a) is the seed doc's stated non-goal-avoidance but is a multi-week investment before any payoff |
| Repo placement | (a) New module inside `Dia/DiaCLI/dia_cli/console/` (same Python package, same Poetry env) · (b) New sibling Python project under `Cluiche/Tools/` or similar · (c) Fully separate repo | See Cluiche-Specific Opportunities below — DiaCLI already sets precedent for Python tooling living inside this repo; a separate repo adds version-sync overhead (console must track every new command) for no clear benefit since it only ever targets this one CLI |
| Test-running scope ("run tests on the CLI commands") | (a) Visually drive engine/product test commands already in DiaCLI (`test googletest`, `test e2e`, etc.) · (b) Run/display DiaCLI's own self-test suite (`dia test cli`, which pytests the CLI itself) · (c) Both | Ambiguous in the user's phrasing — worth resolving before scoping (flagged as open question below) |

## Known Tradeoffs

- Reflecting descriptors from Click vs. hand-authoring YAML: reflection is cheaper and can't drift, but loses the seed doc's richer typed-option vocabulary (`target`, `project`, `duration`, `model identifier`, min/max) unless we extend Click's own `type=` mechanism or layer a thin metadata dict on top of existing `@click.option` calls.
- Tailing the existing NDJSON log vs. building a proper `ExecutionService`: tailing is much cheaper to ship but couples the launcher to a file-based side channel rather than a clean in-process event bus; it's fine as a v1 bridge but would need revisiting if truly async/concurrent multi-command execution (several running at once) becomes a requirement.
- Small curated command set vs. full catalog: curated ships faster and matches stated priority, but risks becoming "yet another place commands need to be added," undermining the "no UI changes for new normal commands" goal the seed doc cares about — mitigated by a documented fallback path (unregistered commands still runnable via a raw-argument passthrough form).
- TUI vs. local web UI: Textual is more "developer-native" for a CLI adjunct and needs no browser; a local web UI can reuse existing Webix/VisJS familiarity in this codebase and is easier to make genuinely "visual" (buttons, dropdowns, colored status) but adds a server process and browser dependency for what might be used from a plain terminal session.

## Known Pitfalls (C++ / game engine context)

- Not directly applicable here — this tool is Python-only tooling around `dia_cli`, not engine C++. The main pitfall is **treating it as if it needs engine-side changes at all**: none of PD-001–PD-010 (StringCRC, ProcessingUnit/Phase, component system, no-STL, vcxproj) bind Python tooling. The real risk is architectural drift in the *Python* side: letting UI code reach into `subprocess`/business logic directly (exactly what R1/R7 in the seed doc warn about), or building bespoke per-command UI that has to be hand-maintained forever (R8).
- Windows-specific: launching built `.exe` targets (GoogleTests.exe, CluicheTest.exe, CluicheEditor.exe) needs careful handling of working directory, process lifetime, and cancellation (`Ctrl+C` semantics differ for a subprocess launched from a TUI vs. a normal shell) — `launch.py` already does `subprocess.run(cmd, cwd=str(out_dir))` synchronously; a visual launcher needs non-blocking spawn + live output streaming instead.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| `Dia/DiaCLI/dia_cli/cli/*.py` | The ~20 Click command groups/commands (`run`, `launch`, `pipeline`, `check`, `test`, `env`, `scaffold`, `docs`, `agent`, `codegen`, `reflect`, `capture`, `api`, `show`, `command`) — the actual source of truth a launcher must reflect, not duplicate |
| `Dia/DiaCLI/dia_cli/utils/dia_output.py` | `OutputContext` — already emits structured, schema-versioned (`dia.output.v1`) NDJSON execution events (`OnRunStarted`, `OnStageStarted`, `OnStepCompleted`, `OnStepFailed`, `OnRunCompleted`, ...) per invocation. This is most of the seed doc's §7 "Execution model and events" already built. |
| `Dia/DiaCLI/dia_cli/cli/launch.py` | `launch_target()` — the existing "launch exe with args" implementation: hardcoded `_TARGET_EXE_MAP` for 3 targets (googletest/cluichetest/cluicheeditor), builds a `cmd` list, `subprocess.run`s it. This is exactly the capability the user wants made visual/general — currently config/filter/shards/automation flags must be typed by hand each time. |
| `Dia/DiaCLI/dia_cli/cli_main.py` | `DiaCLI(click.MultiCommand)` — auto-discovers command modules by walking for `cli/*.py` files; a registry-building pass can reuse this exact discovery mechanism to enumerate every registered Click command programmatically |
| `Dia/DiaCLI/tests/` (pytest) + `dia test cli` | DiaCLI's own self-test suite — candidate target for "run tests on the CLI commands" reading (b) |
| CluicheEditor / DiaEditor plugin framework (`IEditorPlugin`, CEF) | Considered and likely **not** the right host — see Design Axes/UI technology; requires the C++ app to already be built, which a build-and-launch tool cannot assume |
| Webix / VisJS (External/) | Existing precedent in this codebase for web-based debug/visualization UI, relevant if a local web UI is chosen over Textual |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | N/A — Python tooling, not engine C++ entity/component IDs |
| PD-004 No STL in public APIs | N/A — applies to Dia C++ module public headers, not Python CLI tooling |
| PD-005 x64 Windows only | Indirectly relevant: any exe-launching feature only ever targets Windows x64 builds, consistent with existing `launch.py` |
| PD-006 VS project files are source of truth | N/A — DiaCLI has no `.vcxproj`; it's a Poetry-managed Python package, already precedent for non-VS tooling living in this repo |
| PD-010 `.diagame`/`.diastage` typed imports | Relevant if the launcher ever needs to discover *projects* (per the seed doc's Project model) — should resolve via `.diagame`, not hard-code paths, consistent with existing platform rule |

No PD- decision in the platform spec directly targets Python developer tooling; DiaCLI's own existence is the working precedent that such tooling lives inside this repo under `Dia/DiaCLI/`.

## Open Questions for Ideation

- Does "run tests on the CLI commands" mean (a) visually drive product/engine test commands (`test googletest`, `test e2e`, `check cppcheck`, etc.), (b) surface/run DiaCLI's own self-test suite (`dia test cli`) so changes to the CLI itself are validated, or (c) both? This changes what "results" the tool needs to render.
- Should v1 target a small curated command set (run/launch/pipeline/test/check) with a raw-passthrough fallback for everything else, or attempt the seed doc's full-catalog registry from the start?
- Terminal UI (Textual) vs. local web UI (reusing Webix/VisJS familiarity) vs. something even lighter (a single-screen curses-free "launcher" that's really just a fuzzy-picker + form) — how much "visual" does the user actually want vs. how much is "just make it faster to reuse a command with different args"?
- Should command descriptors be purely reflected from existing Click objects (zero maintenance, but limited to what Click already models), or reflected + a thin optional metadata layer (grouping/labels/presets) bolted on via decorator or sidecar dict — avoiding the seed doc's separate hand-maintained YAML-per-command entirely?
- Is there real demand for the seed doc's heavier concerns (project registry across multiple external repos, cancellation semantics, results/findings taxonomy, replaceable-UI-adapter architecture) at v1, or are those premature for a tool whose stated #1 priority is "let me launch an exe with the right flags without retyping them"?
