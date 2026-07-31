# E2E Suite: UIUltralightTestStage Crash Investigation

**Date investigated:** 2026-07-09  
**Status:** Unresolved — added to backlog

---

## Symptom

When `test_all_stages.py` runs the generic stage loop, navigating to `UIUltralightTestStage` immediately hard-crashes the app. The process dies with no error log entry — the session log ends at:

```
module.state.transition module_id=UIUltralightTestStageModule from=kInactive to=kStarting
Module 'UIUltralightTestStageModule' (PU 'MainPU') BeginStart
```

No Windows crash dump is written (no entry in `%LOCALAPPDATA%\CrashDumps`). The Python side sees `ConnectionResetError: [WinError 10054]` on the next `send_command` call.

---

## What we know

- The crash is in `UIUltralightTestStageModule::DoStart` or the Ultralight native library it initialises.
- The crash does **not** reproduce when running `test_ui_ultralight.py` in isolation via `--scenario="*uiultralight*"` — that scenario passes when run standalone (or at least reaches the checkpoint phase).
- The crash only occurs when UIUltralightTestStage is entered during `test_all_stages.py`, which runs it as the first non-Boot stage after the boot test. The app state at that point has already loaded the global assets and run DummyStage → Boot once.
- The UIModule is session-global. After one DummyStage run, `UIModule` has been Started, Stopped, and returned to Inactive. It's possible Ultralight's internal state doesn't survive a Stop/Start cycle cleanly.

---

## Hypothesis

Ultralight's native renderer or JS engine accumulates state (or frees GPU resources) on `UIModule::DoStop` that is not re-initialised correctly on the next `DoStart`. When `test_all_stages` hits UIUltralightTestStage after DummyStage has already started/stopped UIModule once, the second init crashes.

**Test to confirm:** Run `test_all_stages` with UIUltralightTestStage as the *first* stage (before DummyStage). If it doesn't crash, the UIModule stop/restart cycle is the culprit.

---

## E2E impact

The crash kills the entire app process. Every test in the session after UIUltralightTestStage gets `ConnectionClosedError` and is recorded as failed, even if the underlying stage logic is fine. This makes the E2E suite unreliable end-to-end.

---

## Fixes to consider

1. **Fix UIModule's stop/restart cycle** — audit `UIModule::DoStop` and `UIUltralightTestStageModule::DoStart` for Ultralight resources that are freed and not re-created. This is the right fix.

2. **Crash guard in UIUltralightTestStageModule::DoStart** — wrap Ultralight init in a SEH try/except or use `__try/__except` and report a `SetFailed` + navigate back to Boot. Prevents cascade, but doesn't fix the root cause.

3. **Reorder stages** — ensure UIUltralightTestStage runs before any stage that exercises UIModule (i.e., before DummyStage). Workaround only.

---

## Related files

- `Cluiche/CluicheTest/Modules/TestStages/UIUltralightTestStageModule.h/.cpp`
- `Cluiche/CluicheGameBaseline/Modules/UIModule.cpp`
- `Dia/DiaUIUltralight/` — native Ultralight wrapper
- `Cluiche/Tests/E2E/scenarios/cluichetest/test_all_stages.py`
- `Cluiche/Tests/E2E/scenarios/cluichetest/uiultralight/test_ui_ultralight.py`
