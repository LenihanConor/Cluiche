# Plan: orchestrate-hardening

**Spec:** @docs/specs/applications/dia/systems/diacli/dia-orchestrate.md
**Status:** Done

## Context

Post-implementation review of the E2E orchestration system surfaced correctness bugs and spec-divergence. These fixes harden the existing system before adding new scenarios.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Fix log-error cascade: track high-water mark per test | In `plugin.py`, record line count at test setup; only scan lines after that mark at teardown. Verify: two tests where test 1 causes an ERROR — test 2 should not fail from test 1's error. | Done | sonnet | Critical. Current impl re-reads entire session log at every teardown, so one error fails all subsequent tests. |
| 2 | Add state-reset in `dia_client` fixture setup | After connecting, call `navigate_to("Boot")` (or `report()` + conditional navigate) to guarantee known start state. Verify: a test that leaves app in DummyStage does not pollute the next test. | Done | sonnet | Prevents cascading failures when a test dies mid-navigation. |
| 3 | Replace direct exe launch with `dia launch` | Remove `_APP_EXE_MAP`. Use `subprocess.Popen(["dia", "launch", app, "--config", config])` in `app_launcher`. Verify: `dia orchestrate` still starts the app. | Done | sonnet | Spec AC6 says "starts the app via `dia launch`". Current impl calls the exe directly, bypassing CLI path resolution and runtime deps. |
| 4 | Fix determinism test: extract and compare settle frame count | In `test_rigidbody2d_determinism`, extract a numeric field (e.g. `result["frame_count"]` or `result["settle_ticks"]`) from both checkpoint results and assert equality. Verify: test actually compares values, not just `passed`. | Done | haiku | Test currently asserts only that both runs pass — doesn't verify identical outcomes. |
| 5 | Make `poll_checkpoint` distinguish permanent vs transient errors | On `AutomationError`, check if message indicates "checkpoint not found" (transient — module loading) vs other errors (permanent — raise immediately). Verify: typo'd checkpoint name surfaces error quickly instead of timing out. | Done | sonnet | Currently swallows all errors until timeout. |
| 6 | Normalize port: plan default matches spec default | Change `plans/cluichetest/default.json` port from 9002 to 9876, or document why 9002 is intentional and update the spec/DiaClient default. Verify: `--no-launch` works without explicit `--port`. | Done | haiku | Spec and DiaClient default say 9876; plan says 9002. |
| 7 | Guard `_drain_welcome` against consuming real responses | Tag welcome messages by type; if a non-welcome message arrives during drain, push it into a replay buffer that `send_command` checks first. Verify: command sent immediately after connect still receives its response. | Done | sonnet | Low probability race but correctness matters. |
| 8 | Improve `navigate_to` error reporting on invalid target | If `send_command` succeeds but the poll loop times out, include the current stage in the error message so the user knows where the app is stuck. Verify: invalid target produces message like "Timed out navigating to 'Foo' (still on 'Boot')". | Done | haiku | Current error is generic TimeoutError with no context. |
