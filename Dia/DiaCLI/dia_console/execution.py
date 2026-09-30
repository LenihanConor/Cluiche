"""Subprocess-only execution of DiaCLI commands, with typed event streaming.

Execution is **always** a real ``dia`` subprocess (SD-CONSOLE-009): every DiaCLI
command ends in ``ctx.exit()``/``SystemExit`` and mutates global state
(``sys.path``, open log handles), so nothing here may call a Click callback in
process.

Each execution is given its own ``--log-json`` path
(``Cluiche/out/DiaCLI/logs/console/<execution_id>.ndjson``) so concurrent runs
never collide on DiaCLI's default ``<system>/last-run.ndjson``.
:class:`ExecutionHandle` then exposes two *independent* streams over that run:

* :meth:`ExecutionHandle.events` — typed :class:`ExecutionEvent`s tailed from
  the NDJSON file, translated from DiaCLI's ``dia.output.v1`` wire format.
* :meth:`ExecutionHandle.log_lines` — raw stdout/stderr text.

They are never merged, and consuming only one of them never blocks the other:
stdout is drained into an in-memory buffer as soon as the process starts, which
also stops a chatty command from deadlocking on a full pipe.
"""
from __future__ import annotations

import asyncio
import json
import shutil
import subprocess
import sys
import time
import uuid
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, AsyncIterator, Iterable, Mapping, Sequence

from loguru import logger

from dia_cli.utils.repo_root import find_repo_root
from dia_console.model import CommandDescriptor, ExecuteCommandRequest, ExecutionEvent
from dia_console.registry import CommandRegistry


@dataclass
class ExecutionMetrics:
    """Best-effort, process-lifetime execution lifecycle counters (Observation
    Opportunity Scan finding #11) -- not persisted, reset on process restart.

    ``completed``/``failed`` only count executions whose event stream was
    actually consumed to a terminal event (every normal UI/CLI caller does);
    ``cancelled`` is incremented unconditionally from :meth:`ExecutionHandle.cancel`
    itself, since that call happens regardless of whether anyone is still
    consuming :meth:`ExecutionHandle.events`.
    """

    started: int = 0
    completed: int = 0
    failed: int = 0
    cancelled: int = 0


_metrics = ExecutionMetrics()


def get_metrics() -> ExecutionMetrics:
    """The shared, process-lifetime :class:`ExecutionMetrics` instance."""
    return _metrics


def reset_metrics() -> None:
    """Reset the shared counters. Test-only -- production code never calls this."""
    global _metrics
    _metrics = ExecutionMetrics()

# --------------------------------------------------------------------------- #
# Wire format translation (SD-CONSOLE-002)
# --------------------------------------------------------------------------- #

#: DiaCLI's ``event`` field -> ``ExecutionEvent.type``.
EVENT_TYPE_MAP: Mapping[str, str] = {
    "OnRunStarted": "execution.started",
    "OnRunCompleted": "execution.completed",
    "OnRunFailed": "execution.failed",
    "OnStageStarted": "step.started",
    "OnStepStarted": "step.started",
    "OnStageCompleted": "step.completed",
    "OnStepCompleted": "step.completed",
    "OnStageFailed": "step.failed",
    "OnStepFailed": "step.failed",
    "OnStageSkipped": "step.skipped",
    "OnLogLine": "log",
}

#: Any event outside :data:`EVENT_TYPE_MAP` (``OnAssetTransformed``,
#: ``OnBuildCompleted``, anything a future system adds) becomes this, with the
#: original name preserved as ``payload["rawEvent"]``.
FALLBACK_EVENT_TYPE = "step.progress"

EXECUTION_STARTED = "execution.started"
EXECUTION_COMPLETED = "execution.completed"
EXECUTION_FAILED = "execution.failed"
EXECUTION_CANCELLED = "execution.cancelled"

#: Types that end an :meth:`ExecutionHandle.events` stream.
TERMINAL_EVENT_TYPES = frozenset({EXECUTION_COMPLETED, EXECUTION_FAILED, EXECUTION_CANCELLED})

#: Message used when the command never opened its NDJSON file.  Most DiaCLI
#: commands do not use ``OutputContext`` at all, so this is an expected outcome
#: for them, not a defect.
NO_OUTPUT_STREAM_MESSAGE = "output stream never initialized"

_LOG_SUBDIR = ("Cluiche", "out", "DiaCLI", "logs", "console")
_DEFAULT_EVENT_TIMEOUT_SECONDS = 5.0
_DEFAULT_POLL_INTERVAL_SECONDS = 0.1
_LOG_DRAIN_INTERVAL_SECONDS = 0.01
_STDOUT_CHUNK_BYTES = 4096


def event_from_ndjson(raw: Mapping[str, Any], execution_id: str, seq: int) -> ExecutionEvent:
    """Translate one ``dia.output.v1`` NDJSON record into an :class:`ExecutionEvent`.

    Never raises: an unknown or missing ``event`` name degrades to
    :data:`FALLBACK_EVENT_TYPE`, and a missing/invalid ``ts`` falls back to now.
    """
    wire_event = raw.get("event") or ""
    payload = {key: value for key, value in raw.items() if key not in ("event", "ts")}
    event_type = EVENT_TYPE_MAP.get(wire_event)
    if event_type is None:
        event_type = FALLBACK_EVENT_TYPE
        payload["rawEvent"] = wire_event
    stamp = raw.get("ts")
    when = datetime.fromtimestamp(stamp) if isinstance(stamp, (int, float)) else datetime.now()
    step_id = payload.get("step") or payload.get("stage") or None
    return ExecutionEvent(
        execution_id=execution_id,
        seq=seq,
        time=when,
        type=event_type,
        step_id=step_id if step_id is None else str(step_id),
        payload=payload,
    )


def default_argv_prefix() -> list[str]:
    """The argv words that start a ``dia`` invocation.

    Prefers the installed ``dia`` launcher; falls back to running the CLI module
    with the current interpreter when the venv's ``Scripts`` dir is not on PATH.
    """
    executable = shutil.which("dia")
    if executable:
        return [executable]
    return [sys.executable, "-m", "dia_cli.cli_main"]


def _as_bool(value: Any) -> bool:
    if isinstance(value, str):
        return value.strip().lower() in ("1", "true", "yes", "on")
    return bool(value)


# --------------------------------------------------------------------------- #
# ExecutionHandle
# --------------------------------------------------------------------------- #

class ExecutionHandle:
    """A running (or finished) command execution.

    Constructed by :meth:`ExecutionService.execute`; constructed directly only by
    tests.  Must be created while an asyncio loop is running, because it starts
    draining the process's stdout immediately.
    """

    def __init__(
        self,
        execution_id: str,
        process: Any,
        log_path: Path | str,
        *,
        argv: Sequence[str] = (),
        event_timeout_seconds: float = _DEFAULT_EVENT_TIMEOUT_SECONDS,
        poll_interval_seconds: float = _DEFAULT_POLL_INTERVAL_SECONDS,
    ) -> None:
        self.execution_id = execution_id
        self.log_path = Path(log_path)
        self.argv: tuple[str, ...] = tuple(argv)
        self._process = process
        self._event_timeout = event_timeout_seconds
        self._poll_interval = poll_interval_seconds
        self._cancelled = False
        self._log_buffer: list[str] = []
        self._log_done = asyncio.Event()
        self._pump_task: asyncio.Future | None = None
        self._start_log_pump()

    # -- process state ---------------------------------------------------- #

    @property
    def pid(self) -> int | None:
        return getattr(self._process, "pid", None)

    @property
    def returncode(self) -> int | None:
        return self._process.returncode

    @property
    def cancelled(self) -> bool:
        return self._cancelled

    async def wait(self) -> int:
        """Wait for the process to exit and its stdout to be fully drained."""
        code = await self._process.wait()
        if self._pump_task is not None:
            try:
                await self._pump_task
            except asyncio.CancelledError:  # pragma: no cover - defensive
                pass
        return code

    # -- raw log stream --------------------------------------------------- #

    def _start_log_pump(self) -> None:
        stdout = getattr(self._process, "stdout", None)
        if stdout is None:
            self._log_done.set()
            return
        try:
            self._pump_task = asyncio.ensure_future(self._pump_stdout(stdout))
        except RuntimeError:  # pragma: no cover - no running loop
            self._log_done.set()

    async def _pump_stdout(self, stdout: Any) -> None:
        pending = ""
        try:
            while True:
                chunk = await stdout.read(_STDOUT_CHUNK_BYTES)
                if not chunk:
                    break
                pending += chunk.decode("utf-8", errors="replace")
                *lines, pending = pending.split("\n")
                for line in lines:
                    self._log_buffer.append(line.rstrip("\r"))
        except asyncio.CancelledError:  # pragma: no cover - loop teardown
            pass
        finally:
            if pending:
                self._log_buffer.append(pending.rstrip("\r"))
            self._log_done.set()

    def log_tail(self, n: int = 20) -> list[str]:
        """A snapshot of the last ``n`` lines drained from stdout/stderr so far.

        Additive convenience over the internal ``_log_buffer`` list already
        used by :meth:`log_lines` -- does not consume or affect any other
        reader; safe to call at any point in the execution's lifecycle.
        """
        return list(self._log_buffer[-n:])

    async def log_lines(self) -> AsyncIterator[str]:
        """Yield the process's stdout/stderr lines as raw text until it exits.

        Independent of :meth:`events`; safe to consume from zero, one or several
        callers (each gets the full history from the start).
        """
        index = 0
        while True:
            if index < len(self._log_buffer):
                line = self._log_buffer[index]
                index += 1
                yield line
                continue
            if self._log_done.is_set():
                if index < len(self._log_buffer):
                    continue
                return
            await asyncio.sleep(_LOG_DRAIN_INTERVAL_SECONDS)

    # -- typed event stream ----------------------------------------------- #

    def _synthetic_event(self, event_type: str, seq: int, message: str) -> ExecutionEvent:
        return ExecutionEvent(
            execution_id=self.execution_id,
            seq=seq,
            time=datetime.now(),
            type=event_type,
            step_id=None,
            payload={"message": message, "executionId": self.execution_id},
        )

    async def events(self) -> AsyncIterator[ExecutionEvent]:
        """Yield typed events tailed from this execution's NDJSON file.

        Terminates on the first terminal event (``execution.completed`` /
        ``execution.failed`` / ``execution.cancelled``).  Never hangs: if the
        NDJSON file never appears it terminates with ``execution.failed``
        (:data:`NO_OUTPUT_STREAM_MESSAGE`), and if the process exits without
        emitting a terminal event it terminates with ``execution.failed`` too.
        """
        seq = 0
        poll_started = time.monotonic()
        deadline = poll_started + self._event_timeout
        while not self.log_path.exists():
            if self._cancelled:
                yield self._synthetic_event(EXECUTION_CANCELLED, seq, "execution cancelled")
                return
            if time.monotonic() >= deadline:
                logger.warning(
                    "Execution {} timed out waiting for NDJSON output ({}s)",
                    self.execution_id, self._event_timeout,
                )
                yield self._synthetic_event(EXECUTION_FAILED, seq, NO_OUTPUT_STREAM_MESSAGE)
                _metrics.failed += 1
                return
            await asyncio.sleep(self._poll_interval)
        logger.debug(
            "Execution {} NDJSON output appeared after {:.2f}s",
            self.execution_id, time.monotonic() - poll_started,
        )

        pending = ""
        drained_after_exit = False
        saw_premature_terminal_event = False
        with open(self.log_path, "r", encoding="utf-8", errors="replace") as handle:
            while True:
                data = handle.read()
                if data:
                    drained_after_exit = False
                    pending += data
                    *lines, pending = pending.split("\n")
                    for line in lines:
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            raw = json.loads(line)
                        except json.JSONDecodeError:
                            continue
                        if not isinstance(raw, dict):
                            continue
                        event = event_from_ndjson(raw, self.execution_id, seq)
                        seq += 1
                        yield event
                        if event.type in TERMINAL_EVENT_TYPES:
                            if self._process.returncode is None:
                                # The real process is still running -- e.g.
                                # `dia run`'s own pipeline phase reports
                                # OnRunCompleted well before launch_target()
                                # even starts, in the same process (confirmed
                                # live: the log pane stopped after one test
                                # line because this used to end the stream
                                # right here, before GoogleTests.exe had
                                # produced almost any of its output). A
                                # terminal-mapped NDJSON event is not
                                # authoritative on its own; the process's
                                # actual exit is -- handled by the "process
                                # exited without a terminal event" fallback
                                # below, which yields the real final event
                                # once it actually happens.
                                logger.debug(
                                    "Execution {} saw a terminal-mapped event ({}) while the "
                                    "process is still running; continuing to tail",
                                    self.execution_id, event.type,
                                )
                                saw_premature_terminal_event = True
                                continue
                            if event.type == EXECUTION_COMPLETED:
                                _metrics.completed += 1
                            elif event.type == EXECUTION_FAILED:
                                _metrics.failed += 1
                            return
                    continue
                if self._cancelled:
                    yield self._synthetic_event(EXECUTION_CANCELLED, seq, "execution cancelled")
                    return
                if self._process.returncode is not None:
                    if drained_after_exit:
                        # The actual exit code is the only ground truth available
                        # when a command never emits a structured terminal event
                        # (most DiaCLI commands don't use OutputContext at all) --
                        # a real bug reported this as "execution failed" even when
                        # returncode == 0 (confirmed live: `dia run googletest`
                        # PASSED with exit 0, but was always reported as failed).
                        succeeded = self._process.returncode == 0
                        event_type = EXECUTION_COMPLETED if succeeded else EXECUTION_FAILED
                        reason = (
                            "after an earlier terminal event that arrived before the "
                            "process actually exited (e.g. a multi-phase command like "
                            "`dia run`)" if saw_premature_terminal_event
                            else "before emitting a terminal event"
                        )
                        message = f"process exited with code {self._process.returncode} {reason}"
                        log = logger.info if succeeded else logger.warning
                        log("Execution {} exited with code {} {}",
                            self.execution_id, self._process.returncode, reason)
                        yield self._synthetic_event(event_type, seq, message)
                        if succeeded:
                            _metrics.completed += 1
                        else:
                            _metrics.failed += 1
                        return
                    drained_after_exit = True
                await asyncio.sleep(self._poll_interval)

    # -- cancellation ----------------------------------------------------- #

    async def cancel(self) -> None:
        """Force-kill the execution's whole process tree.

        DiaCLI commands shell out to MSBuild, cppcheck and test runners, none of
        which is guaranteed to honour a cooperative signal, so this is a
        ``taskkill /F /T`` of the tree rather than a console-control event.
        After this, :meth:`events` terminates with ``execution.cancelled``.
        """
        already_cancelled = self._cancelled
        self._cancelled = True
        if not already_cancelled:
            logger.info("Cancelling execution {} (pid={})", self.execution_id, self.pid)
            _metrics.cancelled += 1
        if self._process.returncode is None:
            pid = self.pid
            if pid is not None and sys.platform == "win32":
                await self._kill_tree(pid)
            else:  # pragma: no cover - non-Windows fallback
                try:
                    self._process.kill()
                except ProcessLookupError:
                    pass
        try:
            await asyncio.wait_for(self._process.wait(), timeout=10)
        except (asyncio.TimeoutError, ProcessLookupError):  # pragma: no cover - defensive
            pass

    @staticmethod
    async def _kill_tree(pid: int) -> None:
        killer = await asyncio.create_subprocess_exec(
            shutil.which("taskkill") or "taskkill", "/F", "/T", "/PID", str(pid),
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL,
        )
        await killer.wait()


# --------------------------------------------------------------------------- #
# ExecutionService
# --------------------------------------------------------------------------- #

class ExecutionService:
    """Turns an :class:`ExecuteCommandRequest` into a running ``dia`` subprocess."""

    def __init__(
        self,
        registry: CommandRegistry,
        *,
        argv_prefix: Iterable[str] | None = None,
        log_root: Path | str | None = None,
        repo_root: Path | str | None = None,
        event_timeout_seconds: float = _DEFAULT_EVENT_TIMEOUT_SECONDS,
        poll_interval_seconds: float = _DEFAULT_POLL_INTERVAL_SECONDS,
    ) -> None:
        self._registry = registry
        self._repo_root = Path(repo_root) if repo_root is not None else find_repo_root(__file__)
        self._log_root = Path(log_root) if log_root is not None else self._repo_root.joinpath(*_LOG_SUBDIR)
        self._argv_prefix = tuple(argv_prefix) if argv_prefix is not None else tuple(default_argv_prefix())
        self._event_timeout = event_timeout_seconds
        self._poll_interval = poll_interval_seconds

    # -- inspection ------------------------------------------------------- #

    @property
    def registry(self) -> CommandRegistry:
        return self._registry

    @property
    def argv_prefix(self) -> tuple[str, ...]:
        return self._argv_prefix

    @property
    def repo_root(self) -> Path:
        return self._repo_root

    @property
    def log_root(self) -> Path:
        return self._log_root

    def log_path_for(self, execution_id: str) -> Path:
        """This execution's private NDJSON path — never shared between runs."""
        return self._log_root / f"{execution_id}.ndjson"

    # -- argv construction ------------------------------------------------ #

    def build_argv(
        self,
        request: ExecuteCommandRequest,
        execution_id: str,
        log_path: Path | str | None = None,
    ) -> list[str]:
        """Serialize ``request`` into the argv of an equivalent hand-typed command.

        ``--log-json`` is a *root* option on ``dia``, so it precedes the command
        path.  Serialization rules: flags emit their flag when truthy and are
        omitted entirely when falsey (no ``--no-x`` form); ``multiple`` options
        repeat once per value; positional arguments (including ``nargs=-1``) are
        appended in declared order; every supplied value is emitted explicitly
        even when it equals the Click default; all values are ``str()``-coerced.
        Keys absent from the request — and explicit ``None`` values — are
        skipped, since Click will apply its own default in that case.
        """
        descriptor: CommandDescriptor = self._registry.require(request.command_id)
        resolved_log_path = Path(log_path) if log_path is not None else self.log_path_for(execution_id)
        argv: list[str] = [*self._argv_prefix, "--log-json", str(resolved_log_path), *descriptor.path]

        options = request.options or {}
        for option in descriptor.options:
            if option.name not in options:
                continue
            value = options[option.name]
            if option.is_flag:
                if _as_bool(value):
                    argv.append(option.cli_flag)
                continue
            if value is None:
                continue
            if option.multiple or isinstance(value, (list, tuple, set)):
                for item in value:
                    argv.extend([option.cli_flag, str(item)])
                continue
            argv.extend([option.cli_flag, str(value)])

        arguments = request.arguments or {}
        for argument in descriptor.arguments:
            if argument.name not in arguments:
                continue
            value = arguments[argument.name]
            if value is None:
                continue
            if argument.multiple or isinstance(value, (list, tuple, set)):
                argv.extend(str(item) for item in value)
                continue
            argv.append(str(value))

        return argv

    # -- execution -------------------------------------------------------- #

    async def execute(self, request: ExecuteCommandRequest) -> ExecutionHandle:
        """Spawn ``dia <command> ...`` as a subprocess and return its handle."""
        execution_id = uuid.uuid4().hex
        log_path = self.log_path_for(execution_id)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        argv = self.build_argv(request, execution_id=execution_id, log_path=log_path)
        _metrics.started += 1
        logger.info("Execution {} started: {}", execution_id, " ".join(argv))
        process = await asyncio.create_subprocess_exec(
            *argv,
            cwd=str(self._repo_root),
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.STDOUT,
            creationflags=getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
        )
        return ExecutionHandle(
            execution_id=execution_id,
            process=process,
            log_path=log_path,
            argv=argv,
            event_timeout_seconds=self._event_timeout,
            poll_interval_seconds=self._poll_interval,
        )
