"""E2E: DeferredStepTestStage run headless via `dia test e2e`; asserts on the session log (spec AC 33).

Requires a built CluicheTest (`dia run cluichetest --build-only`); the E2E harness does not build.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[4]
_SESSIONS = _REPO / "Cluiche" / "out" / "CluicheTest" / "sessions"
_K = 7
_KV = re.compile(r"(\w+)=(\S+)")


def _kv(msg: str) -> dict:
    return dict(_KV.findall(msg))


@pytest.fixture(scope="module")
def session_records():
    before = {p.name for p in _SESSIONS.iterdir()} if _SESSIONS.exists() else set()
    proc = subprocess.run(
        [sys.executable, "-m", "dia_cli", "test", "e2e", "--suite=cluichetest/deferredstep"],
        cwd=_REPO, capture_output=True, text=True, timeout=600,
    )
    assert proc.returncode == 0, f"dia test e2e failed (rc={proc.returncode}):\n{proc.stdout[-1500:]}\n{proc.stderr[-500:]}"
    new = sorted(p for p in _SESSIONS.iterdir() if p.name not in before)
    assert new, "no new session directory created"
    # The harness may relaunch the app; read every new session, oldest first.
    records = []
    for d in new:
        log = d / "log.jsonl"
        if log.exists():
            for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
                try:
                    records.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
    return records


@pytest.mark.integration
def test_no_error_level_records(session_records):
    errors = [r.get("msg", r) for r in session_records if str(r.get("level", "")).lower() == "error"]
    assert not errors, f"error-level records: {errors[:5]}"


@pytest.mark.integration
def test_result_and_steps(session_records):
    msgs = [r.get("msg", "") for r in session_records]
    results = [m for m in msgs if "DEFERREDSTEP_RESULT" in m]
    assert len(results) == 1, f"expected 1 RESULT line, got {len(results)}"
    res = _kv(results[0])
    steps = [_kv(m) for m in msgs if "DEFERREDSTEP_STEP" in m]

    assert int(res["k"]) == _K
    assert int(res["steps"]) == 20
    assert len(steps) == int(res["steps"])
    assert int(res["applies"]) == int(res["steps"])
    assert int(res["stalls"]) == 0
    assert res["expectedApplyKeysOk"] == "1"
    assert res["checksumOk"] == "1"
    assert res["passed"] == "1"
    # AC 30: SimPU frame time stays inside the stage budget (Run is on workers, not SimPU).
    assert float(res["maxStepMs"]) <= float(res["budgetMs"]), res
    assert float(res["meanStepMs"]) <= float(res["maxStepMs"]), res
    for s in steps:
        assert int(s["appliedAt"]) == int(s["apply"]) == int(s["kick"]) + _K, s
        assert s["checksum"] == s["expected"], s

    # Mid-step save + load (Feature 3, cross-feature E2E): the run must be indistinguishable from an uninterrupted one.
    assert res["saveLoad"] == "ok", res
    saved_step = int(res["saveLoadStep"])
    assert saved_step > 0, res
    assert res["applyOnceOk"] == "1", res
    by_index = [int(s["step"]) for s in steps]
    assert sorted(by_index) == list(range(int(res["steps"]))), f"one STEP line per step index expected: {by_index}"
    saved = next(s for s in steps if int(s["step"]) == saved_step)
    assert int(saved["appliedAt"]) == int(saved["kick"]) + _K, saved
    assert saved["checksum"] == saved["expected"], saved
    # One save and one load INFO line, both on the saved step.
    assert sum(1 for m in msgs if m.startswith("save slot=") and f"step={saved_step} " in m and "ok=1" in m) == 1
    assert sum(1 for m in msgs if m.startswith("load slot=") and f"step={saved_step} " in m and "ok=1" in m) == 1
