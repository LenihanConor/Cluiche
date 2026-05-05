from __future__ import annotations

import time
from datetime import datetime, timezone
from typing import TYPE_CHECKING

from .handler import AssetError, AssetHandler, DeployResult, TransformResult
from .registry import AssetHandlerRegistry

if TYPE_CHECKING:
    from .context import BuildContext

_SYSTEM = "asset"
_VALID_PHASES = ("validate", "transform", "deploy")


class BuildRunner:
    def __init__(self, registry: AssetHandlerRegistry, context: "BuildContext") -> None:
        self._registry = registry
        self._ctx = context
        # Pre-build reverse map: asset_id -> list of stage asset IDs that contain it.
        # Uses `contains` edges stored on stage asset records in the catalogue.
        self._stage_membership: dict[str, list[str]] = _build_stage_membership(context.catalogue)

    # ------------------------------------------------------------------ #
    # Public                                                               #
    # ------------------------------------------------------------------ #

    def run(self, phases: list[str] | None = None) -> int:
        if phases is None:
            phases = list(_VALID_PHASES)

        invalid = [p for p in phases if p not in _VALID_PHASES]
        if invalid:
            raise ValueError(f"Invalid phases: {invalid!r}. Must be subset of {_VALID_PHASES!r}")

        start = time.time()
        pass_count = 0
        fail_count = 0

        for record in self._ctx.catalogue.get("assets", []):
            asset_id = record.get("id", "")

            if not self._should_include_asset(record):
                continue

            handler = self._registry.get(_type_from_id(asset_id)) or AssetHandler()
            errors: list[AssetError] = []
            failed = False

            if "validate" in phases:
                errors = self._run_validate(record, handler)
                if errors:
                    failed = True
                    self._emit_asset_failed(asset_id, handler.type_id, "validate", errors)

            if not failed and "transform" in phases:
                result = self._run_transform(record, handler)
                if not result.success:
                    failed = True
                    self._emit_asset_failed(asset_id, handler.type_id, "transform", result.errors)

            if not failed and "deploy" in phases:
                result = self._run_deploy(record, handler)
                if not result.success:
                    failed = True
                    self._emit_asset_failed(asset_id, handler.type_id, "deploy", result.errors)

            if failed:
                fail_count += 1
            else:
                pass_count += 1

        total_ms = int((time.time() - start) * 1000)
        self._emit_build_completed(pass_count, fail_count, total_ms)

        return 0 if fail_count == 0 else 1

    # ------------------------------------------------------------------ #
    # Filtering                                                            #
    # ------------------------------------------------------------------ #

    def _should_include_asset(self, record: dict) -> bool:
        scope = record.get("scope", "global")
        if scope == "global":
            return True

        asset_id = record.get("id", "")
        if not asset_id:
            return True

        # Warn and include orphaned assets (in no stage)
        containing_stages = self._stage_membership.get(asset_id, [])
        if not containing_stages:
            self._ctx.output.warn(
                _SYSTEM,
                f"Orphaned asset '{asset_id}' is not contained by any stage — deploying to global/misc/",
            )
            return True

        return any(s in self._ctx.asset_stages for s in containing_stages)

    # ------------------------------------------------------------------ #
    # Phase dispatch                                                       #
    # ------------------------------------------------------------------ #

    def _run_validate(self, record: dict, handler: AssetHandler) -> list[AssetError]:
        asset_id = record.get("id", "")
        errors = handler.validate(record, self._ctx)
        self._emit({
            "event": "OnAssetValidated",
            "system": _SYSTEM,
            "assetId": asset_id,
            "typeId": handler.type_id,
            "errors": [{"assetId": e.asset_id, "phase": e.phase, "message": e.message} for e in errors],
            "ts": _now_iso(),
        })
        return errors

    def _run_transform(self, record: dict, handler: AssetHandler) -> TransformResult:
        asset_id = record.get("id", "")
        t0 = time.time()
        result = handler.transform(record, self._ctx)
        duration_ms = int((time.time() - t0) * 1000)
        self._emit({
            "event": "OnAssetTransformed",
            "system": _SYSTEM,
            "assetId": asset_id,
            "typeId": handler.type_id,
            "durationMs": duration_ms,
            "ts": _now_iso(),
        })
        return result

    def _run_deploy(self, record: dict, handler: AssetHandler) -> DeployResult:
        asset_id = record.get("id", "")
        t0 = time.time()
        result = handler.deploy(record, self._ctx)
        duration_ms = int((time.time() - t0) * 1000)
        self._emit({
            "event": "OnAssetDeployed",
            "system": _SYSTEM,
            "assetId": asset_id,
            "destPath": result.deploy_path or "",
            "durationMs": duration_ms,
            "ts": _now_iso(),
        })
        return result

    # ------------------------------------------------------------------ #
    # Event helpers                                                        #
    # ------------------------------------------------------------------ #

    def _emit_asset_failed(
        self,
        asset_id: str,
        type_id: str,
        phase: str,
        errors: list[AssetError],
    ) -> None:
        self._emit({
            "event": "OnAssetFailed",
            "system": _SYSTEM,
            "assetId": asset_id,
            "typeId": type_id,
            "phase": phase,
            "errors": [{"assetId": e.asset_id, "phase": e.phase, "message": e.message} for e in errors],
            "ts": _now_iso(),
        })

    def _emit_build_completed(self, pass_count: int, fail_count: int, total_ms: int) -> None:
        self._emit({
            "event": "OnBuildCompleted",
            "system": _SYSTEM,
            "passCount": pass_count,
            "failCount": fail_count,
            "totalDurationMs": total_ms,
            "ts": _now_iso(),
        })

    def _emit(self, payload: dict) -> None:
        self._ctx.output.emit(payload)


# ------------------------------------------------------------------ #
# Module-level helpers                                                 #
# ------------------------------------------------------------------ #

def _type_from_id(asset_id: str) -> str:
    """Extract the type prefix from a 'type.name' composite ID."""
    return asset_id.split(".")[0] if "." in asset_id else asset_id


def _build_stage_membership(catalogue: dict) -> dict[str, list[str]]:
    """Build reverse map: asset_id -> [stage_asset_ids that contain it].

    Stage assets use `contains` edges in their references list to declare
    which assets belong to that stage.
    """
    membership: dict[str, list[str]] = {}
    for record in catalogue.get("assets", []):
        asset_id = record.get("id", "")
        asset_type = _type_from_id(asset_id)
        if asset_type != "stage":
            continue
        for ref in record.get("references", []):
            if ref.get("type") == "contains":
                target = ref.get("target", "")
                if target:
                    membership.setdefault(target, []).append(asset_id)
    return membership


def _now_iso() -> str:
    return datetime.now(timezone.utc).isoformat()
