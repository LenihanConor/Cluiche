"""Generic stage runner — discovers all stages from the app and runs each one.

No per-stage scenario files needed. Adding a new stage to CluicheTest automatically
includes it in the E2E run.
"""
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

    for stage in stages:
        outcome = _run_stage(dia_client, stage, port)
        results[stage] = outcome

    summary = "\n".join(
        f"  {k}: {v['status']} — {v.get('detail', '')}" for k, v in results.items()
    )
    print(f"\n--- Stage Results ({len(stages)} stages) ---\n{summary}")

    failed = {k: v for k, v in results.items() if v["status"] != "passed"}
    if failed:
        pytest.fail(f"Stage failures:\n{summary}")


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
