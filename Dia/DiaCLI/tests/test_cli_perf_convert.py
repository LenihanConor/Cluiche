"""Tests for dia perf convert (profile.jsonl -> Chrome-trace JSON)."""
from __future__ import annotations

import json
import os
import time
import tracemalloc
from pathlib import Path

import pytest
from click.testing import CliRunner

from dia_cli.commands.perf.convert_cmd import convert, convert_files
from dia_cli.commands.perf.group import perf_group

T0 = 1_747_484_400_000_000_000


def _scope(tid, name, start_off_ns, dur_ns, frame=1, **extra):
    rec = {"frame_number": frame, "thread_id": tid, "scope_name": name, "category": 2,
           "scope_id": 1, "parent_scope_id": 0, "start_unix_nano": T0 + start_off_ns,
           "duration_ns": dur_ns}
    rec.update(extra)
    return rec


def _write(path: Path, records, tail: str = "") -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for r in records:
            f.write(json.dumps(r) + "\n")
        f.write(tail)


def validate_chrome_trace(doc: dict) -> None:
    """Structural Chrome-trace check: required fields per ph, flows paired."""
    assert isinstance(doc["traceEvents"], list)
    flows = {}
    for ev in doc["traceEvents"]:
        ph = ev["ph"]
        assert ph in ("M", "X", "i", "s", "f"), ph
        assert isinstance(ev["pid"], int) and isinstance(ev["tid"], int)
        assert isinstance(ev["ts"], (int, float))
        assert isinstance(ev["name"], str)
        if ph == "M":
            assert ev["name"] in ("process_name", "thread_name")
            assert isinstance(ev["args"]["name"], str)
        elif ph == "X":
            assert ev["dur"] >= 0
        elif ph == "i":
            assert ev["s"] in ("g", "p", "t")
        else:
            assert "id" in ev
            flows.setdefault(ev["id"], []).append(ev)
            if ph == "f":
                assert ev["bp"] == "e"
    for fid, evs in flows.items():
        assert sorted(e["ph"] for e in evs) == ["f", "s"], fid


@pytest.fixture
def session(tmp_path):
    d = tmp_path / "session"
    d.mkdir()
    recs = [
        {"record_type": "thread_name", "thread_id": 1, "name": "Sim"},
        {"record_type": "thread_name", "thread_id": 12, "name": "Worker 1"},
        _scope(1, 111, 0, 5_000_000, subject="ecology"),
        _scope(2, 222, 1_000_000, 2_000_000),
        _scope(12, "job.run", 3_000_000, 1_000_000, subject="ecology"),
        _scope(1, 111, 2_000_000_000, 5_000_000, frame=2),
        {"record_type": "profile_job", "thread_id": 12, "submit_thread": 1, "owner": "ecology",
         "tag": 4, "range": 0, "range_count": 1, "submit_unix_nano": T0 + 500_000,
         "start_unix_nano": T0 + 3_000_000, "end_unix_nano": T0 + 4_000_000},
        {"record_type": "profile_job", "thread_id": 12, "submit_thread": 1, "owner": "0xBEEF",
         "tag": 5, "range": 0, "range_count": 1, "submit_unix_nano": T0 + 2_000_500_000,
         "start_unix_nano": T0 + 2_001_000_000, "end_unix_nano": T0 + 2_002_000_000},
        {"record_type": "future_thing", "x": 1},
    ]
    _write(d / "profile.jsonl", recs)
    _write(d / "trips.jsonl", [{"unit": "SimPU", "start_unix_nano": T0 + 1_000_000_000,
                                "peak_ms": 40.0, "budget_ms": 16.6}])
    return d


def _load(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _by_ph(doc, ph):
    return [e for e in doc["traceEvents"] if e["ph"] == ph]


def test_session_dir_full(session, tmp_path):
    out = tmp_path / "t.json"
    stats = convert_files(session / "profile.jsonl", session / "trips.jsonl", out)
    doc = _load(out)
    validate_chrome_trace(doc)
    assert stats["skipped"] == 0
    names = {e["tid"]: e["args"]["name"] for e in _by_ph(doc, "M") if e["name"] == "thread_name"}
    assert names == {1: "Sim", 12: "Worker 1", 2: "Thread 2"}
    assert len(_by_ph(doc, "X")) == 4
    assert len({e["tid"] for e in _by_ph(doc, "X")}) == 3
    xs = [e for e in _by_ph(doc, "X") if e["tid"] == 12]
    assert xs[0]["args"]["subject"] == "ecology" and xs[0]["name"] == "job.run"
    assert xs[0]["dur"] == 1000.0
    assert _by_ph(doc, "X")[0]["name"] == "0x0000006F"  # CRC scope names pass through as hex
    # flows: 2 jobs, s on submit thread, f on worker thread
    s, f = _by_ph(doc, "s"), _by_ph(doc, "f")
    assert len(s) == len(f) == 2
    assert {e["tid"] for e in s} == {1} and {e["tid"] for e in f} == {12}
    assert len({e["id"] for e in s}) == 2
    assert any(e["name"] == "job 0xBEEF" for e in s)  # unknown owner hex passes through
    inst = _by_ph(doc, "i")
    assert len(inst) == 1 and inst[0]["name"] == "trip SimPU"


def test_cli_session_dir_default_out(session):
    r = CliRunner().invoke(perf_group, ["convert", str(session)])
    assert r.exit_code == 0, r.output
    validate_chrome_trace(_load(session / "trace.json"))


def test_cli_profile_file_and_out(session, tmp_path):
    out = tmp_path / "sub" / "o.json"
    r = CliRunner().invoke(convert, [str(session / "profile.jsonl"), "--out", str(out)])
    assert r.exit_code == 0, r.output
    doc = _load(out)
    validate_chrome_trace(doc)
    assert len(_by_ph(doc, "i")) == 1  # trips.jsonl sibling discovered


def test_cli_missing_profile(tmp_path):
    r = CliRunner().invoke(convert, [str(tmp_path)])
    assert r.exit_code != 0


def test_from_to_window(session, tmp_path):
    out = tmp_path / "w.json"
    convert_files(session / "profile.jsonl", session / "trips.jsonl", out, t_from=1.5, t_to=3.0)
    doc = _load(out)
    validate_chrome_trace(doc)
    xs = _by_ph(doc, "X")
    assert len(xs) == 1 and xs[0]["args"]["frame_number"] == 2
    assert len(_by_ph(doc, "s")) == 1
    assert _by_ph(doc, "i") == []

    out2 = tmp_path / "w2.json"
    convert_files(session / "profile.jsonl", session / "trips.jsonl", out2, t_from=0.5, t_to=1.5)
    doc2 = _load(out2)
    validate_chrome_trace(doc2)
    assert _by_ph(doc2, "X") == [] and len(_by_ph(doc2, "i")) == 1


def test_truncated_and_garbled_lines(tmp_path):
    p = tmp_path / "profile.jsonl"
    _write(p, [_scope(1, 5, 0, 1000)], tail='\nnot json\n[1,2]\n{"frame_number":1,"thread_id":1,"sc')
    out = tmp_path / "o.json"
    stats = convert_files(p, None, out)
    validate_chrome_trace(_load(out))
    assert stats["skipped"] == 3
    assert len(_by_ph(_load(out), "X")) == 1
    r = CliRunner().invoke(convert, [str(p), "--out", str(out)])
    assert r.exit_code == 0 and "skipped 3" in r.output


def test_empty_file(tmp_path):
    p = tmp_path / "profile.jsonl"
    p.write_text("")
    out = tmp_path / "o.json"
    stats = convert_files(p, None, out, t_from=1, t_to=2)
    doc = _load(out)
    validate_chrome_trace(doc)
    assert _by_ph(doc, "X") == [] and stats["skipped"] == 0


def test_real_format_without_record_type(tmp_path):
    """Today's engine format: no record_type, numeric scope_name."""
    p = tmp_path / "profile.jsonl"
    p.write_text('{"frame_number":1,"thread_id":3,"scope_name":123456,"category":1,'
                 '"scope_id":1,"parent_scope_id":0,"start_unix_nano":%d,"duration_ns":900}\n' % T0)
    out = tmp_path / "o.json"
    convert_files(p, None, out)
    doc = _load(out)
    validate_chrome_trace(doc)
    assert len(_by_ph(doc, "X")) == 1


@pytest.mark.skipif(not os.environ.get("DIA_RUN_SLOW"),
                    reason="slow (AB5): set DIA_RUN_SLOW=1 to run")
def test_ab5_200mb_bounded_memory(tmp_path, capsys):
    p = tmp_path / "profile.jsonl"
    target = 200 * 1024 * 1024
    batch = []
    n = 0
    with open(p, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps({"record_type": "thread_name", "thread_id": 1, "name": "Sim"}) + "\n")
        written = 0
        while written < target:
            batch.clear()
            for _ in range(5000):
                n += 1
                batch.append(json.dumps(_scope(1 + n % 4, 1000 + n % 50, n * 1000, 800, frame=n // 100)))
            s = "\n".join(batch) + "\n"
            f.write(s)
            written += len(s)
    size = p.stat().st_size
    assert size >= target
    out = tmp_path / "o.json"
    tracemalloc.start()
    t = time.time()
    stats = convert_files(p, None, out)
    secs = time.time() - t
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    with capsys.disabled():
        print(f"\nAB5: input {size / 1e6:.0f} MB, {stats['events']} events, "
              f"peak {peak / 1e6:.1f} MB (tracemalloc), {secs:.1f} s")
    assert peak < 500 * 1024 * 1024
    assert stats["events"] > n


def test_engine_produced_profile_converts(tmp_path):
    """Real engine-path profile.jsonl (written by the C++ test
    SLOW_JobObserverTest.Integration_WritesRealEngineProfileForConverter).
    Set DIA_ENGINE_PROFILE_FIXTURE to the file; skipped when absent."""
    src = os.environ.get("DIA_ENGINE_PROFILE_FIXTURE")
    if not src or not Path(src).is_file():
        pytest.skip("engine-produced profile.jsonl fixture not present")
    out = tmp_path / "engine.json"
    stats = convert_files(Path(src), None, out)
    doc = json.loads(out.read_text(encoding="utf-8"))
    validate_chrome_trace(doc)
    evs = doc["traceEvents"]
    names = {e["args"]["name"] for e in evs if e["ph"] == "M" and e["name"] == "thread_name"}
    assert {"Worker 0", "Worker 1", "Worker 2"} <= names
    assert stats["skipped"] == 0 and stats["jobs"] == 24
    assert sum(1 for e in evs if e["ph"] == "s") == sum(1 for e in evs if e["ph"] == "f") == 24
    assert any(e["ph"] == "X" and e["args"].get("subject") == "ecology" for e in evs)


def test_trace_linked_streams_emit_flow_events(tmp_path):
    """Task 6: spans with link_trace_id and link_span_id emit Perfetto flow events."""
    p = tmp_path / "profile.jsonl"
    # Writer span (thread 1) with no link
    writer = _scope(1, "Write", 0, 5_000_000)
    # Reader span (thread 2) with link to writer (hex 16-char trace/span ids)
    reader = _scope(2, "Read", 10_000_000, 2_000_000)
    reader["link_trace_id"] = "00000000deadbeef"
    reader["link_span_id"] = "0000000012345678"
    # Another reader without a link (no flow events)
    reader2 = _scope(3, "Read2", 20_000_000, 1_000_000)
    # Reader with zero link (no flow events)
    reader3 = _scope(4, "Read3", 30_000_000, 1_000_000)
    reader3["link_trace_id"] = "0000000000000000"
    reader3["link_span_id"] = "0000000000000000"
    _write(p, [writer, reader, reader2, reader3])
    out = tmp_path / "o.json"
    stats = convert_files(p, None, out)
    doc = _load(out)
    validate_chrome_trace(doc)
    # 4 complete events (X)
    xs = _by_ph(doc, "X")
    assert len(xs) == 4
    # 1 flow event pair (s and f) for the reader with non-zero link
    s_evs = _by_ph(doc, "s")
    f_evs = _by_ph(doc, "f")
    assert len(s_evs) == 1 and len(f_evs) == 1
    assert s_evs[0]["id"] == f_evs[0]["id"]
    assert s_evs[0]["name"] == "stream" and f_evs[0]["name"] == "stream"
    assert s_evs[0]["cat"] == "flow" and f_evs[0]["cat"] == "flow"
    assert f_evs[0]["bp"] == "e"
    # Flow finish is at the reader span start; flow start is slightly before
    reader_ev = [e for e in xs if e["name"] == "Read"][0]
    assert s_evs[0]["ts"] < f_evs[0]["ts"] == reader_ev["ts"]
