"""FastAPI app for DiaConsole (Goals 2-7, SD-CONSOLE-005).

This module is the *only* place that touches ``dia_console.model`` /
``registry`` / ``execution`` directly for the HTTP layer -- ``app.js`` only
ever calls the routes defined here; it never constructs subprocess calls,
talks to Click, or bypasses :class:`ExecutionService`.

Routes (see console-native-shell.md's Data Contracts):

    GET  /                              -> shell HTML
    GET  /static/*                      -> mounted static assets (JS/CSS)
    GET  /api/commands                  -> JSON array of CommandDescriptor
    GET  /api/context                   -> target/config/platform context
    POST /api/execute                   -> {"execution_id": str}
    GET  /api/executions/{id}/events    -> SSE stream of ExecutionEvent JSON
    GET  /api/executions/{id}/logs      -> SSE stream of raw text lines
    GET  /api/executions/{id}/results   -> JSON array of ResultRecord
    POST /api/executions/{id}/cancel    -> 204 No Content
    GET  /api/status                    -> reflection health + execution metrics
    GET    /api/presets                 -> JSON array of PresetDescriptor
    POST   /api/presets                 -> the created PresetDescriptor (scope="local")
    PUT    /api/presets/{id}            -> the updated PresetDescriptor
    DELETE /api/presets/{id}            -> 204 No Content

``/events`` and ``/logs`` are two independent SSE connections, never merged
into one endpoint (Goal 6, SD-CONSOLE-002).
"""
from __future__ import annotations

import dataclasses
import json
import time
from pathlib import Path
from typing import Any, AsyncIterator

from fastapi import FastAPI, HTTPException
from fastapi.encoders import jsonable_encoder
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from loguru import logger
from pydantic import BaseModel, ConfigDict, Field

from dia_cli.cli_main import cli as dia_cli_app
from dia_cli.commands.pipeline import pipeline_config
from dia_console.context import list_targets
from dia_console.execution import ExecutionHandle, ExecutionService
from dia_console.execution import get_metrics as get_execution_metrics
from dia_console.model import CommandDescriptor, ExecuteCommandRequest
from dia_console.presets import PresetDescriptor, delete_preset, load_presets, save_preset
from dia_console.registry import CommandRegistry
from dia_console.results import build_results
from dia_console.web.nav_grouping import group_label_for

#: Fixed per CLAUDE.md's own Configurations section -- not derived from
#: pipeline.toml, which has no such list.
CONFIGS: list[str] = ["Debug", "Release"]

#: Fixed per PD-005 (x64 is the only supported platform) -- no dia_cli/cli/*.py
#: command has a --platform option at all, so there is nothing to select.
PLATFORM = "x64"

STATIC_DIR = Path(__file__).parent / "static"
INDEX_HTML = STATIC_DIR / "index.html"


class ExecuteCommandRequestBody(BaseModel):
    """Wire shape of a ``POST /api/execute`` body -> :class:`ExecuteCommandRequest`."""

    command_id: str
    project_id: str | None = None
    arguments: dict[str, Any] = {}
    options: dict[str, Any] = {}


class CreatePresetRequestBody(BaseModel):
    """Wire shape of a ``POST /api/presets`` body (console-presets.md Data Contracts).

    ``commandId`` is camelCase on the wire, like ``/api/presets``'s own
    response shape -- ``populate_by_name`` lets tests/callers also pass the
    Python field name directly.
    """

    model_config = ConfigDict(populate_by_name=True)

    id: str
    name: str
    command_id: str = Field(alias="commandId")
    values: dict[str, Any] = {}


class UpdatePresetRequestBody(BaseModel):
    """Wire shape of a ``PUT /api/presets/{id}`` body -- both fields optional."""

    name: str | None = None
    values: dict[str, Any] | None = None


def _default_execution_service() -> ExecutionService:
    """A real ExecutionService wired to the real DiaCLI command tree."""
    return ExecutionService(CommandRegistry.from_click_app(dia_cli_app))


def _descriptor_to_json(descriptor: CommandDescriptor) -> dict:
    """A CommandDescriptor as JSON, enriched with its resolved nav ``groupLabel``.

    ``groupLabel`` is not one of CommandDescriptor's own fields (this feature
    does not modify ``dia_console/model.py``) -- it is computed here, once,
    from the fixed nav-grouping table, and shipped alongside the raw
    descriptor so ``app.js`` never re-implements that table in JavaScript.
    """
    payload = dataclasses.asdict(descriptor)
    payload["groupLabel"] = group_label_for(descriptor.path[0])
    return payload


def _preset_to_json(preset: PresetDescriptor) -> dict:
    """A PresetDescriptor as JSON, per console-presets.md's Data Contracts table.

    ``commandId`` is deliberately camelCase (like ``/api/context``'s
    ``appName``) -- everything else (``id``/``name``/``values``/``source``)
    is already the same in both casings.
    """
    return {
        "id": preset.id,
        "name": preset.name,
        "commandId": preset.command_id,
        "values": dict(preset.values),
        "source": preset.source,
    }


def create_app(execution_service: ExecutionService | None = None) -> FastAPI:
    """Build the DiaConsole FastAPI app.

    ``execution_service`` defaults to a real one wired to the real
    ``CommandRegistry.from_click_app(dia_cli_app)`` -- built once here, at
    app-construction time, not per-request. (SD-CONSOLE-010's "no re-reflection"
    rule governs the *registry*'s own construction path, not server
    lifecycle -- building it once when the server starts is a normal
    server-lifecycle choice, not a violation.) Tests inject a fake service so
    they never reflect the real CLI tree or spawn real ``dia`` subprocesses
    except in the one test explicitly marked ``@pytest.mark.integration``.
    """
    app = FastAPI(title="Dia Console")
    app.state.execution_service = execution_service or _default_execution_service()
    app.state.executions: dict[str, ExecutionHandle] = {}
    app.state.execution_commands: dict[str, str] = {}
    app.state.started_at = time.monotonic()

    app.mount("/static", StaticFiles(directory=str(STATIC_DIR)), name="static")

    def _handle_or_404(execution_id: str) -> ExecutionHandle:
        handle = app.state.executions.get(execution_id)
        if handle is None:
            raise HTTPException(status_code=404, detail=f"Unknown execution id: {execution_id!r}")
        return handle

    @app.get("/")
    async def index() -> FileResponse:
        return FileResponse(str(INDEX_HTML))

    @app.get("/api/commands")
    async def get_commands() -> list[dict]:
        registry: CommandRegistry = app.state.execution_service.registry
        return [_descriptor_to_json(descriptor) for descriptor in registry.commands]

    @app.get("/api/context")
    async def get_context() -> dict:
        """Target/config/platform context (console-project-context.md).

        Target/config context ONLY, within the one real project in this repo
        -- no project-level (cross-repo/sibling) switching of any kind.
        """
        repo_root: Path = app.state.execution_service.repo_root
        cfg = pipeline_config.load_pipeline_config(repo_root)
        targets = list_targets(repo_root)
        return {
            "targets": [{"name": t.name, "appName": t.app_name} for t in targets],
            "configs": CONFIGS,
            "platform": PLATFORM,
            "defaults": {
                "target": cfg.global_cfg.default_target,
                "config": cfg.global_cfg.default_config,
            },
        }

    @app.get("/api/status")
    async def get_status() -> dict:
        """Reflection health + execution metrics (Observation Opportunity Scan
        finding #13). Sourced from the registry already built at app-construction
        time (SD-CONSOLE-010) and the process-lifetime :class:`ExecutionMetrics`
        counters -- never re-reflects, never blocks.
        """
        registry: CommandRegistry = app.state.execution_service.registry
        metrics = get_execution_metrics()
        return {
            "commandsCount": len(registry),
            "reflectionErrors": [
                {"commandId": command_id, "message": message}
                for command_id, message in registry.reflection_errors
            ],
            "uptimeSeconds": time.monotonic() - app.state.started_at,
            "executions": {
                "started": metrics.started,
                "completed": metrics.completed,
                "failed": metrics.failed,
                "cancelled": metrics.cancelled,
            },
        }

    @app.post("/api/execute")
    async def execute(body: ExecuteCommandRequestBody) -> dict:
        request = ExecuteCommandRequest(
            command_id=body.command_id,
            project_id=body.project_id,
            arguments=body.arguments,
            options=body.options,
        )
        try:
            handle = await app.state.execution_service.execute(request)
        except KeyError as exc:
            logger.warning("Execute request for unknown command_id={!r}", body.command_id)
            raise HTTPException(status_code=404, detail=str(exc)) from exc
        app.state.executions[handle.execution_id] = handle
        app.state.execution_commands[handle.execution_id] = request.command_id
        return {"execution_id": handle.execution_id}

    @app.get("/api/executions/{execution_id}/events")
    async def stream_events(execution_id: str) -> StreamingResponse:
        handle = _handle_or_404(execution_id)

        async def frames() -> AsyncIterator[str]:
            async for event in handle.events():
                yield f"data: {json.dumps(jsonable_encoder(event))}\n\n"

        return StreamingResponse(frames(), media_type="text/event-stream")

    @app.get("/api/executions/{execution_id}/logs")
    async def stream_logs(execution_id: str) -> StreamingResponse:
        handle = _handle_or_404(execution_id)

        async def frames() -> AsyncIterator[str]:
            async for line in handle.log_lines():
                yield f"data: {json.dumps(line)}\n\n"

        return StreamingResponse(frames(), media_type="text/event-stream")

    @app.get("/api/executions/{execution_id}/results")
    async def get_results(execution_id: str) -> list[dict]:
        handle = _handle_or_404(execution_id)
        command_id = app.state.execution_commands.get(execution_id, "")
        records = build_results(command_id, handle)
        return [dataclasses.asdict(record) for record in records]

    @app.post("/api/executions/{execution_id}/cancel", status_code=204)
    async def cancel(execution_id: str) -> None:
        handle = _handle_or_404(execution_id)
        await handle.cancel()

    def _preset_or_404(repo_root: Path, preset_id: str) -> PresetDescriptor:
        for preset in load_presets(repo_root):
            if preset.id == preset_id:
                return preset
        raise HTTPException(status_code=404, detail=f"Unknown preset id: {preset_id!r}")

    @app.get("/api/presets")
    async def get_presets() -> list[dict]:
        repo_root: Path = app.state.execution_service.repo_root
        return [_preset_to_json(preset) for preset in load_presets(repo_root)]

    @app.post("/api/presets")
    async def create_preset(body: CreatePresetRequestBody) -> dict:
        repo_root: Path = app.state.execution_service.repo_root
        preset = PresetDescriptor(
            id=body.id,
            name=body.name,
            command_id=body.command_id,
            values=body.values,
            source="local",
        )
        save_preset(repo_root, preset, scope="local")
        return _preset_to_json(preset)

    @app.put("/api/presets/{preset_id}")
    async def update_preset(preset_id: str, body: UpdatePresetRequestBody) -> dict:
        repo_root: Path = app.state.execution_service.repo_root
        existing = _preset_or_404(repo_root, preset_id)
        updated = PresetDescriptor(
            id=existing.id,
            name=body.name if body.name is not None else existing.name,
            command_id=existing.command_id,
            values=body.values if body.values is not None else existing.values,
            source=existing.source,
        )
        save_preset(repo_root, updated, scope=existing.source)
        return _preset_to_json(updated)

    @app.delete("/api/presets/{preset_id}", status_code=204)
    async def remove_preset(preset_id: str) -> None:
        repo_root: Path = app.state.execution_service.repo_root
        existing = _preset_or_404(repo_root, preset_id)
        delete_preset(repo_root, existing.id, scope=existing.source)

    return app


#: Module-level singleton -- what ``uvicorn dia_console.web.app:app`` and
#: ``dia_console.shell.launch()`` both use by default. Built once, at import.
app = create_app()
