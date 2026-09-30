"""Unit + integration tests for dia_console/execution.py (console-command-model).

Pure tests (event mapping, argv serialization, timeout/cancel semantics against a
stub process) run everywhere.  Tests that spawn real processes are marked
``integration``.
"""
import asyncio
import inspect
import json
import sys
import time
from datetime import datetime
from pathlib import Path

import click
import pytest

from dia_cli.cli_main import cli as dia_cli_app
from dia_console import execution as execution_module
from dia_console.execution import ExecutionHandle, ExecutionService, event_from_ndjson
from dia_console.model import ExecuteCommandRequest
from dia_console.registry import CommandRegistry


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def run(coro):
    """Drive a coroutine to completion (no pytest-asyncio in this project)."""
    return asyncio.run(coro)


async def collect(agen, limit=1000):
    out = []
    async for item in agen:
        out.append(item)
        if len(out) >= limit:
            break
    return out


class _StubProcess:
    """Minimal stand-in for asyncio.subprocess.Process (no real process)."""

    def __init__(self, returncode=None, pid=999_999):
        self.returncode = returncode
        self.stdout = None
        self.pid = pid
        self.killed = False

    async def wait(self):
        while self.returncode is None:
            await asyncio.sleep(0.01)
        return self.returncode

    def kill(self):
        self.killed = True
        self.returncode = -9


def make_handle(tmp_path, *, returncode=None, timeout=0.3, name="x.ndjson"):
    return ExecutionHandle(
        execution_id="exec1",
        process=_StubProcess(returncode=returncode),
        log_path=tmp_path / name,
        event_timeout_seconds=timeout,
        poll_interval_seconds=0.02,
    )


def write_ndjson(path, events):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "a", encoding="utf-8") as fh:
        for event in events:
            fh.write(json.dumps({**event, "ts": event.get("ts", time.time())}) + "\n")
        fh.flush()


# ---------------------------------------------------------------------------
# Synthetic Click apps
# ---------------------------------------------------------------------------

@pytest.fixture
def argv_app():
    @click.group()
    def root():
        """Root."""

    @root.group("grp")
    def grp():
        """Group."""

    @grp.command("cmd")
    @click.option("--config", type=click.Choice(["Debug", "Release"]), default="Debug")
    @click.option("--shards", type=int, default=1)
    @click.option("--force", is_flag=True, default=False)
    @click.option("--verbose", is_flag=True, default=False)
    @click.option("--filter", "filter_pattern", default=None)
    @click.option("--tag", "tags", multiple=True)
    @click.argument("target")
    @click.argument("extras", nargs=-1)
    def cmd(**kwargs):
        """Command."""

    return root


@pytest.fixture
def argv_service(argv_app, tmp_path):
    return ExecutionService(
        CommandRegistry.from_click_app(argv_app),
        argv_prefix=["dia"],
        log_root=tmp_path / "logs" / "console",
        repo_root=tmp_path,
    )


FAKE_DIA = '''
import json, os, subprocess, sys, time


def write(fh, event, **kw):
    fh.write(json.dumps({"event": event, "system": "fake", **kw, "ts": time.time()}) + "\\n")
    fh.flush()


def main():
    argv = sys.argv[1:]
    log_path = None
    if argv[:1] == ["--log-json"]:
        log_path, argv = argv[1], argv[2:]
    mode = argv[0] if argv else "quick"
    rest = argv[1:]
    print("fake_dia mode=%s rest=%s" % (mode, rest), flush=True)
    if mode == "silent":
        print("silent line one", flush=True)
        print("silent line two", flush=True)
        return 0
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    fh = open(log_path, "w", encoding="utf-8")
    write(fh, "OnRunStarted", schema="dia.output.v1")
    write(fh, "OnStageStarted", stage="work")
    write(fh, "OnAssetTransformed", assetId="a1")
    print("working", flush=True)
    if mode == "slow":
        marker = rest[rest.index("--marker") + 1]
        code = "import sys,time\\np=sys.argv[1]\\nwhile True:\\n    open(p,'a').write('x')\\n    time.sleep(0.05)\\n"
        subprocess.Popen([sys.executable, "-c", code, marker])
        time.sleep(300)
    write(fh, "OnStageCompleted", stage="work", durationMs=1)
    write(fh, "OnRunCompleted", passCount=1, failCount=0, durationMs=2)
    fh.close()
    return 0


sys.exit(main())
'''


@pytest.fixture
def fake_service(tmp_path):
    """ExecutionService wired to a deterministic stand-in for the `dia` exe."""
    script = tmp_path / "fake_dia.py"
    script.write_text(FAKE_DIA, encoding="utf-8")

    @click.group()
    def root():
        """Root."""

    @root.command("quick")
    def quick():
        """Fast successful run."""

    @root.command("slow")
    @click.option("--marker", required=True)
    def slow(marker):
        """Long run that spawns a grandchild."""

    @root.command("silent")
    def silent():
        """Writes no NDJSON at all."""

    return ExecutionService(
        CommandRegistry.from_click_app(root),
        argv_prefix=[sys.executable, str(script)],
        log_root=tmp_path / "logs" / "console",
        repo_root=tmp_path,
        event_timeout_seconds=2.0,
        poll_interval_seconds=0.05,
    )


@pytest.fixture
def real_service():
    return ExecutionService(CommandRegistry.from_click_app(dia_cli_app))


# ===========================================================================
# Event-type mapping (SD-CONSOLE-002)
# ===========================================================================

@pytest.mark.parametrize("wire_event,expected", [
    ("OnRunStarted", "execution.started"),
    ("OnRunCompleted", "execution.completed"),
    ("OnRunFailed", "execution.failed"),
    ("OnStageStarted", "step.started"),
    ("OnStepStarted", "step.started"),
    ("OnStageCompleted", "step.completed"),
    ("OnStepCompleted", "step.completed"),
    ("OnStageFailed", "step.failed"),
    ("OnStepFailed", "step.failed"),
    ("OnStageSkipped", "step.skipped"),
    ("OnLogLine", "log"),
])
def test_event_type_mapping_table(wire_event, expected):
    event = event_from_ndjson({"event": wire_event, "ts": 1.0}, execution_id="e", seq=3)
    assert event.type == expected
    assert event.execution_id == "e"
    assert event.seq == 3


@pytest.mark.parametrize("wire_event", [
    "OnAssetValidated", "OnAssetTransformed", "OnAssetDeployed",
    "OnAssetFailed", "OnBuildCompleted", "OnSomethingInventedLater",
])
def test_unknown_event_falls_back_to_step_progress_with_raw_name(wire_event):
    event = event_from_ndjson({"event": wire_event, "ts": 1.0, "assetId": "a"}, execution_id="e", seq=0)
    assert event.type == "step.progress"
    assert event.payload["rawEvent"] == wire_event
    assert event.payload["assetId"] == "a"


def test_payload_excludes_event_and_ts():
    event = event_from_ndjson(
        {"event": "OnLogLine", "ts": 1.0, "level": "warn", "message": "m"},
        execution_id="e", seq=0,
    )
    assert "event" not in event.payload
    assert "ts" not in event.payload
    assert event.payload == {"level": "warn", "message": "m"}


def test_time_comes_from_ts_epoch_float():
    stamp = 1_700_000_000.5
    event = event_from_ndjson({"event": "OnRunStarted", "ts": stamp}, execution_id="e", seq=0)
    assert event.time == datetime.fromtimestamp(stamp)


def test_step_id_prefers_step_then_stage_then_none():
    from_step = event_from_ndjson(
        {"event": "OnStepStarted", "ts": 1.0, "stage": "compile", "step": "msbuild"}, "e", 0)
    from_stage = event_from_ndjson({"event": "OnStageStarted", "ts": 1.0, "stage": "compile"}, "e", 0)
    neither = event_from_ndjson({"event": "OnRunStarted", "ts": 1.0}, "e", 0)
    assert from_step.step_id == "msbuild"
    assert from_stage.step_id == "compile"
    assert neither.step_id is None


def test_event_mapping_never_raises_on_malformed_input():
    event = event_from_ndjson({}, execution_id="e", seq=0)
    assert event.type == "step.progress"
    assert isinstance(event.time, datetime)


def test_event_mapping_ts_as_string_falls_back_to_now_instead_of_raising():
    """`ts` is documented as an epoch float; a string value (malformed but
    still valid JSON) must degrade to "now", not raise or propagate garbage."""
    before = datetime.now()
    event = event_from_ndjson({"event": "OnRunStarted", "ts": "not-a-timestamp"}, "e", 0)
    after = datetime.now()
    assert before <= event.time <= after


def test_event_mapping_missing_ts_falls_back_to_now():
    before = datetime.now()
    event = event_from_ndjson({"event": "OnRunStarted"}, "e", 0)
    after = datetime.now()
    assert before <= event.time <= after


# ===========================================================================
# argv serialization (Open Design Question 3)
# ===========================================================================

def test_argv_starts_with_prefix_then_log_json_then_command_path(argv_service, tmp_path):
    """--log-json is a *root* option on `dia`, so it must precede the command path."""
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {}),
        execution_id="abc",
    )
    assert argv[0] == "dia"
    assert argv[1] == "--log-json"
    assert argv[2].endswith("abc.ndjson")
    assert argv[3:5] == ["grp", "cmd"]


def test_argv_flag_true_emits_flag_and_false_is_omitted(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"force": True, "verbose": False}),
        execution_id="abc",
    )
    assert "--force" in argv
    assert "--verbose" not in argv
    assert "--no-verbose" not in argv


def test_argv_multiple_option_is_repeated_per_value(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"tags": ["x", "y"]}),
        execution_id="abc",
    )
    assert argv.count("--tag") == 2
    assert argv[argv.index("--tag"):argv.index("--tag") + 4] == ["--tag", "x", "--tag", "y"]


def test_argv_positional_arguments_in_declared_order_including_variadic(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "main", "extras": ["a", "b"]}, {}),
        execution_id="abc",
    )
    assert argv[-3:] == ["main", "a", "b"]


def test_argv_passes_value_explicitly_even_when_equal_to_click_default(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"config": "Debug", "shards": 1}),
        execution_id="abc",
    )
    assert "--config" in argv and argv[argv.index("--config") + 1] == "Debug"
    assert "--shards" in argv and argv[argv.index("--shards") + 1] == "1"


def test_argv_coerces_all_values_to_str(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": 42}, {"shards": 4}),
        execution_id="abc",
    )
    assert all(isinstance(word, str) for word in argv)
    assert "4" in argv
    assert "42" in argv


def test_argv_omits_absent_and_none_values(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"filter_pattern": None}),
        execution_id="abc",
    )
    assert "--filter" not in argv
    assert "--config" not in argv
    assert "--shards" not in argv


def test_argv_uses_real_cli_flag_not_the_python_param_name(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"filter_pattern": "Foo*"}),
        execution_id="abc",
    )
    assert "--filter" in argv
    assert "--filter-pattern" not in argv
    assert argv[argv.index("--filter") + 1] == "Foo*"


def test_argv_full_expected_sequence(argv_service, tmp_path):
    argv = argv_service.build_argv(
        ExecuteCommandRequest(
            "grp.cmd", "cluichetest",
            {"target": "main", "extras": ["e1"]},
            {"config": "Release", "shards": 2, "force": True, "verbose": False, "tags": ("a", "b")},
        ),
        execution_id="deadbeef",
    )
    log = str(tmp_path / "logs" / "console" / "deadbeef.ndjson")
    assert argv == [
        "dia", "--log-json", log, "grp", "cmd",
        "--config", "Release", "--shards", "2", "--force",
        "--tag", "a", "--tag", "b",
        "main", "e1",
    ]


def test_build_argv_rejects_unknown_command_id(argv_service):
    with pytest.raises(KeyError):
        argv_service.build_argv(ExecuteCommandRequest("nope", None, {}, {}), execution_id="abc")


# ---------------------------------------------------------------------------
# argv-injection-shaped values -- must always land as exactly one argv
# element, never split or shell-interpreted (subprocess_exec, never shell=True)
# ---------------------------------------------------------------------------

def test_argv_positional_value_with_shell_metacharacters_is_one_argv_element(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "; rm -rf / #"}, {}),
        execution_id="abc",
    )
    assert argv[-1] == "; rm -rf / #"
    assert argv.count("; rm -rf / #") == 1
    assert "rm" not in argv  # never split on whitespace


def test_argv_option_value_that_looks_like_a_flag_is_one_argv_element(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"filter_pattern": "--evil-flag"}),
        execution_id="abc",
    )
    assert argv[argv.index("--filter") + 1] == "--evil-flag"
    # Only one "--filter" word exists; the value itself is never re-parsed as a flag.
    assert argv.count("--filter") == 1


def test_argv_value_with_embedded_quotes_is_preserved_verbatim(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": 'he said "hi" & bye'}, {}),
        execution_id="abc",
    )
    assert argv[-1] == 'he said "hi" & bye'


def test_argv_unicode_value_is_preserved_verbatim(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "héllo-wörld-测试-🎮"}, {}),
        execution_id="abc",
    )
    assert argv[-1] == "héllo-wörld-测试-🎮"


def test_argv_negative_and_large_integer_options_coerce_to_str(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"shards": -1}),
        execution_id="abc",
    )
    assert argv[argv.index("--shards") + 1] == "-1"

    argv2 = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"shards": 999_999_999}),
        execution_id="abc",
    )
    assert argv2[argv2.index("--shards") + 1] == "999999999"


# ---------------------------------------------------------------------------
# `multiple` option: zero, one, and several values
# ---------------------------------------------------------------------------

def test_argv_multiple_option_with_zero_values_emits_nothing(argv_service):
    """Key present with an empty list -- distinct from the key being absent
    entirely -- must still emit no `--tag` words at all (nothing to repeat)."""
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"tags": []}),
        execution_id="abc",
    )
    assert "--tag" not in argv


def test_argv_multiple_option_with_exactly_one_value(argv_service):
    argv = argv_service.build_argv(
        ExecuteCommandRequest("grp.cmd", None, {"target": "t"}, {"tags": ["solo"]}),
        execution_id="abc",
    )
    assert argv.count("--tag") == 1
    assert argv[argv.index("--tag") + 1] == "solo"


# ===========================================================================
# Log path allocation (Goal 4)
# ===========================================================================

def test_log_path_is_unique_per_execution_and_under_console_dir(argv_service, tmp_path):
    first = argv_service.log_path_for("aaa")
    second = argv_service.log_path_for("bbb")
    assert first != second
    assert first.parent == tmp_path / "logs" / "console"
    assert first.name == "aaa.ndjson"


def test_default_log_root_is_repo_cluiche_out_diacli_logs_console(real_service):
    parts = real_service.log_path_for("x").parts
    assert parts[-6:] == ("Cluiche", "out", "DiaCLI", "logs", "console", "x.ndjson")


def test_default_argv_prefix_resolves_dia_or_falls_back_to_module(real_service):
    prefix = real_service.argv_prefix
    assert prefix
    launcher_is_dia = Path(prefix[0]).stem.lower() == "dia"
    assert launcher_is_dia or prefix[-1] == "dia_cli.cli_main"


# ===========================================================================
# SD-CONSOLE-009 — subprocess only, never in-process Click invocation
# ===========================================================================

def test_execution_module_never_invokes_click_in_process():
    source = inspect.getsource(execution_module)
    for banned in (".invoke(", "CliRunner", "standalone_mode", "make_context"):
        assert banned not in source, f"in-process Click invocation via {banned!r}"


def test_execution_module_uses_create_new_process_group_and_taskkill():
    source = inspect.getsource(execution_module)
    assert "CREATE_NEW_PROCESS_GROUP" in source
    assert "taskkill" in source
    assert "CTRL_BREAK_EVENT" not in source


# ===========================================================================
# events() lifecycle against a stub process (no real subprocess)
# ===========================================================================

def test_events_times_out_to_failed_when_ndjson_never_appears(tmp_path):
    handle = make_handle(tmp_path, timeout=0.3)
    events = run(collect(handle.events()))
    assert [e.type for e in events] == ["execution.failed"]
    assert events[0].payload["message"] == "output stream never initialized"
    assert events[0].seq == 0


def test_events_timeout_is_bounded_not_a_hang(tmp_path):
    handle = make_handle(tmp_path, timeout=0.3)
    started = time.monotonic()
    run(collect(handle.events()))
    assert 0.25 <= time.monotonic() - started < 3.0


def test_events_tails_ndjson_and_stops_at_terminal_event(tmp_path):
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0)
    write_ndjson(log_path, [
        {"event": "OnRunStarted", "system": "fake"},
        {"event": "OnStageStarted", "system": "fake", "stage": "work"},
        {"event": "OnAssetTransformed", "system": "fake", "assetId": "a1"},
        {"event": "OnStageCompleted", "system": "fake", "stage": "work"},
        {"event": "OnRunCompleted", "system": "fake", "passCount": 1, "failCount": 0},
        {"event": "OnRunStarted", "system": "never-read"},
    ])
    events = run(collect(handle.events()))
    assert [e.type for e in events] == [
        "execution.started", "step.started", "step.progress",
        "step.completed", "execution.completed",
    ]
    assert [e.seq for e in events] == [0, 1, 2, 3, 4]


def test_events_picks_up_lines_appended_after_iteration_started(tmp_path):
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=3.0)
    write_ndjson(log_path, [{"event": "OnRunStarted", "system": "fake"}])

    async def scenario():
        out = []
        agen = handle.events()
        out.append(await agen.__anext__())
        write_ndjson(log_path, [{"event": "OnRunCompleted", "system": "fake"}])
        out.append(await agen.__anext__())
        return out

    events = run(scenario())
    assert [e.type for e in events] == ["execution.started", "execution.completed"]


def test_events_ignores_blank_and_malformed_lines(tmp_path):
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        '\n{not json}\n{"event":"OnRunStarted","ts":1.0}\n\n{"event":"OnRunCompleted","ts":2.0}\n',
        encoding="utf-8",
    )
    events = run(collect(handle.events()))
    assert [e.type for e in events] == ["execution.started", "execution.completed"]


def test_events_ignores_lines_that_are_valid_json_but_not_an_object(tmp_path):
    """`[1, 2, 3]` and a bare `"hello"` are valid JSON, just not the dict shape
    every real NDJSON record has -- must be skipped, not raise or be treated
    as a payload."""
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text(
        '[1, 2, 3]\n"hello"\n42\n'
        '{"event":"OnRunStarted","ts":1.0}\n'
        '{"event":"OnRunCompleted","ts":2.0}\n',
        encoding="utf-8",
    )
    events = run(collect(handle.events()))
    assert [e.type for e in events] == ["execution.started", "execution.completed"]


def test_events_is_replayable_from_the_start_on_a_second_call(tmp_path):
    """Requesting events() again for an execution that already finished
    (e.g. a second GET /events after the run completed) must replay the same
    terminal event, not error out or return nothing."""
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0, returncode=0)
    write_ndjson(log_path, [
        {"event": "OnRunStarted", "system": "fake"},
        {"event": "OnRunCompleted", "system": "fake"},
    ])
    first = run(collect(handle.events()))
    second = run(collect(handle.events()))
    assert [e.type for e in first] == ["execution.started", "execution.completed"]
    assert [e.type for e in second] == ["execution.started", "execution.completed"]


def test_events_terminates_when_process_exits_without_terminal_event(tmp_path):
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0, returncode=1)
    write_ndjson(log_path, [{"event": "OnRunStarted", "system": "fake"}])
    events = run(collect(handle.events()))
    assert [e.type for e in events] == ["execution.started", "execution.failed"]
    assert "exited" in events[-1].payload["message"]


def test_events_yields_cancelled_after_cancel(tmp_path):
    """Process already exited so cancel() performs no kill — semantics only."""
    handle = make_handle(tmp_path, timeout=2.0, returncode=0)

    async def scenario():
        await handle.cancel()
        return await collect(handle.events())

    events = run(scenario())
    assert [e.type for e in events] == ["execution.cancelled"]


def test_cancelling_an_already_completed_execution_does_not_raise(tmp_path):
    """cancel() on a process that has already exited must be a clean no-op
    (no attempt to kill a dead process tree)."""
    handle = make_handle(tmp_path, timeout=2.0, returncode=0)
    run(handle.cancel())
    assert handle.cancelled is True
    assert handle.returncode == 0


def test_cancelling_twice_is_idempotent(tmp_path):
    handle = make_handle(tmp_path, timeout=2.0, returncode=0)

    async def scenario():
        await handle.cancel()
        await handle.cancel()

    run(scenario())
    assert handle.cancelled is True


def test_events_yields_cancelled_even_with_pending_ndjson_lines(tmp_path):
    log_path = tmp_path / "t.ndjson"
    handle = make_handle(tmp_path, name="t.ndjson", timeout=2.0, returncode=0)
    write_ndjson(log_path, [{"event": "OnRunStarted", "system": "fake"}])

    async def scenario():
        await handle.cancel()
        return await collect(handle.events())

    events = run(scenario())
    assert [e.type for e in events] == ["execution.started", "execution.cancelled"]


# ===========================================================================
# Full stack against a deterministic fake `dia` executable
# ===========================================================================

@pytest.mark.integration
def test_fake_execution_streams_events_and_logs_independently(fake_service):
    async def scenario():
        handle = await fake_service.execute(ExecuteCommandRequest("quick", None, {}, {}))
        events = await collect(handle.events())
        lines = await collect(handle.log_lines())
        return handle, events, lines

    handle, events, lines = run(scenario())
    types = [e.type for e in events]
    assert types[0] == "execution.started"
    assert types[-1] == "execution.completed"
    assert "step.progress" in types
    assert all(e.execution_id == handle.execution_id for e in events)
    assert any("working" in line for line in lines)
    assert handle.log_path.exists()


@pytest.mark.integration
def test_fake_execution_log_lines_work_without_consuming_events(fake_service):
    """A consumer of only one stream must not block on the other."""
    async def scenario():
        handle = await fake_service.execute(ExecuteCommandRequest("quick", None, {}, {}))
        return await collect(handle.log_lines())

    lines = run(scenario())
    assert any("working" in line for line in lines)


@pytest.mark.integration
def test_fake_non_instrumented_command_logs_but_events_time_out(fake_service):
    """Coverage caveat: no NDJSON written -> events() fails, log_lines() still works."""
    async def scenario():
        handle = await fake_service.execute(ExecuteCommandRequest("silent", None, {}, {}))
        lines = await collect(handle.log_lines())
        events = await collect(handle.events())
        return lines, events

    lines, events = run(scenario())
    assert any("silent line one" in line for line in lines)
    assert [e.type for e in events] == ["execution.failed"]
    assert events[0].payload["message"] == "output stream never initialized"


@pytest.mark.integration
def test_cancel_force_kills_the_whole_process_tree(fake_service, tmp_path):
    """Goal 6: taskkill /F /T must reap the grandchild too, not just `dia`."""
    marker = tmp_path / "grandchild.txt"

    async def scenario():
        handle = await fake_service.execute(
            ExecuteCommandRequest("slow", None, {}, {"marker": str(marker)}))
        deadline = time.monotonic() + 30
        while not (marker.exists() and marker.stat().st_size > 0):
            if time.monotonic() > deadline:
                raise AssertionError("grandchild never started")
            await asyncio.sleep(0.05)
        await handle.cancel()
        await asyncio.sleep(1.0)
        size_after_kill = marker.stat().st_size
        await asyncio.sleep(1.0)
        events = await collect(handle.events())
        return handle, size_after_kill, marker.stat().st_size, events

    handle, size_after_kill, size_later, events = run(scenario())
    assert handle.returncode is not None, "parent process still alive after cancel"
    assert size_later == size_after_kill, "grandchild survived cancel (tree not killed)"
    assert events[-1].type == "execution.cancelled"


# ===========================================================================
# Real DiaCLI end-to-end (AC: `dia asset build --target cluichetest`)
# ===========================================================================

@pytest.mark.integration
def test_real_asset_build_completes_with_events_and_logs(real_service):
    async def scenario():
        handle = await real_service.execute(ExecuteCommandRequest(
            "asset.build", "cluichetest", {}, {"target": "cluichetest"}))
        events = await collect(handle.events())
        lines = await collect(handle.log_lines())
        await handle.wait()
        return handle, events, lines

    handle, events, lines = run(scenario())
    types = [e.type for e in events]
    assert types[0] == "execution.started", types[:3]
    assert types[-1] == "execution.completed", types[-3:]
    assert any(t.startswith("step.") for t in types)
    assert lines, "no stdout captured"
    assert handle.log_path.name == f"{handle.execution_id}.ndjson"
    assert handle.returncode == 0


@pytest.mark.integration
def test_real_non_instrumented_command_times_out_to_failed(real_service):
    """`dia show config` never opens the NDJSON file — expected, not a defect."""
    async def scenario():
        handle = await real_service.execute(ExecuteCommandRequest("show.config", None, {}, {}))
        lines = await collect(handle.log_lines())
        events = await collect(handle.events())
        await handle.wait()
        return handle, lines, events

    handle, lines, events = run(scenario())
    assert lines, "no stdout captured"
    assert [e.type for e in events] == ["execution.failed"]
    assert events[0].payload["message"] == "output stream never initialized"
    assert not handle.log_path.exists()


@pytest.mark.integration
def test_two_concurrent_real_executions_do_not_collide(real_service):
    async def scenario():
        request = ExecuteCommandRequest("asset.build", "cluichetest", {}, {"target": "cluichetest"})
        first, second = await asyncio.gather(
            real_service.execute(request), real_service.execute(request))
        first_events, second_events = await asyncio.gather(
            collect(first.events()), collect(second.events()))
        await asyncio.gather(first.wait(), second.wait())
        return first, second, first_events, second_events

    first, second, first_events, second_events = run(scenario())

    assert first.execution_id != second.execution_id
    assert first.log_path != second.log_path
    assert first.log_path.exists() and second.log_path.exists()
    for handle, events in ((first, first_events), (second, second_events)):
        assert all(e.execution_id == handle.execution_id for e in events)
        assert [e.type for e in events].count("execution.started") == 1
        assert events[0].type == "execution.started"
        assert events[-1].type == "execution.completed"
    # No cross-contamination: each file holds exactly one run
    for handle in (first, second):
        raw = [json.loads(line) for line in
               handle.log_path.read_text(encoding="utf-8").strip().splitlines()]
        assert [r["event"] for r in raw].count("OnRunStarted") == 1
