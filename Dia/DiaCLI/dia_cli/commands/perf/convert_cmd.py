"""dia perf convert -- profile.jsonl (+ trips.jsonl) to Chrome-trace JSON.

Streaming: the input is read line by line and the output array is written
incrementally, so memory stays bounded regardless of session size.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Iterator, Optional, Tuple

import click

_PID = 1
_TS_KEYS = ("unix_nano", "timestamp_unix_nano", "time_unix_nano", "start_unix_nano", "ts_unix_nano")
_MARKER_TYPES = ("perf_marker", "marker")
_TRIP_TYPES = ("trip", "pu_trip", "perf_trip")


def _dumps(obj) -> str:
    return json.dumps(obj, separators=(",", ":"))


def _us(ns) -> float:
    return round(ns / 1000.0, 3)


def _as_int(v, default=0) -> int:
    try:
        return int(v)
    except (TypeError, ValueError):
        return default


def _name_of(v) -> str:
    if isinstance(v, str):
        return v
    if isinstance(v, int):
        return "0x%08X" % v
    return str(v)


def _record_ts(rec: dict) -> Optional[int]:
    for k in _TS_KEYS:
        if isinstance(rec.get(k), (int, float)):
            return int(rec[k])
    return None


def _iter_records(path: Path, stats: dict) -> Iterator[dict]:
    """Yield dict records, counting garbled lines in stats['skipped']."""
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rec = json.loads(line)
            except ValueError:
                stats["skipped"] += 1
                continue
            if not isinstance(rec, dict):
                stats["skipped"] += 1
                continue
            yield rec


def resolve_inputs(src: Path) -> Tuple[Path, Optional[Path]]:
    """Return (profile.jsonl, trips.jsonl or None) for a session dir or a file."""
    if src.is_dir():
        profile = src / "profile.jsonl"
        if not profile.is_file():
            raise click.ClickException(f"No profile.jsonl in {src}")
        trips = src / "trips.jsonl"
        return profile, (trips if trips.is_file() else None)
    if not src.is_file():
        raise click.ClickException(f"Not found: {src}")
    trips = src.parent / "trips.jsonl"
    return src, (trips if trips.is_file() else None)


def _first_timestamp(profile: Path, trips: Optional[Path]) -> Optional[int]:
    """Cheap pre-pass: first timestamp found (reads only until one is seen)."""
    scratch = {"skipped": 0}
    for p in (profile, trips):
        if p is None:
            continue
        for rec in _iter_records(p, scratch):
            ts = _record_ts(rec)
            if ts is None and "submit_unix_nano" in rec:
                ts = _as_int(rec["submit_unix_nano"])
            if ts is not None:
                return ts
    return None


class _Writer:
    def __init__(self, out):
        self.out = out
        self.first = True
        self.count = 0

    def event(self, ev: dict) -> None:
        self.out.write(("" if self.first else ",\n") + _dumps(ev))
        self.first = False
        self.count += 1


def convert_files(profile: Path, trips: Optional[Path], out_path: Path,
                  t_from: Optional[float] = None, t_to: Optional[float] = None) -> dict:
    """Convert; returns stats (events, skipped, threads, jobs, trips, markers)."""
    stats = {"skipped": 0, "events": 0, "threads": 0, "jobs": 0, "trips": 0, "markers": 0}
    base = _first_timestamp(profile, trips)
    lo = hi = None
    if base is not None:
        if t_from is not None:
            lo = base + int(t_from * 1e9)
        if t_to is not None:
            hi = base + int(t_to * 1e9)

    def in_window(ts_ns: int) -> bool:
        return (lo is None or ts_ns >= lo) and (hi is None or ts_ns <= hi)

    seen_threads = set()
    named = set()
    explicit = set()
    flow_id = 0

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as out:
        out.write('{"displayTimeUnit":"ms","traceEvents":[\n')
        w = _Writer(out)
        w.event({"ph": "M", "pid": _PID, "tid": 0, "ts": 0, "name": "process_name",
                 "args": {"name": "Cluiche"}})

        def name_thread(tid: int, name: str) -> None:
            if (tid, name) in named:
                return
            named.add((tid, name))
            w.event({"ph": "M", "pid": _PID, "tid": tid, "ts": 0, "name": "thread_name",
                     "args": {"name": name}})

        def handle_instant(rec: dict, default_name: str, cat: str) -> None:
            ts = _record_ts(rec)
            if ts is None or not in_window(ts):
                return
            tid = _as_int(rec.get("thread_id"), 0)
            name = rec.get("name") or rec.get("label") or rec.get("text")
            if name is None and rec.get("unit"):
                name = f"{default_name} {rec['unit']}"
            args = {k: v for k, v in rec.items()
                    if isinstance(v, (str, int, float, bool)) and k != "record_type"}
            w.event({"ph": "i", "s": "g" if tid == 0 else "t", "pid": _PID, "tid": tid,
                     "ts": _us(ts), "name": str(name or default_name), "cat": cat, "args": args})

        def handle(rec: dict, from_trips: bool) -> None:
            nonlocal flow_id
            rtype = rec.get("record_type")
            if rtype is None and from_trips:
                rtype = "trip"
            if rtype is None or rtype == "profile_scope":
                if "start_unix_nano" not in rec or "duration_ns" not in rec:
                    return
                tid = _as_int(rec.get("thread_id"))
                start = _as_int(rec["start_unix_nano"])
                if not in_window(start):
                    return
                seen_threads.add(tid)
                args = {"category": rec.get("category"), "frame_number": rec.get("frame_number")}
                if rec.get("subject") not in (None, "", 0):
                    args["subject"] = _name_of(rec["subject"])
                w.event({"ph": "X", "pid": _PID, "tid": tid, "ts": _us(start),
                         "dur": _us(_as_int(rec["duration_ns"])),
                         "name": _name_of(rec.get("scope_name", rec.get("name", "?"))),
                         "cat": str(rec.get("category", "")), "args": args})
                # Emit flow events when link fields are present (Task 6: trace-linked streams)
                link_trace = rec.get("link_trace_id")
                link_span = rec.get("link_span_id")
                if link_trace and link_span and link_trace != "0" * 16 and link_span != "0" * 16:
                    flow_id += 1
                    # Flow start: emitted at the current span's start (writer span time unknown)
                    # In a real implementation with full trace data, we'd emit "s" at the writer
                    # span's end time. Here we emit it just before the reader span as a visual cue.
                    # Use 1 ns before to ensure it's visible after rounding to 3 decimal places.
                    w.event({"ph": "s", "id": flow_id, "pid": _PID, "tid": tid, "ts": _us(start - 1000),
                             "name": "stream", "cat": "flow"})
                    # Flow finish: at this span's start
                    w.event({"ph": "f", "bp": "e", "id": flow_id, "pid": _PID, "tid": tid,
                             "ts": _us(start), "name": "stream", "cat": "flow"})
            elif rtype == "thread_name":
                tid = _as_int(rec.get("thread_id"))
                explicit.add(tid)
                seen_threads.add(tid)
                name_thread(tid, str(rec.get("name", f"Thread {tid}")))
            elif rtype == "profile_job":
                start = _as_int(rec.get("start_unix_nano"), -1)
                submit = _as_int(rec.get("submit_unix_nano"), -1)
                if start < 0 or submit < 0 or not in_window(start):
                    return
                wtid = _as_int(rec.get("thread_id"))
                stid = _as_int(rec.get("submit_thread"))
                seen_threads.update((wtid, stid))
                flow_id += 1
                stats["jobs"] += 1
                nm = "job " + _name_of(rec.get("owner", "?"))
                w.event({"ph": "s", "id": flow_id, "pid": _PID, "tid": stid, "ts": _us(submit),
                         "name": nm, "cat": "job"})
                w.event({"ph": "f", "bp": "e", "id": flow_id, "pid": _PID, "tid": wtid,
                         "ts": _us(start), "name": nm, "cat": "job"})
            elif rtype in _MARKER_TYPES:
                stats["markers"] += 1
                handle_instant(rec, "marker", "perf.marker")
            elif rtype in _TRIP_TYPES:
                stats["trips"] += 1
                handle_instant(rec, "trip", "perf.trip")
            # unknown record types are ignored

        for rec in _iter_records(profile, stats):
            handle(rec, False)
        if trips is not None:
            for rec in _iter_records(trips, stats):
                handle(rec, True)

        for tid in sorted(seen_threads - explicit):
            name_thread(tid, f"Thread {tid}")
        stats["threads"] = len(seen_threads)
        out.write("\n]}\n")
        stats["events"] = w.count
    return stats


@click.command("convert")
@click.argument("source", type=click.Path(path_type=Path))
@click.option("--out", "out_path", type=click.Path(path_type=Path), default=None,
              help="Output trace file (default: trace.json next to the input).")
@click.option("--from", "t_from", type=float, default=None,
              help="Start of window, seconds after the first timestamp.")
@click.option("--to", "t_to", type=float, default=None,
              help="End of window, seconds after the first timestamp.")
def convert(source: Path, out_path: Optional[Path], t_from, t_to):
    """Convert a session dir or profile.jsonl to Chrome-trace JSON (Perfetto).

    One track per thread, scopes as complete events, jobs as flow arrows from
    the submitting thread, trips and markers as instant events.
    """
    profile, trips = resolve_inputs(source)
    if out_path is None:
        out_path = profile.parent / "trace.json"
    stats = convert_files(profile, trips, out_path, t_from, t_to)
    click.echo(f"[dia perf] wrote {out_path} ({stats['events']} events, "
               f"{stats['threads']} threads, {stats['jobs']} jobs, {stats['trips']} trips)")
    if stats["skipped"]:
        click.echo(f"WARNING: skipped {stats['skipped']} unparseable line(s)", err=True)
