# Research: Ideate — CLI Launcher / Dia Console

**Input:** docs/research/cli_launcher/explore.md

## Candidates

### Candidate 1: Trogon-Style Reflected Launcher
**Home module/system:** New `dia_cli/console/` package inside `Dia/DiaCLI` (invoked as `dia console` or bare `dia`)
**Size:** S
**Description:** A single Textual screen that walks `DiaCLI`'s existing command discovery (`cli_main.py`'s `_find_modules`) and Click's own introspection (`ctx.command.params`, `help`, subcommands) to build the command tree and argument forms at runtime — zero hand-authored descriptors. User picks a command from a searchable tree, fills in a generated form (checkbox for flags, text for strings, dropdown for any `click.Choice`), sees the equivalent CLI string, and runs it as a subprocess with streamed stdout/stderr.
**Primary value:** Every one of the ~50 existing subcommands is browsable and runnable from day one, with zero registry-maintenance cost, directly answering "understand all the CLI commands" and "launch exes/commands visually."

### Candidate 2: Curated Vertical-Slice Console
**Home module/system:** New `dia_cli/console/` package inside `Dia/DiaCLI`
**Size:** M
**Description:** Implements the seed doc's Phase 0-5 for a hand-picked command set (`run`, `launch`, `pipeline`, `test googletest`, `test e2e`, `test cli`, `check arch`, `check cppcheck`, `scaffold module`) with real descriptor structs, an `ExecutionService` that tails the existing `dia.output.v1` NDJSON stream, and a Textual shell with command nav / form / execution / output / results panes. Non-curated commands fall back to a raw-argument passthrough form (same as Candidate 1's generic renderer) so nothing is unreachable.
**Primary value:** Delivers the seed doc's "prove the architecture on a vertical slice" milestone with a genuinely structured results pane (test pass/fail counts, findings) for the commands used daily, while every other command stays reachable via fallback.

### Candidate 3: Full Dia Console (seed doc as written)
**Home module/system:** New `dia_cli/console/` package (mirrors seed doc's `dia_app.*` + `dia_console.*` module split) inside `Dia/DiaCLI`
**Size:** XL
**Description:** Build the complete architecture from the spec: command/project/execution/results models, full descriptor registry for all ~50 subcommands, project configuration/discovery, replaceable-UI-adapter boundary, presets, history, golden architecture test, developer docs for adding commands/projects/renderers.
**Primary value:** Matches the seed doc's Definition of Done exactly — a durable, extensible platform for CLI interaction that could later grow a web or editor front end without touching business logic.

### Candidate 4: Local Web Launcher (native-feeling window)
**Home module/system:** New `dia_cli/console_web/` package inside `Dia/DiaCLI`, FastAPI/Flask backend run in-process, free-only front end (plain HTML/CSS + a small no-license library such as Alpine.js/htmx — explicitly **not** Webix, whose free tier has licensing restrictions and whose paid tier is a cost we don't need to take on)
**Size:** M
**Description:** Same reflected-descriptor + NDJSON-tail approach as Candidate 1/2, rendered as a local web UI, but presented in a **chromeless native window via `pywebview`** (backed by Windows' built-in WebView2 runtime — no bundled browser, no address bar/tabs) rather than a browser tab, so `dia console` opens something that looks and behaves like a standalone app. Live log tail via Server-Sent Events or WebSocket. Optionally packaged as a PyInstaller one-file `.exe` for true double-click launch with no visible terminal.
**Primary value:** The only candidate that's genuinely more visual than a terminal grid (real buttons/color/resizable panes, and the only one that can render images inline — relevant for capture/render-diff artifacts) while still feeling like a native app rather than "here's a localhost URL, open your browser."

### Candidate 6: Live NDJSON Dashboard (viewer only, no launching)
**Home module/system:** New `dia_cli/cli/watch.py` command inside existing `dia_cli/cli/`
**Size:** S
**Description:** A `dia watch <system>` command that tails `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson` and renders it as a live-updating Rich table/tree (stage/step progress, pass/fail counts) instead of raw scrolling text — for a command already running in another terminal (e.g. a long `dia pipeline` kicked off elsewhere).
**Primary value:** Makes the *existing* structured event stream visible and useful today, independent of any launcher work; very cheap, immediately answers part of "understand what's happening" without touching command execution at all.

### Candidate 7: Exe-Only Launcher Panel
**Home module/system:** Extends existing `dia_cli/cli/launch.py` + new thin `dia_cli/cli/console.py`
**Size:** S
**Description:** Narrowly targets the user's stated #1 priority only: a small interactive picker (target: googletest/cluichetest/cluicheeditor, config: Debug/Release/Asan/Ubsan, plus target-specific flags like `--gtest_filter`/`--automation`) that builds and runs the same `cmd` list `launch_target()` already constructs, but through prompts instead of remembering flag names. No command registry, no reflection, no other commands touched.
**Primary value:** Directly and minimally solves "launch certain exes with command lines" — the single thing the user called most important — with almost no new architecture.

### Candidate 8: Saved Presets / Favorites (CLI-only, no new UI)
**Home module/system:** New `dia_cli/cli/preset.py` + `.dia/console.yaml`-style config inside `Dia/DiaCLI`
**Size:** S
**Description:** Implements just the seed doc's §11 presets concept: a YAML file of named command+argument combinations (`dia preset run smoke-e2e` → resolves to `dia test e2e --suite smoke`), with a `dia preset list`/`dia preset add` CLI. No visual/TUI component at all — pure convenience layer over existing commands.
**Primary value:** Solves "I keep retyping the same long command line" cheaply, complementary to (and buildable independently of) any visual launcher.

### Candidate 10: Reflected Registry + Execution Foundation (no UI)
**Home module/system:** New `dia_cli/console/model.py` (command/project/execution model only) inside `Dia/DiaCLI`
**Size:** M
**Description:** Implements only the seed doc's Phase 1-2: `CommandDescriptor`/`ExecuteCommandRequest`/`ExecutionEvent` dataclasses, a registry built by reflecting Click commands, and an `ExecutionService` that wraps existing handlers and republishes the existing NDJSON stream as typed events — with zero UI. Existing CLI is unchanged; this is pure plumbing that any future UI (Textual, web, editor) could sit on top of.
**Primary value:** De-risks the architecture question (can descriptors really be reflected? can existing commands route through a shared service without behavior changes?) before committing to any specific UI technology, at roughly 1-2 weeks of cost with a testable artifact (golden architecture test from the seed doc) even if no UI ever gets built.

## Coverage Map

Candidates 5 (Fuzzy Command Palette) and 9 (CluicheEditor Console Plugin) were discussed and dropped: 5 added little beyond what 1/7 already cover, and 9 has an unresolved bootstrapping problem (can't build CluicheEditor with a tool that lives inside CluicheEditor). Candidate 4 was refined in discussion: local web UI, served in-process, presented in a chromeless native window via `pywebview`/WebView2 (not a browser tab), with an explicitly free-only front-end stack — no Webix, to sidestep its licensing tier question entirely.

The remaining 8 candidates span every axis from explore.md: **descriptor source** (pure reflection in 1/4/7/10 vs. hand-authored-per-command in 3 vs. N/A in 6/8); **execution transport** (in-process/subprocess exec in 1/7 vs. NDJSON-tail in 2/4/6/10); **UI technology** (Textual in 1/2/3, native-windowed web in 4, none in 6/8/10); **scope** (single-purpose slivers in 6/7/8 vs. curated slice in 2 vs. full catalog in 3). All candidates place inside `Dia/DiaCLI`. Sizes range S (four candidates, days-to-a-week each) through M (three, 1-3 weeks) to one XL, giving evaluation genuine cost/value spread rather than only variations on the seed doc's own plan.
