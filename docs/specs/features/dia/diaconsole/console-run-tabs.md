# Feature Spec: DiaConsole — Console Run Tabs

## Parent System
@docs/specs/applications/dia/systems/diaconsole/diaconsole.md

**Status:** `Done`

---

## Summary

The web UI currently tracks exactly one in-flight execution at a time (`state.executionId`/`eventsSource`/`logsSource` in `app.js`), even though the backend already tracks arbitrary concurrent executions (`app.state.executions: dict[str, ExecutionHandle]` in `dia_console/web/app.py`, and `shell.py`'s window-close handler already cancels *every* tracked execution, not just one). Starting a second run today doesn't kill the first process, but it silently drops the UI's only connection to its logs and events — there's no way to get back to it.

This feature adds a tab strip above the existing Log/Results tabs: one tab per execution, newest appended to the right and focused automatically. Each new Run/Debug action always opens a new tab — so running the game and the editor side by side (or the same command twice) just means two tabs, not a replaced view. Tabs can be closed individually; closing a running tab cancels it first. This is a pure front-end feature — no new backend endpoints, since the execution/cancel/stream API already supports arbitrary `execution_id`s concurrently.

Mockup: [`mockups/console-run-tabs.html`](mockups/console-run-tabs.html).

---

## Goals

1. `app.js` state changes from singular (`executionId`, `eventsSource`, `logsSource`) to a list: `state.runs: RunTab[]` plus `state.activeRunId`. Each `RunTab` holds `{ executionId, commandId, label, status, eventsSource, logsSource, statusline fields, bufferedLogLines, results }` — everything today's single-run state held, now owned per-tab instead of shared.
2. Clicking **Run** or a **Debug** action (`run-btn`'s existing handler) always appends a new `RunTab` and makes it active — never reuses or restarts an existing tab, even for the identical command. This matches the simplest mental model: a tab *is* a run.
3. New `.runtabstrip` renders above the existing `.tabstrip` (Log/Results), one `.runtab` per `state.runs` entry in creation order. Each shows a status dot (`accent`, pulsing, while running; `green` on `execution.completed`; `red` on `execution.failed`; `yellow` on `execution.cancelled`) and a label, per the mockup.
4. Tab label is the command's path joined with spaces (e.g. `run cluichetest`). If a label collides with another currently-open tab, append ` (2)`, ` (3)`, ... in open order — distinguishes "game run #1" from "game run #2" without needing argument values in the label.
5. Switching the active tab (clicking a `.runtab`) swaps the entire output area to that tab's own state: its Log/Results sub-tab selection, statusline (step/running dot/progress bar), log buffer, and results — per-tab scope, not a shared pane that just changes source. Log/results content already received while a tab was backgrounded must still be visible when it's refocused (buffered per tab, not re-fetched).
6. Each `RunTab` opens its own `EventSource` against `/api/executions/{id}/events` and `/api/executions/{id}/logs` at creation time (not just the active one) — backgrounded tabs keep streaming and updating their stored status/log buffer/results in the background, so their dot and buffered content are accurate whenever you switch to them.
7. `.runtab-close` (visible on hover, or always on the active tab per the mockup): if the tab's status is still running, `POST /api/executions/{id}/cancel` first (same call as today's `cancel-btn`), await its settling, then remove the tab from `state.runs` and close both its `EventSource`s; if already finished, just remove it. Closing the active tab focuses the next tab to its left, or shows the existing empty-hint state if none remain.
8. No cap on concurrent tabs; `.runtabstrip` scrolls horizontally once tabs overflow the width (per the mockup's `overflow-x: auto`).
9. The header's `.runstate` indicator reflects *any* tab running (e.g. "2 running"), not just the active one — today's singular "Idle"/"Running" text needs a count, since two tabs can legitimately be running at once.
10. No backend changes. `/api/execute`, `/api/executions/{id}/events`, `/api/executions/{id}/logs`, and `/api/executions/{id}/cancel` already operate per-`execution_id` with no shared/global state, and `shell.py:cancel_all_executions` already cancels every tracked execution (not a single current one) on window close — both already generalize correctly to N concurrent tabs without modification.

---

## Data Contracts

No new HTTP routes or Python data types. Entirely a client-side (`app.js`) state reshape:

```js
// Replaces the singular executionId/eventsSource/logsSource/selectedCommand-
// execution coupling. selectedCommand (nav selection, for the run FORM)
// stays separate and singular -- it's "what command is the form showing,"
// unrelated to "which runs are in flight."
state.runs = [
  {
    executionId: "exec-...",
    commandId: "run",
    label: "run cluichetest",       // de-duplicated per Goal 4
    status: "running",              // running | done | failed | cancelled
    eventsSource: EventSource,
    logsSource: EventSource,
    statusline: { stepText, running, progressVisible },
    logLines: [ /* buffered <div class="logline"> data, not DOM nodes */ ],
    results: [ /* ResultRecord[], once console-typed-results populates this */ ],
  },
  // ...
];
state.activeRunId = "exec-...";     // which tab's data is currently rendered
```

**Markup (`index.html`):** new `<div class="runtabstrip" id="runtabstrip"></div>` inserted immediately before the existing `<div class="tabstrip">` inside `.outputpanel`, populated entirely by `app.js` (no static tab markup, unlike the fixed Log/Results tabstrip).

---

## Tasks

| # | Task | Notes | Model |
|---|------|-------|-------|
| 1 | `app.js` — reshape state to `state.runs[]` / `state.activeRunId`; remove singular `executionId`/`eventsSource`/`logsSource` | Every call site that read the old singular fields (status line render, log append, cancel button, results load) moves to reading/writing `state.runs[activeIndex]`. | sonnet |
| 2 | `app.js` — Run/Debug handler creates a new `RunTab`, opens its two `EventSource`s immediately, appends and focuses it | Reuses the existing `/api/execute` call and SSE wiring verbatim; only the *ownership* of the sources changes (per-tab, not shared). | sonnet |
| 3 | `app.js` + `index.html` — render `.runtabstrip`, one `.runtab` per `state.runs`, status-dot coloring from `status` | Matches `mockups/console-run-tabs.html` class names (`.runtab`, `.runtab-label`, `.runtab-close`, `data-state`). | sonnet |
| 4 | `app.js` — tab click switches `state.activeRunId` and re-renders statusline/log/results from that tab's buffered data | No re-fetch; buffered data from Task 2's background streaming is the source of truth. | sonnet |
| 5 | `app.js` — close button: cancel-if-running then remove tab, close its `EventSource`s, refocus neighbor or show empty state | Reuses the existing cancel POST (today's `cancel-btn` logic at the old `state.executionId` call site). | sonnet |
| 6 | `app.js` — label de-duplication (` (2)`, ` (3)`...) at tab-creation time | Computed against currently-open `state.runs` labels only — a closed tab's number can be reused. | sonnet |
| 7 | `app.js` — header `.runstate` shows a running count across all tabs instead of a single Idle/Running state | | haiku |
| 8 | `styles.css` — `.runtabstrip`/`.runtab`/`.runtab-label`/`.runtab-close` rules, lifted from the mockup | | haiku |
| 9 | Test: opening two runs keeps both `EventSource`s alive and both tabs' buffered state independently correct | New test in `test_dia_console_web_app.py` or a JS-level test if one exists for `app.js` already — otherwise a Python test driving `/api/execute` twice and asserting both `execution_id`s remain independently reachable via `/events`/`/logs`/`/cancel`. | sonnet |
| 10 | Test: closing a running tab calls `/cancel` before the tab disappears; closing a finished tab does not call `/cancel` | | sonnet |

---

## Binding Decisions

| Decision | Implication |
|----------|--------------|
| SD-CONSOLE-005 — UI depends only on the model's interfaces; never constructs subprocess calls or business logic directly | This feature adds zero new business logic — it only re-shapes which HTTP responses (`/api/execute`, `/events`, `/logs`, `/cancel`) the UI keeps around and renders. |
| SD-CONSOLE-008 — DiaConsole is additive; existing behavior must keep working | Single-run usage still works exactly as today, just inside a tab (the first tab); nothing about the existing `/api/*` surface changes. |
| SD-CONSOLE-009 — execution is subprocess-only, never in-process | Unaffected — each tab still just owns an `execution_id` whose subprocess lifecycle is entirely server-side. |

No conflicts. `console-native-shell`'s Goal 8 (cancel in-flight executions before window close) and `shell.py:cancel_all_executions` already iterate `app.state.executions` in full, so this feature's multiple concurrent tabs are already covered by that existing behavior without modification.

---

## Open Design Questions

1. **Results tab per finished run.** `console-typed-results` was built against a single current execution. This spec assumes each `RunTab` gets its own `results` buffer (Goal 1), but the actual rendering code in that feature may need a small follow-up change to read from the active tab's buffer instead of a single shared one — worth checking against the real `console-typed-results` implementation before marking Task 4 done, not just assuming the shape lines up.
2. **Memory growth from backgrounded log buffers.** Buffering full log text per tab (Goal 5/6) for an unbounded number of tabs (Goal 8: no cap) means a long-lived console session with many large runs keeps all their log text in memory. Not a problem at today's usage scale, but worth revisiting if someone leaves DiaConsole open for days running dozens of full pipeline builds.

---

## Status

`Done` — implemented 2026-09-30 (plan: `docs/specs/features/dia/diaconsole/console-run-tabs.plan.md`, all 12 tasks Done, `pytest tests/test_dia_console_web_app.py` → 66 passed). Manually verified in-browser 2026-10-08 (tab switching, concurrent streams, close/cancel UX) against a freshly rebuilt `DiaConsole.exe`.
