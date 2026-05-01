import io
import json
import sys
import threading
import time
from datetime import datetime
from pathlib import Path
from typing import Optional

from rich.console import Console
from rich.style import Style
from rich.text import Text

# Force UTF-8 on Windows terminals that default to CP1252
if sys.stdout.encoding and sys.stdout.encoding.lower() not in ("utf-8", "utf-8-sig"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
if sys.stderr.encoding and sys.stderr.encoding.lower() not in ("utf-8", "utf-8-sig"):
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")


_STYLE_STARTED   = Style(color="white")
_STYLE_COMPLETED = Style(color="green")
_STYLE_FAILED    = Style(color="red", bold=True)
_STYLE_WARNING   = Style(color="yellow")
_STYLE_SKIPPED   = Style(dim=True)
_STYLE_SUMMARY   = Style(color="green", bold=True)
_STYLE_SUMMARY_F = Style(color="red", bold=True)
_STYLE_DIM       = Style(dim=True)

_SCHEMA_VERSION = "dia.output.v1"


class OutputContext:
    """Shared output layer for all DiaCLI commands.

    Writes structured terminal output via rich and NDJSON events to
    Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson in real time.
    Thread-safe. One instance per CLI invocation.
    """

    def __init__(
        self,
        log_dir: Path,
        no_color: bool = False,
        quiet: bool = False,
        log_json_override: Optional[Path] = None,
    ) -> None:
        self._log_dir = log_dir
        self._no_color = no_color
        self._quiet = quiet
        self._log_json_override = log_json_override
        self._lock = threading.Lock()
        self._log_file = None
        self._console = Console(
            highlight=False,
            markup=False,
            no_color=no_color,
        )
        # Maps (system, stage, step_or_None) -> start time float
        self._start_times: dict = {}

    # ------------------------------------------------------------------ #
    # Public event API                                                     #
    # ------------------------------------------------------------------ #

    def run_started(self, system: str, **kwargs) -> None:
        payload = {"event": "OnRunStarted", "schema": _SCHEMA_VERSION, "system": system, **kwargs}
        self._open_log(system)
        self._record_start(system, None, None)
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}] ▶ {system} run started", style=_STYLE_STARTED))

    def stage_started(self, system: str, stage: str) -> None:
        payload = {"event": "OnStageStarted", "system": system, "stage": stage}
        self._record_start(system, stage, None)
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}] ▶ {stage}", style=_STYLE_STARTED))

    def step_started(self, system: str, stage: str, step: str, detail: str = "") -> None:
        payload = {"event": "OnStepStarted", "system": system, "stage": stage, "step": step}
        if detail:
            payload["detail"] = detail
        self._record_start(system, stage, step)
        self._write_event(payload)
        label = f"{step} {detail}".strip()
        self._render(Text(f"[{self._ts()}]   {label} ...", style=_STYLE_DIM))

    def step_completed(self, system: str, stage: str, step: str) -> None:
        duration_ms = self._elapsed_ms(system, stage, step)
        payload = {"event": "OnStepCompleted", "system": system, "stage": stage, "step": step, "durationMs": duration_ms}
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}]   {step} ... done ({duration_ms / 1000:.1f}s)", style=_STYLE_COMPLETED))

    def step_failed(self, system: str, stage: str, step: str, error: str) -> None:
        duration_ms = self._elapsed_ms(system, stage, step)
        payload = {"event": "OnStepFailed", "system": system, "stage": stage, "step": step, "error": error, "durationMs": duration_ms}
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}]   {step} ... FAILED: {error}", style=_STYLE_FAILED))

    def stage_completed(self, system: str, stage: str) -> None:
        duration_ms = self._elapsed_ms(system, stage, None)
        payload = {"event": "OnStageCompleted", "system": system, "stage": stage, "durationMs": duration_ms}
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}] ✓ {stage}  {duration_ms / 1000:.1f}s", style=_STYLE_COMPLETED))

    def stage_failed(self, system: str, stage: str, error: str) -> None:
        duration_ms = self._elapsed_ms(system, stage, None)
        payload = {"event": "OnStageFailed", "system": system, "stage": stage, "error": error, "durationMs": duration_ms}
        self._write_event(payload)
        self._render(Text(f"[{self._ts()}] ✗ {stage}  FAILED: {error}", style=_STYLE_FAILED))

    def stage_skipped(self, system: str, stage: str, reason: str = "") -> None:
        payload = {"event": "OnStageSkipped", "system": system, "stage": stage}
        if reason:
            payload["reason"] = reason
        self._write_event(payload)
        suffix = f"  ({reason})" if reason else ""
        self._render(Text(f"[{self._ts()}] ○ {stage}  skipped{suffix}", style=_STYLE_SKIPPED))

    def run_completed(self, system: str, pass_count: int, fail_count: int) -> None:
        duration_ms = self._elapsed_ms(system, None, None)
        payload = {"event": "OnRunCompleted", "system": system, "passCount": pass_count, "failCount": fail_count, "durationMs": duration_ms, "totalDurationMs": duration_ms}
        self._write_event(payload)
        self._close_log()
        sep = "─" * 45
        self._render(Text(f"[{self._ts()}] {sep}", style=_STYLE_DIM))
        self._render(Text(
            f"[{self._ts()}] ✓ {system} complete  {pass_count} passed · {fail_count} failed · {duration_ms / 1000:.1f}s",
            style=_STYLE_SUMMARY,
        ))

    def run_failed(self, system: str, pass_count: int, fail_count: int) -> None:
        duration_ms = self._elapsed_ms(system, None, None)
        payload = {"event": "OnRunFailed", "system": system, "passCount": pass_count, "failCount": fail_count, "durationMs": duration_ms, "totalDurationMs": duration_ms}
        self._write_event(payload)
        self._close_log()
        sep = "─" * 45
        self._render(Text(f"[{self._ts()}] {sep}", style=_STYLE_DIM))
        self._render(Text(
            f"[{self._ts()}] ✗ {system} failed  {pass_count} passed · {fail_count} failed · {duration_ms / 1000:.1f}s",
            style=_STYLE_SUMMARY_F,
        ))

    def log(self, system: str, level: str, message: str, stage: str = "") -> None:
        payload = {"event": "OnLogLine", "system": system, "level": level, "message": message}
        if stage:
            payload["stage"] = stage
        self._write_event(payload)
        if not self._quiet:
            style = _STYLE_WARNING if level == "warn" else (_STYLE_FAILED if level == "error" else _STYLE_DIM)
            self._render(Text(f"[{self._ts()}]   {message}", style=style))

    def warn(self, system: str, message: str, stage: str = "") -> None:
        self.log(system=system, level="warn", message=message, stage=stage)

    # ------------------------------------------------------------------ #
    # Internal helpers                                                     #
    # ------------------------------------------------------------------ #

    def _ts(self) -> str:
        return datetime.now().strftime("%H:%M:%S")

    def _record_start(self, system: str, stage: Optional[str], step: Optional[str]) -> None:
        self._start_times[(system, stage, step)] = time.time()

    def _elapsed_ms(self, system: str, stage: Optional[str], step: Optional[str]) -> int:
        start = self._start_times.get((system, stage, step))
        if start is None:
            return 0
        return int((time.time() - start) * 1000)

    def _log_path(self, system: str) -> Path:
        if self._log_json_override:
            return self._log_json_override
        return self._log_dir / system / "last-run.ndjson"

    def _open_log(self, system: str) -> None:
        path = self._log_path(system)
        path.parent.mkdir(parents=True, exist_ok=True)
        self._log_file = open(path, "w", encoding="utf-8")  # noqa: SIM115 — intentional; closed in _close_log

    def _close_log(self) -> None:
        if self._log_file:
            self._log_file.close()
            self._log_file = None

    def _write_event(self, payload: dict) -> None:
        payload["ts"] = time.time()
        line = json.dumps(payload, separators=(",", ":"))
        with self._lock:
            if self._log_file:
                self._log_file.write(line + "\n")
                self._log_file.flush()

    def _render(self, text: Text) -> None:
        if self._quiet:
            return
        with self._lock:
            self._console.print(text)
