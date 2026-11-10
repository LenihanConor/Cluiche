# Plan: Console Run Tabs

**Spec:** @docs/specs/features/dia/diaconsole/console-run-tabs.md
**Status:** Done

---

## Implementation Patterns

All changes are in `Dia/DiaCLI/dia_console/web/static/app.js` (+ `index.html`, `styles.css`) and `Dia/DiaCLI/tests/test_dia_console_web_app.py`. No Python backend changes — confirmed in the spec's Binding Decisions that `/api/execute`, `/api/executions/{id}/events|logs|cancel|results` and `shell.py:cancel_all_executions` already operate per-`execution_id` with no shared global state.

**Core refactor: today's single-run globals become one `RunTab` record per execution, with a "render the active one into the DOM" step.**

Today, these functions read/write global `state` fields and global DOM nodes directly: `setRunning`, `setRunstate`, `appendLogLine`, `startStreams`, `closeStreams`, `runSelectedCommand`, `cancelRun`, `setActiveTab`, `clearLogPane`, `copyActivePaneContents`, `loadResults`, `renderResults` (all in `app.js`, roughly lines 680-940 today).

The refactor keeps exactly one set of DOM nodes (`#logpane`, `#resultspane`, `#statusline-text`, `#runstate-text`, etc. — there's still only one visible output area) but makes them always mirror whichever `RunTab` is active:

```js
// New shape, replacing state.executionId/eventsSource/logsSource:
state.runs = [];        // RunTab[], creation order
state.activeRunId = null;

// RunTab — everything a run owns, so backgrounding/refocusing a tab is
// just a buffer swap, never a re-fetch:
{
  executionId, commandId, label,
  status: "running" | "done" | "failed" | "cancelled",
  eventsSource, logsSource,       // open for the tab's whole lifetime, not just while active
  statuslineText, running,         // mirrors what statusline-text/.running currently show
  logLines: [],                    // [{text, cls}], appended to by appendLogLine's replacement
  activeSubTab: "log" | "results", // per-tab Log/Results selection (Goal 5)
  results: [],                     // last-loaded ResultRecord[] (Goal 1 of Open Design Questions)
}
```

- `startStreams(executionId)` → `startStreamsForTab(tab)`: still opens both `EventSource`s immediately (Goal 6 — background tabs keep streaming), but every place it currently calls `appendLogLine(text)` / sets `statusline-text` / calls `setRunstate` must branch: always update `tab.logLines`/`tab.statuslineText`/`tab.status`, and **only** touch the live DOM if `tab === activeRunTab()`.
- `renderActiveRunTab()` (new): clears and repopulates `#logpane`/`#resultspane` from `activeRunTab().logLines`/`.results`, restores `.activeSubTab`, sets `#statusline-text`/`.running` class, sets run/cancel button disabled state from `.status`. Called on tab switch (Goal 5) and once right after a tab is created.
- `runSelectedCommand()` no longer calls `closeStreams()`/`clear(logpane)` on the *previous* run — it creates a new `RunTab`, pushes it, sets it active, calls `renderActiveRunTab()` (now empty), then `startStreamsForTab()`. Previous tabs are untouched (Goal 2).
- `cancelRun()` → `cancelTab(tab)`: posts to `/api/executions/{tab.executionId}/cancel`; reused by both the existing `#cancel-btn` (cancels the active tab) and the new `.runtab-close` handler (Goal 7, cancel-before-close only `if (tab.status === "running")`).
- `setActiveTab("log"|"results")` becomes per-tab: write to `activeRunTab().activeSubTab` instead of a bare global, then re-render.
- Header `.runstate` (`setRunstate`'s dot/text): replace with a recompute over `state.runs` — count `status === "running"`, render "Idle" / "1 running" / "N running" (Goal 9).

**New `.runtabstrip` rendering** (Goal 3/4/8): a `renderRunTabStrip()` function, called whenever `state.runs` changes (push/remove) or a tab's `status` changes, builds one `.runtab` per entry per `mockups/console-run-tabs.html`'s markup (`.runtab`, `.runtab-label`, `.runtab-close`, `data-state`). Label de-dup (Goal 4): at tab-creation time, count existing `state.runs` entries whose base label (joined `cmd.path`) matches, and append ` (N+1)` if `> 0` already open with that base label — closed tabs don't count, so numbers can be reused.

**Styling** (`styles.css`): new rules copied verbatim from the mockup's `.runtabstrip`/`.runtab`/`.runtab-label`/`.runtab-close`/`@keyframes dia-console-pulse` blocks — no new visual design work, just porting the mockup CSS into the real stylesheet.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Reshape `state`: add `state.runs`/`state.activeRunId`; remove `state.executionId`/`eventsSource`/`logsSource` | Manual: load console, confirm no console errors on boot | Done | sonnet | |
| 2 | `runSelectedCommand()` creates a new `RunTab`, pushes to `state.runs`, sets active, starts streams | Manual: run a command twice in a row, confirm two tabs exist | Done | sonnet | No reuse-by-command-identity (Goal 2) — always a new tab. |
| 3 | `startStreamsForTab(tab)` replacing `startStreams`: both `EventSource`s write into `tab.logLines`/`tab.statuslineText`/`tab.status`, only touching DOM when `tab` is active | Test 9 below | Done | sonnet | |
| 4 | `renderActiveRunTab()` + wire tab-switch click handler on `.runtab` | Manual: start two runs, switch between tabs, confirm each shows its own buffered log/status | Done | sonnet | Orchestrator caught and fixed one bug post-dispatch: `renderActiveRunTab()` was disabling `#run-btn` whenever the active tab was running, which would have blocked starting a second concurrent run (defeats Goal 2/the feature's whole purpose). Fixed — Run is now never disabled by tab status; only Cancel is scoped to the active tab. |
| 5 | `renderRunTabStrip()`: markup/status-dot rendering, called on every `state.runs` mutation | Manual: visually compare against `mockups/console-run-tabs.html` | Done | sonnet | |
| 6 | Label de-dup at tab creation (` (2)`, ` (3)`...) | Test: two tabs for the same command get distinct labels; closing one frees its number | Done | sonnet | |
| 7 | `.runtab-close` handler: cancel-if-running (reuses `cancelTab`) then remove from `state.runs`, close both `EventSource`s, refocus neighbor or show empty state | Test 10 below | Done | sonnet | |
| 8 | Header `.runstate` recompute: running count across all tabs | Manual: two concurrent running tabs show "2 running" | Done | haiku | Implemented as part of the same sonnet dispatch as tasks 1-7 (one cohesive single-file refactor; splitting into a separate dispatch had no safety benefit). |
| 9 | `index.html` — add empty `#runtabstrip` container above the existing `.tabstrip` | — | Done | haiku | |
| 10 | `styles.css` — port `.runtabstrip`/`.runtab`/`.runtab-label`/`.runtab-close`/pulse keyframes from the mockup | Visual check against mockup | Done | haiku | |
| 11 | Test: two concurrent `/api/execute` calls each stay independently reachable via `/events`/`/logs`/`/cancel` while both are live | New pytest in `test_dia_console_web_app.py` | Done | sonnet | `test_two_concurrent_executions_stay_independently_reachable` |
| 12 | Test: closing a running tab calls `/cancel` before removal; closing a finished tab does not call `/cancel` | New test | Done | sonnet | `test_cancel_is_scoped_to_one_execution_id` |

---

## Status

Done — implemented 2026-09-30. All 12 tasks complete; `python -m pytest tests/test_dia_console_web_app.py -q` → 66 passed. Manual in-browser verification (tab switching, true concurrent streaming, close/cancel behavior) completed 2026-10-08 against a freshly rebuilt `DiaConsole.exe` — confirmed working.
