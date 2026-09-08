"""Generic stage runner — discovers all stages from the app and runs each one.

No per-stage scenario files needed. Adding a new stage to CluicheTest automatically
includes it in the E2E run.
"""
import json
import time
from pathlib import Path

import pytest

from client import AutomationError, DiaClient


_STAGE_TIMEOUT_S = 60.0
_NAVIGATE_TIMEOUT_S = 60.0


def test_discover_stages(dia_client):
    """Verify the app reports at least one navigable stage."""
    found = dia_client.list_stages()
    assert len(found) > 0, "No stages discovered from dia.manifest.stages"


def test_run_all_stages(dia_client, request):
    """Navigate to every stage, wait for its checkpoints, then return to Boot.

    A stage that times out or errors is recorded as failed but does NOT
    abort the run — the next stage still executes.

    Stages listed in plan["generic_run_skip"] are excluded — use this for stages
    that have dedicated scenario files or are known to crash the app under the
    generic runner (e.g. stages that require native plugin initialisation).
    """
    plan = request.config._dia_plan
    port = plan.get("port", 9002)
    skip_raw = plan.get("generic_run_skip", {})
    # Accept both list (legacy) and dict (stage → reason)
    if isinstance(skip_raw, list):
        skip_map = {s: "excluded from generic runner" for s in skip_raw}
    else:
        skip_map = skip_raw

    all_stages = dia_client.list_stages()
    results = {}
    run_start = time.time()

    for stage in all_stages:
        if stage in skip_map:
            results[stage] = {"status": "skipped", "detail": skip_map[stage], "duration_s": 0}

    stages = [s for s in all_stages if s not in skip_map]
    for stage in stages:
        t0 = time.time()
        outcome = _run_stage(dia_client, stage, port)
        outcome["duration_s"] = round(time.time() - t0, 2)
        results[stage] = outcome
        # If the app is stuck (won't return to Boot) and we couldn't reconnect,
        # mark all remaining stages as aborted and stop the loop.
        if outcome["status"] == "stuck" and "connection lost" in outcome.get("detail", ""):
            for remaining in stages[stages.index(stage) + 1:]:
                results[remaining] = {"status": "aborted", "detail": "prior stage left app stuck", "duration_s": 0}
            break

    total_s = round(time.time() - run_start, 2)

    # Always print the summary (visible with -s or on failure)
    summary_lines = []
    for k, v in results.items():
        if v["status"] == "passed":
            icon = "PASS"
        elif v["status"] == "skipped":
            icon = "SKIP"
        else:
            icon = "FAIL"
        summary_lines.append(
            f"  [{icon}] {k} ({v['duration_s']}s) — {v.get('detail', '')}"
        )
    summary = "\n".join(summary_lines)
    n_run = len(stages)
    n_skip = len(skip_map)
    print(f"\n{'=' * 60}")
    print(f"  E2E Stage Run: {n_run} run, {n_skip} skipped, {total_s}s total")
    print(f"{'=' * 60}")
    print(summary)
    print(f"{'=' * 60}\n")

    # Write machine-readable JSON report
    _write_report(request, results, total_s)

    failed = {k: v for k, v in results.items() if v["status"] not in ("passed", "skipped")}
    if failed:
        pytest.fail(f"Stage failures ({len(failed)}/{n_run}):\n{summary}")


def _run_stage(dia_client, stage: str, port: int) -> dict:
    """Run a single stage: navigate, discover checkpoints, poll them, return to Boot."""
    try:
        dia_client.navigate_to(stage, timeout_s=_NAVIGATE_TIMEOUT_S)
    except TimeoutError as e:
        _try_return_boot(dia_client)
        return {"status": "navigate_timeout", "detail": str(e)}
    except AutomationError as e:
        _try_return_boot(dia_client)
        return {"status": "navigate_error", "detail": str(e)}
    except Exception as e:
        if not _try_reconnect(dia_client, port):
            return {"status": "crash", "detail": f"connection lost navigating to {stage}: {e}"}
        _try_return_boot(dia_client)
        return {"status": "crash", "detail": f"app dropped connection: {e}"}

    # Discover checkpoints registered for this stage
    try:
        checkpoints = dia_client.list_checkpoints()
    except Exception:
        checkpoints = []

    if not checkpoints:
        _try_return_boot(dia_client)
        return {"status": "passed", "detail": "no checkpoints (presence-only)"}

    # All checkpoints share one stage-level deadline.
    # This prevents N-checkpoint stages from spending N × _STAGE_TIMEOUT_S.
    stage_deadline = time.time() + _STAGE_TIMEOUT_S
    all_passed = True
    messages = []
    for cp in checkpoints:
        remaining = stage_deadline - time.time()
        if remaining <= 0:
            all_passed = False
            messages.append(f"{cp}: stage budget exhausted")
            continue

        try:
            result = dia_client.poll_checkpoint(cp, timeout_s=remaining, interval_s=0.5,
                                                expected_stage=stage)
            if result.get("passed"):
                messages.append(f"{cp}: passed")
            else:
                all_passed = False
                messages.append(f"{cp}: {result.get('message', 'failed')}")
        except AutomationError as e:
            all_passed = False
            messages.append(f"{cp}: error — {e}")
        except TimeoutError:
            all_passed = False
            messages.append(f"{cp}: timeout after {_STAGE_TIMEOUT_S:.0f}s")
        except Exception as e:
            all_passed = False
            messages.append(f"{cp}: connection error — {e}")
            if not _try_reconnect(dia_client, port):
                break

    if not _try_return_boot(dia_client):
        if not _try_reconnect(dia_client, port):
            return {"status": "stuck", "detail": "app did not return to Boot and connection lost; " + "; ".join(messages)}
        return {"status": "stuck", "detail": "app did not return to Boot within timeout; " + "; ".join(messages)}

    detail = "; ".join(messages)
    if all_passed:
        return {"status": "passed", "detail": detail}
    return {"status": "failed", "detail": detail}


def _try_return_boot(dia_client) -> bool:
    """Return to Boot. C++ auto-releases navigation hold on resolve/timeout,
    so navigate_to("Boot") is usually a fast-path (already there or event buffered).
    Only send abort_stage() if the app is still on a non-Boot stage — sending it
    unconditionally creates a stale abort that lands on the *next* stage's DoUpdate.
    """
    try:
        current = dia_client.report().get("stage")
        if current != "Boot":
            dia_client.abort_stage()
    except Exception:
        dia_client.abort_stage()  # can't tell — send it anyway
    try:
        dia_client.navigate_to("Boot", timeout_s=_NAVIGATE_TIMEOUT_S)
        return True
    except Exception:
        return False


def _try_reconnect(dia_client, port: int) -> bool:
    """Attempt to reconnect after a connection drop."""
    try:
        dia_client.disconnect()
        dia_client.connect(timeout=10.0)
        return True
    except Exception:
        return False


def _write_report(request, results: dict, total_s: float):
    """Write a JSON report alongside the session log for CI consumption."""
    repo_root = getattr(request.config, "_dia_repo_root", None)
    if not repo_root:
        return

    report_dir = Path(repo_root) / "Cluiche" / "out" / "e2e_reports"
    report_dir.mkdir(parents=True, exist_ok=True)

    report = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "total_duration_s": total_s,
        "stage_count": len(results),
        "passed": sum(1 for v in results.values() if v["status"] == "passed"),
        "skipped": sum(1 for v in results.values() if v["status"] == "skipped"),
        "failed": sum(1 for v in results.values() if v["status"] not in ("passed", "skipped")),
        "stages": {k: v for k, v in results.items()},
    }

    report_path = report_dir / f"e2e_{time.strftime('%Y%m%d_%H%M%S')}.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
