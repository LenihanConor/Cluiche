# System Spec: DiaConsole

**Research:** @docs/research/cli_launcher/summary.md

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaConsole is a visual launcher and console for DiaCLI. It gives developers a native-window, web-rendered UI to browse every DiaCLI command, fill in arguments through a generated form, run the command, and see structured progress and results — instead of memorizing flags and reading scrolling terminal text. DiaConsole does not reimplement or duplicate any DiaCLI command's business logic: it reflects DiaCLI's existing Click command tree into a typed model, drives execution through that model, and consumes DiaCLI's existing structured NDJSON event stream. Every `dia <command>` invocation continues to work unchanged whether or not DiaConsole exists.

## Responsibilities

- **Command reflection & registry** — Build a typed command descriptor for every DiaCLI Click command/subcommand by introspecting the existing command tree (name, help, options, argument types, defaults), with an optional thin metadata overlay for richer semantic types (target/project/config/platform pickers) that Click's own model can't express.
- **Command/execution model** — Own a strictly UI-independent model (`CommandDescriptor`, `ExecuteCommandRequest`, `ExecutionEvent`, `ResultRecord`) that any front end depends on; the UI never constructs or executes business logic directly.
- **Execution service** — Invoke DiaCLI commands exclusively via subprocess (spawn `dia <reconstructed args>`, the same invocation a user would type) and republish DiaCLI's existing `dia.output.v1` NDJSON event stream as typed `ExecutionEvent`s. Does not invent a new wire event schema, and does not call DiaCLI commands in-process (see SD-CONSOLE-009).
- **Typed results** — Classify command output into structured `ResultRecord`s (diagnostic findings, test results, artifacts, generated files) for the results/findings pane, instead of leaving the user to read raw log text.
- **Project/target context** — Track which project/target (e.g. `cluichetest`, and later `CoW` and further sibling projects) the console is currently pointed at, and supply the right defaults to command forms. Resolved via DiaCLI's existing project-root tree-walk discovery and `pipeline.toml` target definitions — not a new cross-repo registry.
- **Presets** — Store and surface named, saved command+argument combinations (e.g. "E2E Smoke" → `test e2e --suite smoke`) as a core, first-class feature.
- **Native-window UI shell** — Render the above as a local web UI, presented in a chromeless native window via `pywebview` on WebView2 (not a browser tab, not a terminal UI), so the console looks and behaves like a standalone app.
- **Desktop entry point** — Package DiaConsole as a windowed (no console flash) executable with a real icon, and provide a one-command way to install a Windows Desktop shortcut to it, so a developer can launch DiaConsole by double-clicking an icon rather than typing `dia console` in a terminal.

## Public Interfaces

### Endpoints / APIs

**Command-Line Interface (new DiaCLI command, boots DiaConsole):**
```bash
dia console                     # Launch DiaConsole in its native window
dia console install-shortcut    # Create a Windows Desktop shortcut to the packaged DiaConsole.exe
```

**Command/Execution/Result Model (Python API — the UI-independent layer other front ends could sit on):**
```python
from dia_console.model import (
    CommandDescriptor, ArgumentDescriptor, OptionDescriptor, CommandCapabilities,
    ExecuteCommandRequest, ExecutionEvent, ExecutionSummary, ResultRecord,
)
from dia_console.registry import CommandRegistry
from dia_console.execution import ExecutionService, ExecutionHandle

registry = CommandRegistry.from_click_app(dia_cli.cli_main.cli)   # reflected, not hand-authored
service = ExecutionService(registry)
handle: ExecutionHandle = await service.execute(ExecuteCommandRequest(
    command_id="run", project_id="cluichetest",
    arguments={"target": "cluichetest"}, options={"config": "Debug", "shards": 4},
))
async for event in handle.events():
    ...  # ExecutionEvent stream, wraps DiaCLI's existing dia.output.v1 NDJSON
```

**Presets (config-driven, CLI-addressable):**
```bash
dia preset list
dia preset run smoke-e2e
```

No network-exposed API. The local web server backing the native window binds `127.0.0.1` only and is not reachable from other machines.

### Events Emitted

- **OnCommandSelected(commandId)** — User selects a command in the nav tree.
- **OnFormFieldChanged(commandId, fieldId, value)** — A generated form field is edited.
- **OnExecutionStarted / OnExecutionCompleted / OnExecutionFailed / OnExecutionCancelled** — Console-level lifecycle events wrapping the underlying `ExecutionEvent` stream (which itself wraps DiaCLI's existing `dia.output.v1` schema — DiaConsole does not redefine that schema).
- **OnResultProduced(resultRecord)** — A typed `ResultRecord` becomes available for the results pane.

### Data Contracts

```python
@dataclass(frozen=True)
class CommandDescriptor:
    id: str
    path: tuple[str, ...]
    name: str
    description: str
    category: str
    arguments: tuple[ArgumentDescriptor, ...] = ()
    options: tuple[OptionDescriptor, ...] = ()
    capabilities: CommandCapabilities = CommandCapabilities()
    execution_binding: str = ""

@dataclass(frozen=True)
class ExecuteCommandRequest:
    command_id: str
    project_id: str | None
    arguments: Mapping[str, Any]
    options: Mapping[str, Any]

@dataclass(frozen=True)
class ExecutionEvent:
    execution_id: str
    seq: int
    time: datetime
    type: str            # execution.started | step.started | step.progress | log | result.produced | step.completed | execution.completed | execution.failed | execution.cancelled
    step_id: str | None
    payload: Mapping[str, Any]

@dataclass(frozen=True)
class ResultRecord:
    kind: str             # DiagnosticFinding | TestResult | Artifact | GeneratedFile | GenericResult
    severity: str | None
    title: str
    summary: str | None
    payload: Mapping[str, Any]
```

**Preset files (repo-shared + user-local overlay, merged at load; user-local wins on ID collision):**
```yaml
# .dia/console-presets.yaml — committed, team-shared
presets:
  - id: smoke-e2e
    name: E2E Smoke
    command: test.e2e
    values:
      suite: smoke

# .dia/console-presets.local.yaml — gitignored, personal
presets:
  - id: my-quick-run
    name: My Quick Run
    command: run
    values:
      target: cluichetest
      config: Debug
```

## Features

| Feature | Description | Key Capabilities | Spec | Effort | Status |
|---------|-------------|-------------------|------|--------|--------|
| console-command-model | Typed command/execution/result model + registry built by reflecting DiaCLI's Click command tree | `CommandDescriptor`, `ExecuteCommandRequest`, `ExecutionEvent`, `ResultRecord`; `CommandRegistry.from_click_app()`; `ExecutionService` wrapping existing NDJSON stream | TBD | 6 days | Draft |
| console-native-shell | Local web UI rendered in a chromeless native window | `dia console` entrypoint, pywebview/WebView2 host, in-process local server (127.0.0.1 only), layout regions per mockup (`mockups/console.html`) | TBD | 6 days | Draft |
| console-typed-results | Structured results/findings pane fed by typed `ResultRecord`s | Severity-grouped cards, expandable detail, artifact list with open/reveal actions | TBD | 4 days | Draft |
| console-presets | Named, saved command+argument combinations | Repo-shared `.dia/console-presets.yaml` + gitignored user-local `.dia/console-presets.local.yaml` overlay (local wins on ID collision); `dia preset list/run`, "save current" UI action, favorites in nav | TBD | 3 days | Draft |
| console-project-context | Project/target selector + defaults, sibling-project discovery | Reads `pipeline.toml` targets, discovers sibling projects (e.g. CoW) via DiaCLI's existing tree-walk, supplies form defaults on project switch without clobbering user-edited fields | TBD | 3 days | Draft |
| console-desktop-install | Packaged, double-clickable desktop entry point with a real icon | PyInstaller windowed one-file build producing `DiaConsole.exe` with a bundled icon; `dia console install-shortcut` creates a Windows Desktop `.lnk` targeting it | TBD | 3 days | Draft |

**Total Effort Estimate:** 25 days

**Recommended Implementation Order:**
1. console-command-model (6d) — foundation; nothing else can be built or tested without it
2. console-native-shell (6d) — needs the model to render against
3. console-typed-results (4d) — needs the shell to display in
4. console-project-context (3d) — layers on the model + shell
5. console-presets (3d) — layers on the model + shell; can run concurrently with #4
6. console-desktop-install (3d) — needs a working shell to package; can run concurrently with #3/#4/#5

## Platform Primitives Used

**External (Python Packages, new for this system):**
- **pywebview** — Native chromeless window backed by WebView2, hosting the local web UI
- **PyInstaller** — Required (not optional) windowed one-file `DiaConsole.exe` build; Poetry's `console_scripts` launcher (`dia console`) always uses the console subsystem on Windows, which would flash a terminal on double-click, so the desktop entry point is a separate packaged executable
- **pywin32** — `win32com.client` `WScript.Shell` COM automation to create the Desktop `.lnk` shortcut file

**Reused from DiaCLI (no new dependency):**
- **click** — Reflected to build the command registry
- **rich** (via DiaCLI's `OutputContext`) — Existing structured terminal + NDJSON event emission that `ExecutionService` wraps

**Explicitly excluded:**
- **Webix** — Licensing-tier uncertainty; not used. Front-end stack is plain HTML/CSS plus, if needed, a small free library (e.g. htmx/Alpine.js).

## Dependencies on Other Systems

**Required:**
- **DiaCLI** — DiaConsole reflects DiaCLI's Click command tree for its registry, tails DiaCLI's existing `dia.output.v1` NDJSON event stream for execution events, and reuses `launch.py`'s exe-launch logic and `pipeline.toml` target definitions for project/target context. DiaConsole has no command business logic of its own.

**None:**
- No dependency on DiaAPI, DiaPython, DiaEditor, or CluicheEditor. A CluicheEditor-hosted variant was considered during research and ruled out (bootstrapping problem: a tool that builds CluicheEditor can't usefully live inside CluicheEditor).

**Dependents:**
- Developers working across Dia, CluicheTest, CoW, and future sibling projects under this same repo will use DiaConsole as an optional, additive layer over DiaCLI.

## Out of Scope

- **Command business logic** — Stays entirely in DiaCLI; DiaConsole only reflects, invokes, and presents it.
- **Changing DiaCLI's non-interactive CI/CD behavior** — Individual `dia <command>` invocations remain scriptable and non-interactive exactly as today.
- **Cross-repo project registry** — CoW and future projects are siblings inside this same repo; DiaCLI's existing tree-walk discovery already reaches them. A registry spanning separate checkouts/repos is not built for v1.
- **Bespoke per-command UI** — v1 uses generic generated forms and generic result renderers for every command by default; command-specific view extensions are not required to ship v1.
- **Remote/networked access** — Local-only, single-user; the backing web server is never exposed beyond `127.0.0.1`.
- **Replacing or deprecating the CLI** — `dia <command>` remains fully supported and is not made to depend on DiaConsole in any way.
- **CluicheEditor integration** — Considered and ruled out during research; not part of this system.

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-CONSOLE-001 | Command descriptors are reflected from existing Click command objects, not hand-authored per-command YAML | Click already carries name/help/params/types/defaults; a parallel hand-maintained descriptor file would drift from the real command | All features | Accepted | Yes |
| SD-CONSOLE-002 | Execution events wrap/tail DiaCLI's existing `dia.output.v1` NDJSON stream rather than a new wire schema | `OutputContext` already emits schema-versioned structured events; reuse instead of reinventing | console-command-model | Accepted | Yes |
| SD-CONSOLE-003 | UI is a local web UI rendered in a chromeless native window via `pywebview`/WebView2, not a browser tab or terminal UI | Genuine visual richness (real layout, inline artifact images) while still feeling like a standalone app; WebView2 ships with Windows 10/11, no bundled browser runtime | console-native-shell | Accepted | Yes |
| SD-CONSOLE-004 | Front-end stack is free-only; Webix is explicitly excluded | Sidesteps Webix's licensing-tier question entirely | console-native-shell | Accepted | Yes |
| SD-CONSOLE-005 | UI code depends only on the command/execution/result model's interfaces; it never constructs subprocess calls or business logic directly | Standing architectural rule for all tools built over existing business logic, not scoped to this system alone | All features | Accepted | Yes |
| SD-CONSOLE-006 | Named presets are a core v1 feature | Explicit priority — not deferred polish | console-presets | Accepted | Yes |
| SD-CONSOLE-007 | Project/target context is resolved via DiaCLI's existing sibling-project discovery and `pipeline.toml`, not a new cross-repo registry | CoW and future projects live as siblings in this same repo; the harder cross-repo problem doesn't apply | console-project-context | Accepted | Yes |
| SD-CONSOLE-008 | DiaConsole is additive; every existing `dia <command>` invocation is unaffected and DiaConsole can be removed without breaking DiaCLI | Mirrors DiaCLI's own CI/non-interactive guarantee; DiaConsole must never become a required dependency of ordinary CLI usage | All features | Accepted | Yes |
| SD-CONSOLE-009 | Execution is subprocess-only in v1 — DiaConsole never invokes DiaCLI commands in-process | Every DiaCLI command uses `ctx.exit()`/`SystemExit` and mutates global state (`sys.path`, log file handles); none has a boundary safe for in-process execution inside the console's own long-running process. Subprocess also gives free cancellation (process kill) without needing cooperative cancellation support DiaCLI commands don't have | console-command-model | Accepted | Yes |
| SD-CONSOLE-010 | Command registry is rebuilt by fresh reflection on every `dia console` launch; no persistent cache in v1 | One-time reflection cost (~20 module imports) is paid while a window is already opening; caching would add invalidation complexity for a startup-latency problem not yet measured to exist | console-command-model | Accepted | Yes |
| SD-CONSOLE-011 | Presets load from a repo-shared `.dia/console-presets.yaml` merged with a gitignored user-local `.dia/console-presets.local.yaml` overlay; local wins on ID collision | Supports both team-shared workflow presets and personal shortcuts without forcing a single scope, mirroring the workspace-vs-user-settings split | console-presets | Accepted | Yes |
| SD-CONSOLE-012 | Desktop entry point is a PyInstaller windowed one-file executable plus a `dia console install-shortcut` command that scripts creation of the Desktop `.lnk`, not a manual pythonw/Poetry-script setup | Poetry's `console_scripts` launcher always uses the console subsystem on Windows and would flash a terminal on double-click; scripting `.lnk` creation also gives a proper bundled icon and a one-command setup instead of asking users to hand-configure a shortcut's target/icon/working directory | console-desktop-install | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-005 | Platform | x64 is the only supported build target | DiaConsole's project/target context and exe-launch forms only ever target Windows x64 builds, consistent with DiaCLI's existing `launch.py`. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaConsole has no `.vcxproj` of its own (pure Python, same treatment as DiaCLI); it must not invent a parallel build-path resolution mechanism — it reads the same output paths DiaCLI already resolves via `Directory.Build.props`-derived locations. |
| PD-010 | Platform | `.diagame` is the project root file; typed imports route loading | If project/target context ever needs to resolve a sibling project's structure beyond `pipeline.toml` targets, it must resolve via that project's `.diagame`, never a hard-coded path. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Does not apply — DiaConsole is a Python system, not a C++ Dia module; documented in this system spec only, same as DiaCLI. |

**Note:** Most platform/app binding decisions (PD-001 StringCRC, PD-004 No STL, AD-002/AD-003 C++ namespaces/STL) apply to C++ code only and do not constrain this system, which is pure Python tooling — same treatment as its parent system, DiaCLI.

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Purpose | Should DiaConsole be its own system or a feature of DiaCLI? | Its own system. DiaCLI's spec explicitly lists Interactive TUI/GUI as out of scope for individual commands staying CI-safe; DiaConsole is a separate, optional, additive layer, not a mode change to DiaCLI. |
| 2 | Responsibilities | Should DiaConsole invent its own execution-event schema? | No — wrap/tail DiaCLI's existing `dia.output.v1` NDJSON stream. Avoids a second source of truth for what "a command is doing" means. |
| 3 | Public Interfaces | Should the command/execution/result model be a genuinely reusable Python API, or private to the console? | Reusable API (`dia_console.model`), per the standing UI/business-separation rule — even though no second front end is planned yet, the model must not assume a UI at all. |
| 4 | Project model | Does DiaConsole need a cross-repo `ProjectRegistry` like the original seed doc proposed? | No — CoW and future projects are confirmed to live as siblings in this same repo. Existing tree-walk discovery + `pipeline.toml` targets are sufficient for v1. |
| 5 | Decisions | Why native-window web UI instead of a terminal UI (Textual)? | Explicit user requirement for genuine visual richness, including inline artifact/image rendering that a character-grid TUI cannot provide. |
| 6 | Out of Scope | Should DiaConsole support remote/networked access so it could be checked from another machine? | No for v1 — local-only, single-user. Revisit only if a real remote-monitoring need appears. |
| 7 | Responsibilities | In-process invocation or subprocess for command execution? | Subprocess-only. Every DiaCLI command raises `SystemExit` via `ctx.exit()` and mutates global state; no command has a boundary safe for in-process calls inside the console's own process. Subprocess also yields free cancellation. |
| 8 | Responsibilities | Should the reflected command registry be cached across launches? | No for v1 — fresh reflection on every launch. The cost (~20 module imports) is a one-time startup cost paid while a window is already opening; cache invalidation isn't worth building for an unmeasured problem. |
| 9 | Features | Should presets be repo-shared, user-local, or both? | Both — `.dia/console-presets.yaml` (committed) merged with `.dia/console-presets.local.yaml` (gitignored), local wins on collision. Supports team workflow presets and personal shortcuts without forcing one scope. |
| 10 | Features | Should the desktop entry point be a manual pythonw shortcut or a packaged, auto-installed one? | Packaged — a PyInstaller windowed `DiaConsole.exe` plus `dia console install-shortcut` scripting the Desktop `.lnk`. Poetry's console-script launcher can't avoid a terminal-window flash on Windows, and scripting shortcut creation gives a real bundled icon instead of the default Python one. |

## Status

`Approved` — spec written from research (`docs/research/cli_launcher/`), not yet implemented. Plan: @docs/specs/applications/dia/systems/diaconsole/diaconsole.plan.md. All features below are `Draft`; none `Approved` yet — each needs its own `/spec-feature` pass before implementation starts.
