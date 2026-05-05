"""Unit tests for BuildRunner — phase dispatch, error collection, stage filtering, NDJSON events."""
from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import MagicMock

import pytest

from dia_cli.commands.asset.context import BuildContext
from dia_cli.commands.asset.handler import AssetError, AssetHandler, DeployResult, TransformResult
from dia_cli.commands.asset.registry import AssetHandlerRegistry
from dia_cli.commands.asset.runner import BuildRunner, _build_stage_membership, _type_from_id


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_output() -> MagicMock:
    out = MagicMock()
    out.emit = MagicMock()
    out.warn = MagicMock()
    out.run_started = MagicMock()
    out.run_completed = MagicMock()
    out.run_failed = MagicMock()
    return out


def _make_context(catalogue: dict, asset_stages: list[str] | None = None) -> BuildContext:
    return BuildContext(
        catalogue=catalogue,
        config="Debug",
        platform="x64",
        app_name="TestApp",
        deploy_root=Path("/bin/TestApp/Debug/x64/assets"),
        asset_stages=asset_stages or [],
        output=_make_output(),
        source_root=Path("/src"),
    )


def _make_record(
    asset_id: str,
    scope: str = "global",
    stage_name: str = "",
    references: list[dict] | None = None,
) -> dict:
    return {
        "id": asset_id,
        "type": asset_id.split(".")[0],
        "source_path": f"Raw/{asset_id}.bin",
        "content_hash": 0,
        "status": "active",
        "scope": scope,
        "stage_name": stage_name,
        "tags": [],
        "references": references or [],
    }


def _make_stage_record(stage_id: str, members: list[str]) -> dict:
    return {
        "id": stage_id,
        "type": "stage",
        "source_path": "",
        "content_hash": 0,
        "status": "active",
        "scope": "global",
        "stage_name": "",
        "tags": [],
        "references": [{"type": "contains", "target": m} for m in members],
    }


# ---------------------------------------------------------------------------
# _type_from_id
# ---------------------------------------------------------------------------

def test_type_from_id_extracts_prefix():
    assert _type_from_id("texture.player") == "texture"


def test_type_from_id_no_dot_returns_full():
    assert _type_from_id("texture") == "texture"


# ---------------------------------------------------------------------------
# _build_stage_membership
# ---------------------------------------------------------------------------

def test_stage_membership_maps_contained_assets():
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg", "audio.music"]),
        ]
    }
    m = _build_stage_membership(catalogue)
    assert m["texture.bg"] == ["stage.menu"]
    assert m["audio.music"] == ["stage.menu"]


def test_stage_membership_multiple_stages():
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg"]),
            _make_stage_record("stage.level1", ["texture.bg", "texture.enemy"]),
        ]
    }
    m = _build_stage_membership(catalogue)
    assert set(m["texture.bg"]) == {"stage.menu", "stage.level1"}
    assert m["texture.enemy"] == ["stage.level1"]


def test_stage_membership_ignores_non_stage_assets():
    catalogue = {
        "assets": [
            {**_make_record("texture.bg"), "references": [{"type": "uses", "target": "config.x"}]},
        ]
    }
    m = _build_stage_membership(catalogue)
    assert m == {}


# ---------------------------------------------------------------------------
# Stage filtering (_should_include_asset)
# ---------------------------------------------------------------------------

def test_global_assets_always_included():
    catalogue = {"assets": [_make_record("texture.bg", scope="global")]}
    ctx = _make_context(catalogue, asset_stages=[])
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner._should_include_asset(catalogue["assets"][0]) is True


def test_stage_asset_included_when_stage_in_list():
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg"]),
            _make_record("texture.bg", scope="stage", stage_name="menu"),
        ]
    }
    ctx = _make_context(catalogue, asset_stages=["stage.menu"])
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner._should_include_asset(catalogue["assets"][1]) is True


def test_stage_asset_excluded_when_stage_not_in_list():
    catalogue = {
        "assets": [
            _make_stage_record("stage.menu", ["texture.bg"]),
            _make_record("texture.bg", scope="stage", stage_name="menu"),
        ]
    }
    ctx = _make_context(catalogue, asset_stages=["stage.level1"])
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner._should_include_asset(catalogue["assets"][1]) is False


def test_orphaned_asset_included_with_warning():
    catalogue = {"assets": [_make_record("texture.bg", scope="stage")]}
    ctx = _make_context(catalogue, asset_stages=[])
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner._should_include_asset(catalogue["assets"][0]) is True
    ctx.output.warn.assert_called_once()


# ---------------------------------------------------------------------------
# BuildRunner.run — phase dispatch and exit codes
# ---------------------------------------------------------------------------

def test_run_returns_zero_on_empty_catalogue():
    ctx = _make_context({"assets": []})
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner.run() == 0


def test_run_returns_zero_when_all_pass():
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner.run() == 0


def test_run_returns_one_when_validate_fails():
    class FailHandler(AssetHandler):
        type_id = "texture"
        def validate(self, record, context):
            return [AssetError(asset_id=record["id"], phase="validate", message="bad")]

    reg = AssetHandlerRegistry()
    reg.register(FailHandler())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    assert BuildRunner(reg, ctx).run() == 1


def test_run_returns_one_when_transform_fails():
    class FailHandler(AssetHandler):
        type_id = "texture"
        def transform(self, record, context):
            return TransformResult(success=False, errors=[
                AssetError(asset_id=record["id"], phase="transform", message="bad")
            ])

    reg = AssetHandlerRegistry()
    reg.register(FailHandler())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    assert BuildRunner(reg, ctx).run() == 1


def test_run_returns_one_when_deploy_fails():
    class FailHandler(AssetHandler):
        type_id = "texture"
        def deploy(self, record, context):
            return DeployResult(success=False, errors=[
                AssetError(asset_id=record["id"], phase="deploy", message="bad")
            ])

    reg = AssetHandlerRegistry()
    reg.register(FailHandler())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    assert BuildRunner(reg, ctx).run() == 1


def test_run_no_early_exit_processes_all_assets():
    """A failed asset must not prevent other assets from being processed."""
    processed = []

    class TrackHandler(AssetHandler):
        type_id = "texture"
        def validate(self, record, context):
            processed.append(record["id"])
            if record["id"] == "texture.bad":
                return [AssetError(asset_id=record["id"], phase="validate", message="bad")]
            return []

    reg = AssetHandlerRegistry()
    reg.register(TrackHandler())
    catalogue = {
        "assets": [
            _make_record("texture.bad"),
            _make_record("texture.good"),
        ]
    }
    ctx = _make_context(catalogue)
    code = BuildRunner(reg, ctx).run()
    assert code == 1
    assert "texture.bad" in processed
    assert "texture.good" in processed


def test_validate_failure_skips_transform_and_deploy():
    transform_called = []
    deploy_called = []

    class FailValidate(AssetHandler):
        type_id = "texture"
        def validate(self, record, context):
            return [AssetError(asset_id=record["id"], phase="validate", message="bad")]
        def transform(self, record, context):
            transform_called.append(record["id"])
            return TransformResult(success=True)
        def deploy(self, record, context):
            deploy_called.append(record["id"])
            return DeployResult(success=True)

    reg = AssetHandlerRegistry()
    reg.register(FailValidate())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    BuildRunner(reg, ctx).run()
    assert transform_called == []
    assert deploy_called == []


def test_run_uses_base_handler_for_unregistered_type():
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    assert runner.run() == 0


# ---------------------------------------------------------------------------
# Phase selection
# ---------------------------------------------------------------------------

def test_run_validate_only_phase():
    deploy_called = []

    class TrackDeploy(AssetHandler):
        type_id = "texture"
        def deploy(self, record, context):
            deploy_called.append(True)
            return DeployResult(success=True)

    reg = AssetHandlerRegistry()
    reg.register(TrackDeploy())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    BuildRunner(reg, ctx).run(phases=["validate"])
    assert deploy_called == []


def test_run_invalid_phase_raises():
    ctx = _make_context({"assets": []})
    runner = BuildRunner(AssetHandlerRegistry(), ctx)
    with pytest.raises(ValueError, match="Invalid phases"):
        runner.run(phases=["compile"])


# ---------------------------------------------------------------------------
# NDJSON events
# ---------------------------------------------------------------------------

def test_events_emitted_for_successful_asset():
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    BuildRunner(AssetHandlerRegistry(), ctx).run()

    emitted = [call.args[0]["event"] for call in ctx.output.emit.call_args_list]
    assert "OnAssetValidated" in emitted
    assert "OnAssetTransformed" in emitted
    assert "OnAssetDeployed" in emitted
    assert "OnBuildCompleted" in emitted


def test_on_asset_failed_emitted_on_validate_failure():
    class FailHandler(AssetHandler):
        type_id = "texture"
        def validate(self, record, context):
            return [AssetError(asset_id=record["id"], phase="validate", message="oops")]

    reg = AssetHandlerRegistry()
    reg.register(FailHandler())
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    BuildRunner(reg, ctx).run()

    emitted = [call.args[0]["event"] for call in ctx.output.emit.call_args_list]
    assert "OnAssetFailed" in emitted


def test_on_build_completed_has_counts():
    class FailHandler(AssetHandler):
        type_id = "texture"
        def validate(self, record, context):
            return [AssetError(asset_id=record["id"], phase="validate", message="bad")]

    reg = AssetHandlerRegistry()
    reg.register(FailHandler())
    catalogue = {
        "assets": [
            _make_record("texture.bad"),
            _make_record("config.ok"),  # no handler -> default pass
        ]
    }
    ctx = _make_context(catalogue)
    BuildRunner(reg, ctx).run()

    completed = next(
        call.args[0] for call in ctx.output.emit.call_args_list
        if call.args[0]["event"] == "OnBuildCompleted"
    )
    assert completed["passCount"] == 1
    assert completed["failCount"] == 1
    assert "totalDurationMs" in completed


def test_events_include_ts_field():
    catalogue = {"assets": [_make_record("texture.bg")]}
    ctx = _make_context(catalogue)
    BuildRunner(AssetHandlerRegistry(), ctx).run()
    for call in ctx.output.emit.call_args_list:
        assert "ts" in call.args[0], f"Missing ts in event: {call.args[0]}"
