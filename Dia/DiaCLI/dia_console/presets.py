"""Named, saved command+argument presets for DiaConsole (console-presets.md).

Two YAML files store presets per repo: a committed, team-shared
``.dia/console-presets.yaml`` and a gitignored, personal
``.dia/console-presets.local.yaml``. Both are loaded and merged by ``id``;
the local file wins on collision (SD-CONSOLE-011). A missing file for either
source degrades to an empty list for that source -- never an error, since a
fresh clone normally has no local file at all.

UI-independent (SD-CONSOLE-005): this module never imports anything from
``dia_console.web`` -- the web routes call into this module, never the
reverse.
"""
from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Literal, Mapping

import yaml

from dia_console.model import CommandDescriptor, ExecuteCommandRequest

#: Filenames under ``<repo_root>/.dia/`` -- see the module docstring.
_SHARED_FILENAME = "console-presets.yaml"
_LOCAL_FILENAME = "console-presets.local.yaml"


@dataclass(frozen=True)
class PresetDescriptor:
    """A named, saved command+argument combination.

    ``source`` records which file this preset currently lives in -- ``"local"``
    if it won an id collision or only ever existed there, ``"shared"``
    otherwise. Needed so :func:`save_preset`/:func:`delete_preset` write back
    to the right file.
    """

    id: str
    name: str
    command_id: str
    values: Mapping[str, Any]
    source: Literal["shared", "local"]


def _preset_path(repo_root: Path, scope: Literal["shared", "local"]) -> Path:
    filename = _SHARED_FILENAME if scope == "shared" else _LOCAL_FILENAME
    return repo_root / ".dia" / filename


def _read_raw_presets(path: Path) -> list[dict]:
    """The ``presets`` list of one preset file. Missing/empty/malformed-top-level
    file degrades to ``[]`` -- never raises."""
    if not path.exists():
        return []
    with open(path, "r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle) or {}
    if not isinstance(data, dict):
        return []
    return list(data.get("presets") or [])


def _write_raw_presets(path: Path, raws: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        yaml.safe_dump({"presets": raws}, handle, sort_keys=False)


def _descriptor_from_raw(raw: Mapping[str, Any], source: Literal["shared", "local"]) -> PresetDescriptor:
    return PresetDescriptor(
        id=raw["id"],
        name=raw.get("name", raw["id"]),
        command_id=raw["command"],
        values=dict(raw.get("values") or {}),
        source=source,
    )


def _raw_from_descriptor(preset: PresetDescriptor) -> dict:
    return {
        "id": preset.id,
        "name": preset.name,
        "command": preset.command_id,
        "values": dict(preset.values),
    }


def load_presets(repo_root: Path) -> list[PresetDescriptor]:
    """Read and merge both preset files by ``id``; the local overlay wins on collision."""
    shared_raw = _read_raw_presets(_preset_path(repo_root, "shared"))
    local_raw = _read_raw_presets(_preset_path(repo_root, "local"))

    by_id: dict[str, PresetDescriptor] = {}
    for raw in shared_raw:
        descriptor = _descriptor_from_raw(raw, "shared")
        by_id[descriptor.id] = descriptor
    for raw in local_raw:
        descriptor = _descriptor_from_raw(raw, "local")
        by_id[descriptor.id] = descriptor  # local always wins on collision

    return list(by_id.values())


def save_preset(repo_root: Path, preset: PresetDescriptor, *, scope: Literal["shared", "local"]) -> None:
    """Read-modify-write only the ``scope`` file -- never the other one.

    Replaces an existing entry with the same ``id`` in that file, or appends
    a new one. Creates the file (starting from an empty ``presets: []``) if
    it doesn't exist yet. Idempotent: re-saving the same preset twice leaves
    the file in the same end state.
    """
    path = _preset_path(repo_root, scope)
    raws = _read_raw_presets(path)
    new_raw = _raw_from_descriptor(preset)
    for index, raw in enumerate(raws):
        if raw.get("id") == preset.id:
            raws[index] = new_raw
            break
    else:
        raws.append(new_raw)
    _write_raw_presets(path, raws)


def delete_preset(repo_root: Path, preset_id: str, *, scope: Literal["shared", "local"]) -> None:
    """Read-modify-write only the ``scope`` file, removing the entry with ``preset_id``.

    A no-op (not an error) if ``preset_id`` isn't present -- idempotent, same
    as :func:`save_preset`.
    """
    path = _preset_path(repo_root, scope)
    raws = _read_raw_presets(path)
    filtered = [raw for raw in raws if raw.get("id") != preset_id]
    _write_raw_presets(path, filtered)


def build_execute_request(
    descriptor: CommandDescriptor,
    values: Mapping[str, Any],
    project_id: str | None = None,
) -> ExecuteCommandRequest:
    """Split a preset's flat ``values`` into ``arguments``/``options`` via ``descriptor``.

    ``descriptor`` (not the preset file) is what knows which of a command's
    parameters are positional vs named, so the on-disk YAML never needs to
    encode that distinction. Keys that match neither an argument nor an
    option name (a stale/typo'd preset) are dropped silently, never passed
    through as bogus argv.
    """
    argument_names = {argument.name for argument in descriptor.arguments}
    option_names = {option.name for option in descriptor.options}

    arguments = {key: value for key, value in values.items() if key in argument_names}
    options = {key: value for key, value in values.items() if key in option_names}

    return ExecuteCommandRequest(
        command_id=descriptor.id,
        project_id=project_id,
        arguments=arguments,
        options=options,
    )
