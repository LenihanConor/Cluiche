"""Unit tests for the E2E pytest plugin's implicit log-error scan."""
import importlib.util
import json
import sys
import warnings
from pathlib import Path
from types import ModuleType

import pytest

_PLUGIN = Path(__file__).resolve().parents[3] / "Cluiche" / "Tests" / "E2E" / "plugin.py"


@pytest.fixture(scope="module")
def plugin():
    saved = sys.modules.get("client")
    stub = ModuleType("client")
    stub.DiaClient = object
    sys.modules["client"] = stub
    try:
        spec = importlib.util.spec_from_file_location("e2e_plugin_under_test", _PLUGIN)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
    finally:
        if saved is None:
            sys.modules.pop("client", None)
        else:
            sys.modules["client"] = saved
    return mod


def _rec(level, msg):
    return json.dumps({"level": level, "msg": msg}).encode("utf-8")


def _use_log(plugin, monkeypatch, path):
    monkeypatch.setattr(plugin, "_find_log_path", lambda config, plan: path)


def test_valid_log_collects_only_errors(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(_rec("info", "ok") + b"\n" + _rec("ERROR", "boom") + b"\n")
    _use_log(plugin, monkeypatch, log)
    assert plugin._collect_log_errors(None, {}) == ["boom"]


def test_skip_lines_honoured(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(_rec("error", "old") + b"\n" + _rec("error", "new") + b"\n")
    _use_log(plugin, monkeypatch, log)
    assert plugin._collect_log_errors(None, {}, skip_lines=1) == ["new"]


def test_non_utf8_byte_does_not_hide_errors(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    # 0x97 is a cp1252 em dash and invalid as a UTF-8 start byte.
    bad = b'{"level": "info", "msg": "a \x97 b"}'
    log.write_bytes(bad + b"\n" + _rec("error", "still found") + b"\n")
    _use_log(plugin, monkeypatch, log)
    assert plugin._collect_log_errors(None, {}) == ["still found"]


def test_malformed_line_skipped_and_reported(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(
        _rec("error", "first") + b"\n{not json\n" + _rec("error", "second") + b"\n"
    )
    _use_log(plugin, monkeypatch, log)
    with pytest.warns(RuntimeWarning, match="malformed"):
        errors = plugin._collect_log_errors(None, {})
    assert errors == ["first", "second"]


def test_unreadable_log_is_surfaced(plugin, monkeypatch, tmp_path):
    missing = tmp_path / "gone" / "log.jsonl"
    _use_log(plugin, monkeypatch, missing)
    errors = plugin._collect_log_errors(None, {})
    assert len(errors) == 1
    assert "Could not read session log" in errors[0]


def test_no_log_path_returns_empty(plugin, monkeypatch):
    _use_log(plugin, monkeypatch, None)
    with warnings.catch_warnings():
        warnings.simplefilter("error")
        assert plugin._collect_log_errors(None, {}) == []


def _locked_read(monkeypatch, plugin, exc, fail_times):
    calls = {"n": 0}
    real = Path.read_text

    def fake(self, *a, **k):
        if self.name == "log.jsonl":
            calls["n"] += 1
            if calls["n"] <= fail_times:
                raise exc
        return real(self, *a, **k)

    monkeypatch.setattr(Path, "read_text", fake)
    monkeypatch.setattr(plugin.time, "sleep", lambda s: None)
    return calls


def test_lock_released_during_retry_still_scans(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(_rec("error", "boom") + b"\n")
    _use_log(plugin, monkeypatch, log)
    calls = _locked_read(monkeypatch, plugin, PermissionError(13, "denied"), 2)
    assert plugin._collect_log_errors(None, {}, lock_retry_s=5.0) == ["boom"]
    assert calls["n"] == 3


def test_persistent_lock_warns_not_fails(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(_rec("error", "boom") + b"\n")
    _use_log(plugin, monkeypatch, log)
    _locked_read(monkeypatch, plugin, PermissionError(13, "denied"), 10**9)
    ticks = iter(range(0, 10**6))
    monkeypatch.setattr(plugin.time, "monotonic", lambda: next(ticks) * 0.5)
    with pytest.warns(RuntimeWarning, match="still locked"):
        assert plugin._collect_log_errors(None, {}, lock_retry_s=2.0) == []


def test_non_lock_oserror_still_fails(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(b"")
    _use_log(plugin, monkeypatch, log)
    _locked_read(monkeypatch, plugin, OSError(5, "io error"), 10**9)
    errors = plugin._collect_log_errors(None, {})
    assert len(errors) == 1 and "Could not read session log" in errors[0]


def test_scan_log_reports_locked_without_waiting(plugin, monkeypatch, tmp_path):
    log = tmp_path / "log.jsonl"
    log.write_bytes(b"")
    _use_log(plugin, monkeypatch, log)
    _locked_read(monkeypatch, plugin, PermissionError(13, "denied"), 10**9)
    assert plugin._scan_log(None, {}, lock_retry_s=0.0) == ([], True)
