"""Tests for dia_console/web/app.py (console-native-shell).

HTTP-layer tests use FastAPI's TestClient against a fake ExecutionService
wired to a synthetic Click app, so almost all of them never reflect the real
DiaCLI tree or spawn a real `dia` subprocess. The one exception is marked
`@pytest.mark.integration`: it drives a real `dia asset build` through the
full HTTP stack (Task 11).
"""
from __future__ import annotations

import json
import textwrap
import uuid
from pathlib import Path

import click
import pytest
from fastapi.testclient import TestClient

from dia_console.execution import ExecutionHandle
from dia_console.model import ExecuteCommandRequest
from dia_console.registry import CommandRegistry
from dia_console.web import nav_grouping
from dia_console.web.app import create_app


# ---------------------------------------------------------------------------
# Fakes -- no real subprocess anywhere in this module except the bottom test.
# ---------------------------------------------------------------------------

class _FakeProcess:
    """Stand-in for asyncio.subprocess.Process -- no real process spawned."""

    def __init__(self, returncode=None, pid=4242):
        self.returncode = returncode
        self.stdout = None
        self.pid = pid

    async def wait(self):
        return self.returncode if self.returncode is not None else 0

    def kill(self):
        self.returncode = -9


class FakeExecutionService:
    """Drop-in ExecutionService that never spawns a real process.

    Uses the real ExecutionHandle (not reimplemented) with a fake process
    against a log path that never appears, so events()/log_lines()/cancel()
    all run their real, already-tested code paths (see
    test_dia_console_execution.py's "output stream never initialized"
    coverage) deterministically and fast.
    """

    def __init__(self, registry: CommandRegistry, tmp_path: Path):
        self.registry = registry
        self._tmp_path = tmp_path
        self.executed: list[ExecuteCommandRequest] = []
        self.handles: dict[str, ExecutionHandle] = {}

    @property
    def repo_root(self) -> Path:
        """Mirrors ExecutionService.repo_root -- GET /api/context reads this
        to find pipeline.toml, exactly like the real service does."""
        return self._tmp_path

    async def execute(self, request: ExecuteCommandRequest) -> ExecutionHandle:
        self.registry.require(request.command_id)  # raises KeyError, like real build_argv
        self.executed.append(request)
        execution_id = uuid.uuid4().hex
        handle = ExecutionHandle(
            execution_id=execution_id,
            process=_FakeProcess(returncode=0),
            log_path=self._tmp_path / f"{execution_id}.ndjson",
            event_timeout_seconds=0.2,
            poll_interval_seconds=0.02,
        )
        self.handles[execution_id] = handle
        return handle


def parse_sse_frames(body: str) -> list:
    """Split an SSE response body into its decoded `data: <json>` frames."""
    frames = []
    for chunk in body.split("\n\n"):
        chunk = chunk.strip()
        if not chunk:
            continue
        assert chunk.startswith("data: "), f"non-SSE-framed chunk: {chunk!r}"
        frames.append(json.loads(chunk[len("data: "):]))
    return frames


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

@pytest.fixture
def fake_registry():
    @click.group()
    def root():
        """Root."""

    @root.command("build")
    @click.option("--target", required=True)
    def build(target):
        """Build a target."""

    @root.group("check")
    def check():
        """Check group."""

    @check.command("arch")
    def arch():
        """Arch check."""

    @root.group("test")
    def test_group():
        """Test group."""

    @test_group.command("googletest")
    def googletest():
        """Run googletest."""

    return CommandRegistry.from_click_app(root)


#: A minimal, deterministic pipeline.toml used only by the /api/context
#: tests -- written into tmp_path rather than pointed at the real repo root,
#: so these tests never depend on the real pipeline.toml's target list
#: staying exactly as it is today. Mirrors the real file's shape (a
#: non-hidden target with an app_name, plus a hidden one) closely enough to
#: exercise the same filtering path context.list_targets() uses.
_CONTEXT_TOML = textwrap.dedent("""\
    [global]
    default_config = "Debug"
    default_platform = "x64"
    default_target = "cluichetest"

    [targets.cluichetest]
    project = "Cluiche/CluicheTest/CluicheTest.vcxproj"
    app_name = "CluicheTest"

    [targets.diasfml]
    project = "Dia/DiaSFML/DiaSFML.vcxproj"
    hidden = true
""")


@pytest.fixture
def fake_service(fake_registry, tmp_path):
    return FakeExecutionService(fake_registry, tmp_path)


@pytest.fixture
def app(fake_service):
    return create_app(execution_service=fake_service)


@pytest.fixture
def client(app):
    return TestClient(app)


@pytest.fixture
def context_client(fake_registry, tmp_path):
    (tmp_path / "pipeline.toml").write_text(_CONTEXT_TOML)
    service = FakeExecutionService(fake_registry, tmp_path)
    return TestClient(create_app(execution_service=service))


def _execute(client, command_id="build", arguments=None, options=None):
    response = client.post("/api/execute", json={
        "command_id": command_id,
        "project_id": None,
        "arguments": arguments or {},
        "options": options if options is not None else {"target": "cluichetest"},
    })
    assert response.status_code == 200, response.text
    return response.json()["execution_id"]


# ===========================================================================
# GET /
# ===========================================================================

def test_root_serves_index_html(client):
    response = client.get("/")
    assert response.status_code == 200
    assert "Dia Console" in response.text


def test_static_assets_are_served(client):
    js = client.get("/static/app.js")
    css = client.get("/static/styles.css")
    assert js.status_code == 200
    assert css.status_code == 200


# ===========================================================================
# GET /api/commands
# ===========================================================================

def test_get_commands_returns_every_registry_command(client, fake_registry):
    body = client.get("/api/commands").json()
    assert {c["id"] for c in body} == {d.id for d in fake_registry.commands}


def test_get_commands_enriches_each_command_with_group_label(client):
    by_id = {c["id"]: c for c in client.get("/api/commands").json()}
    assert by_id["build"]["groupLabel"] == "Advanced"       # "build" is unmapped -> fallback
    assert by_id["check.arch"]["groupLabel"] == "Test & Quality"  # path[0] == "check"


def test_get_commands_serializes_tuples_as_json_arrays(client):
    by_id = {c["id"]: c for c in client.get("/api/commands").json()}
    assert isinstance(by_id["build"]["options"], list)
    assert isinstance(by_id["build"]["path"], list)


def test_registry_is_built_once_not_per_request(client, fake_registry):
    client.get("/api/commands")
    client.get("/api/commands")
    assert client.app.state.execution_service.registry is fake_registry


# ===========================================================================
# POST /api/execute
# ===========================================================================

def test_execute_returns_execution_id_and_tracks_handle(client, fake_service):
    execution_id = _execute(client)
    assert execution_id in fake_service.handles
    assert fake_service.executed[0].command_id == "build"


def test_execute_unknown_command_id_is_404(client):
    response = client.post("/api/execute", json={
        "command_id": "nope.nope", "project_id": None, "arguments": {}, "options": {},
    })
    assert response.status_code == 404


# ===========================================================================
# GET /api/executions/{id}/events and /logs — SSE framing + independence
# ===========================================================================

def test_events_stream_uses_sse_data_framing(client):
    execution_id = _execute(client)
    response = client.get(f"/api/executions/{execution_id}/events")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/event-stream")
    frames = parse_sse_frames(response.text)
    assert frames, "expected at least one SSE frame"
    # No NDJSON file ever appears (fake process) -> times out to a synthetic
    # execution.failed, exactly like the real ExecutionHandle's documented
    # "output stream never initialized" behaviour.
    assert frames[0]["type"] == "execution.failed"
    assert frames[0]["execution_id"] == execution_id


def test_logs_stream_uses_sse_data_framing_and_terminates(client):
    execution_id = _execute(client)
    response = client.get(f"/api/executions/{execution_id}/logs")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/event-stream")
    # Fake process has no stdout -> zero log lines, but the stream still
    # terminates cleanly (no hang).
    assert parse_sse_frames(response.text) == []


def test_events_and_logs_are_independent_endpoints(client):
    """Goal 6 / SD-CONSOLE-002: never merge the two SSE streams."""
    execution_id = _execute(client)
    events_frames = parse_sse_frames(client.get(f"/api/executions/{execution_id}/events").text)
    logs_frames = parse_sse_frames(client.get(f"/api/executions/{execution_id}/logs").text)
    assert events_frames and events_frames[0]["type"] == "execution.failed"
    assert logs_frames == []
    assert all(isinstance(frame, dict) for frame in events_frames)


def test_events_404_for_unknown_execution_id(client):
    assert client.get("/api/executions/does-not-exist/events").status_code == 404


def test_logs_404_for_unknown_execution_id(client):
    assert client.get("/api/executions/does-not-exist/logs").status_code == 404


# ===========================================================================
# GET /api/executions/{id}/results
# ===========================================================================

def test_results_returns_generic_fallback_for_a_command_with_no_specific_adapter(client):
    execution_id = _execute(client)  # "build" -- no adapter registered for it
    response = client.get(f"/api/executions/{execution_id}/results")
    assert response.status_code == 200
    body = response.json()
    assert len(body) == 1
    assert body[0]["kind"] == "GenericResult"
    assert body[0]["payload"]["returncode"] == 0
    assert body[0]["payload"]["logTail"] == []


def test_results_dispatches_test_googletest_command_id_to_googletest_adapter(client, monkeypatch, tmp_path):
    from dia_console import results as results_module

    xml_path = tmp_path / "last_run.xml"
    xml_path.write_text(
        '<?xml version="1.0" encoding="UTF-8"?>'
        '<testsuites tests="1" failures="0">'
        '<testsuite name="S" tests="1" failures="0">'
        '<testcase name="T" classname="S" time="0.001" />'
        "</testsuite></testsuites>",
        encoding="utf-8",
    )
    monkeypatch.setattr(results_module, "_GTEST_XML_PATH", xml_path)

    execution_id = _execute(client, command_id="test.googletest", arguments={}, options={})
    response = client.get(f"/api/executions/{execution_id}/results")
    assert response.status_code == 200
    body = response.json()
    assert body[0]["kind"] == "TestResult"
    assert body[0]["payload"] == {"passCount": 1, "failCount": 0, "totalCount": 1}
    assert any(record["kind"] == "Artifact" for record in body)


def test_results_404_for_unknown_execution_id(client):
    assert client.get("/api/executions/does-not-exist/results").status_code == 404


# ===========================================================================
# POST /api/executions/{id}/cancel
# ===========================================================================

def test_cancel_calls_handle_cancel_and_returns_204(client, fake_service):
    execution_id = _execute(client)
    handle = fake_service.handles[execution_id]
    assert handle.cancelled is False

    response = client.post(f"/api/executions/{execution_id}/cancel")
    assert response.status_code == 204
    assert handle.cancelled is True


def test_cancel_404_for_unknown_execution_id(client):
    assert client.post("/api/executions/does-not-exist/cancel").status_code == 404


# ===========================================================================
# GET /api/context
# ===========================================================================

def test_get_context_shape(context_client):
    body = context_client.get("/api/context").json()
    assert set(body.keys()) == {"targets", "configs", "platform", "defaults"}
    assert body["configs"] == ["Debug", "Release"]
    assert body["platform"] == "x64"
    assert body["defaults"] == {"target": "cluichetest", "config": "Debug"}


def test_get_context_excludes_hidden_targets(context_client):
    body = context_client.get("/api/context").json()
    names = {t["name"] for t in body["targets"]}
    assert names == {"cluichetest"}


def test_get_context_target_shape_uses_camelcase_app_name(context_client):
    body = context_client.get("/api/context").json()
    assert body["targets"] == [{"name": "cluichetest", "appName": "CluicheTest"}]


# ===========================================================================
# Nav grouping fallback (Data Contracts table, tested as a pure Python fn)
# ===========================================================================

@pytest.mark.parametrize("segment,expected", [
    ("run", "Run & Debug"), ("launch", "Run & Debug"), ("pipeline", "Run & Debug"),
    ("fix", "Run & Debug"), ("diagnose", "Run & Debug"),
    ("test", "Test & Quality"), ("check", "Test & Quality"),
    ("asset", "Assets & Data"), ("reflect", "Assets & Data"),
    ("scaffold", "Create & Docs"), ("docs", "Create & Docs"), ("codegen", "Create & Docs"),
    ("env", "Environment"),
])
def test_group_label_for_known_top_level_segments(segment, expected):
    assert nav_grouping.group_label_for(segment) == expected


@pytest.mark.parametrize("segment", [
    "api", "show", "capture", "agent", "command",
    "cli_validate", "cli_check", "something_invented_later", "",
])
def test_group_label_for_unmapped_segment_falls_back_to_advanced(segment):
    assert nav_grouping.group_label_for(segment) == nav_grouping.ADVANCED_GROUP_LABEL


# ===========================================================================
# /api/presets CRUD (console-presets.md, Task 14)
# ===========================================================================

def _write_presets_file(tmp_path, filename, text):
    dia_dir = tmp_path / ".dia"
    dia_dir.mkdir(exist_ok=True)
    (dia_dir / filename).write_text(text, encoding="utf-8")


def test_get_presets_returns_merged_list_with_camelcase_command_id(client, tmp_path):
    _write_presets_file(
        tmp_path, "console-presets.yaml",
        "presets:\n  - id: a\n    name: A\n    command: build\n    values:\n      target: x\n",
    )
    body = client.get("/api/presets").json()
    assert body == [{"id": "a", "name": "A", "commandId": "build", "values": {"target": "x"}, "source": "shared"}]


def test_get_presets_empty_when_no_files_exist(client):
    assert client.get("/api/presets").json() == []


def test_post_presets_creates_a_local_scoped_preset(client, tmp_path):
    response = client.post("/api/presets", json={
        "id": "new-one", "name": "New One", "commandId": "build", "values": {"target": "cluichetest"},
    })
    assert response.status_code == 200, response.text
    body = response.json()
    assert body == {
        "id": "new-one", "name": "New One", "commandId": "build",
        "values": {"target": "cluichetest"}, "source": "local",
    }
    assert (tmp_path / ".dia" / "console-presets.local.yaml").exists()
    assert not (tmp_path / ".dia" / "console-presets.yaml").exists()


def test_put_presets_writes_back_to_the_presets_own_source_file(client, tmp_path):
    _write_presets_file(
        tmp_path, "console-presets.yaml",
        "presets:\n  - id: dup\n    name: Old\n    command: build\n    values:\n      target: x\n",
    )
    response = client.put("/api/presets/dup", json={"name": "New Name"})
    assert response.status_code == 200, response.text
    body = response.json()
    assert body["name"] == "New Name"
    assert body["source"] == "shared"
    assert body["values"] == {"target": "x"}  # untouched -- PUT body omitted "values"

    shared_text = (tmp_path / ".dia" / "console-presets.yaml").read_text(encoding="utf-8")
    assert "New Name" in shared_text


def test_put_presets_404_for_unknown_id(client):
    response = client.put("/api/presets/does-not-exist", json={"name": "x"})
    assert response.status_code == 404


def test_delete_presets_removes_from_its_own_source_file_and_returns_204(client, tmp_path):
    _write_presets_file(
        tmp_path, "console-presets.local.yaml",
        "presets:\n  - id: gone\n    name: Gone\n    command: build\n    values: {}\n",
    )
    response = client.delete("/api/presets/gone")
    assert response.status_code == 204
    assert client.get("/api/presets").json() == []


def test_delete_presets_404_for_unknown_id(client):
    assert client.delete("/api/presets/does-not-exist").status_code == 404


# ===========================================================================
# Real end-to-end: `dia asset build --target cluichetest` (Task 11, AC)
# ===========================================================================

@pytest.mark.integration
def test_real_asset_build_streams_through_full_http_stack():
    from dia_console.web.app import app as real_app

    # Must use the `with` (context-manager) form: it keeps one persistent
    # anyio portal/event loop alive across all calls on this client. Without
    # it, TestClient spins up a *fresh* throwaway portal per call (see
    # starlette.testclient.TestClient._portal_factory) -- the real
    # subprocess's stdout-pump task, started during POST /api/execute on the
    # first portal's loop, would then get cancelled the instant that portal
    # tears down, before the GET .../logs call's own (new) portal ever gets
    # a chance to read anything. This is purely a TestClient testing
    # artifact -- the real server (uvicorn, one persistent loop for its
    # whole lifetime) never hits this.
    with TestClient(real_app) as real_client:
        execute_response = real_client.post("/api/execute", json={
            "command_id": "asset.build",
            "project_id": "cluichetest",
            "arguments": {},
            "options": {"target": "cluichetest"},
        })
        assert execute_response.status_code == 200, execute_response.text
        execution_id = execute_response.json()["execution_id"]

        logs_response = real_client.get(f"/api/executions/{execution_id}/logs")
        assert logs_response.status_code == 200
        log_lines = parse_sse_frames(logs_response.text)
        assert log_lines, "expected at least one real log line"

        events_response = real_client.get(f"/api/executions/{execution_id}/events")
        assert events_response.status_code == 200
        event_frames = parse_sse_frames(events_response.text)
        assert event_frames[0]["type"] == "execution.started", event_frames[:3]
        assert event_frames[-1]["type"] == "execution.completed", event_frames[-3:]
