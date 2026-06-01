"""Generic stage runner — discovers all stages from the app and runs each one.

No per-stage scenario files needed. Adding a new stage to CluicheTest automatically
includes it in the E2E run.
"""
import json
import time
from pathlib import Path

import pytest

from client import AutomationError, DiaClient


_STAGE_TIMEOUT_S = 30.0
_NAVIGATE_TIMEOUT_S = 15.0


def test_discover_stages(dia_client):
    """Verify the app reports at least one navigable stage."""
    found = dia_client.list_stages()
    assert len(found) > 0, "No stages discovered from dia.manifest.stages"


def test_run_all_stages(dia_client, request):
    """Navigate to every stage, wait for its checkpoints, then return to Boot.

    A stage that times out or errors is recorded as failed but does NOT
    abort the run — the next stage still executes.
    """
    plan = request.config._dia_plan
    port = plan.get("port", 9002)

    stages = dia_client.list_stages()
    results = {}
    run_start = time.time()

    for stage in stages:
        t0 = time.time()
        outcome = _run_stage(dia_client, stage, port)
        outcome["duration_s"] = round(time.time() - t0, 2)
        results[stage] = outcome

    total_s = round(time.time() - run_start, 2)

    # Always print the summary (visible with -s or on failure)
    summary_lines = []
    for k, v in results.items():
        status_icon = "PASS" if v["status"] == "passed" else "FAIL"
        summary_lines.append(
            f"  [{status_icon}] {k} ({v['duration_s']}s) — {v.get('detail', '')}"
        )
    summary = "\n".join(summary_lines)
    print(f"\n{'=' * 60}")
    print(f"  E2E Stage Run: {len(stages)} stages in {total_s}s")
    print(f"{'=' * 60}")
    print(summary)
    print(f"{'=' * 60}\n")

    # Write machine-readable JSON report
    _write_report(request, results, total_s)

    failed = {k: v for k, v in results.items() if v["status"] != "passed"}
    if failed:
        pytest.fail(f"Stage failures ({len(failed)}/{len(stages)}):\n{summary}")


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

    # Poll each checkpoint
    all_passed = True
    messages = []
    for cp in checkpoints:
        try:
            result = dia_client.poll_checkpoint(cp, timeout_s=_STAGE_TIMEOUT_S, interval_s=0.5)
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
            messages.append(f"{cp}: timeout after {_STAGE_TIMEOUT_S}s")
        except Exception as e:
            all_passed = False
            messages.append(f"{cp}: connection error — {e}")
            if not _try_reconnect(dia_client, port):
                break

    _try_return_boot(dia_client)

    detail = "; ".join(messages)
    if all_passed:
        return {"status": "passed", "detail": detail}
    return {"status": "failed", "detail": detail}


def _try_return_boot(dia_client):
    """Best-effort navigate back to Boot."""
    try:
        dia_client.navigate_to("Boot", timeout_s=_NAVIGATE_TIMEOUT_S)
    except Exception:
        pass


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
        "failed": sum(1 for v in results.values() if v["status"] != "passed"),
        "stages": {k: v for k, v in results.items()},
    }

    report_path = report_dir / f"e2e_{time.strftime('%Y%m%d_%H%M%S')}.json"
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
