"""Unit tests for dia_console/model.py (console-command-model feature).

Structural tests: dataclass shape, frozen-ness, defaults, and the
"no Click/subprocess types leak into the model" constraint.
"""
import dataclasses
import inspect
from datetime import datetime

import pytest

from dia_console import model
from dia_console.model import (
    ArgumentDescriptor,
    ArgumentType,
    CommandCapabilities,
    CommandDescriptor,
    ExecuteCommandRequest,
    ExecutionEvent,
    OptionDescriptor,
    ResultRecord,
)


def _field_names(cls):
    return tuple(f.name for f in dataclasses.fields(cls))


# ---------------------------------------------------------------------------
# ArgumentType
# ---------------------------------------------------------------------------

def test_argument_type_is_closed_six_value_str_enum():
    assert [m.value for m in ArgumentType] == ["string", "int", "float", "bool", "path", "choice"]
    # str enum so it serializes/compares as plain text for a future UI
    assert ArgumentType.CHOICE == "choice"
    assert isinstance(ArgumentType.PATH, str)


# ---------------------------------------------------------------------------
# Dataclass shapes match the spec's Data Contracts
# ---------------------------------------------------------------------------

def test_argument_descriptor_shape():
    assert _field_names(ArgumentDescriptor) == (
        "name", "help", "type", "required", "default", "multiple", "raw_click_type",
    )


def test_option_descriptor_shape():
    assert _field_names(OptionDescriptor) == (
        "name", "help", "type", "required", "default", "multiple", "is_flag",
        "choices", "raw_click_type", "cli_flag",
    )


def test_command_descriptor_shape():
    assert _field_names(CommandDescriptor) == (
        "id", "path", "name", "description", "category",
        "arguments", "options", "capabilities", "execution_binding",
    )


def test_execute_command_request_shape():
    assert _field_names(ExecuteCommandRequest) == ("command_id", "project_id", "arguments", "options")


def test_execution_event_shape():
    assert _field_names(ExecutionEvent) == ("execution_id", "seq", "time", "type", "step_id", "payload")


def test_result_record_shape():
    assert _field_names(ResultRecord) == ("kind", "severity", "title", "summary", "payload")


def test_command_capabilities_has_no_fields():
    """Neither spec defines a capability field yet — it stays empty, not invented."""
    assert dataclasses.fields(CommandCapabilities) == ()


# ---------------------------------------------------------------------------
# Frozen-ness
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("cls", [
    ArgumentDescriptor, OptionDescriptor, CommandCapabilities, CommandDescriptor,
    ExecuteCommandRequest, ExecutionEvent, ResultRecord,
])
def test_every_model_dataclass_is_frozen(cls):
    assert dataclasses.is_dataclass(cls)
    assert cls.__dataclass_params__.frozen is True


def test_frozen_instance_rejects_mutation():
    descriptor = CommandDescriptor(id="run", path=("run",), name="run", description="", category="")
    with pytest.raises(dataclasses.FrozenInstanceError):
        descriptor.id = "other"


# ---------------------------------------------------------------------------
# Defaults
# ---------------------------------------------------------------------------

def test_command_descriptor_defaults():
    descriptor = CommandDescriptor(id="run", path=("run",), name="run", description="d", category="")
    assert descriptor.arguments == ()
    assert descriptor.options == ()
    assert descriptor.capabilities == CommandCapabilities()
    assert descriptor.execution_binding == ""


def test_option_descriptor_defaults():
    option = OptionDescriptor(
        name="target", help="h", type=ArgumentType.STRING,
        required=True, default=None, multiple=False, is_flag=False,
    )
    assert option.choices == ()
    assert option.raw_click_type == ""
    assert option.cli_flag == ""


# ---------------------------------------------------------------------------
# No Click / subprocess / UI types leak into the model
# ---------------------------------------------------------------------------

def test_model_module_does_not_import_click_or_subprocess():
    """The model must be usable without Click or any process machinery."""
    import types

    imported = {name for name, value in vars(model).items() if isinstance(value, types.ModuleType)}
    for banned in ("click", "subprocess", "asyncio", "pathlib", "os"):
        assert banned not in imported, f"model.py must not import {banned!r}"

    code_lines = [
        line for line in inspect.getsource(model).splitlines()
        if line.startswith(("import ", "from "))
    ]
    assert code_lines == [
        "from __future__ import annotations",
        "from dataclasses import dataclass",
        "from datetime import datetime",
        "from enum import Enum",
        "from typing import Any, Mapping",
    ]


def test_model_fields_are_primitives_only():
    """Only primitives, tuples, Mapping, datetime and ArgumentType appear in annotations."""
    allowed = {
        "str", "int", "float", "bool", "Any", "None", "datetime",
        "ArgumentType", "CommandCapabilities", "Mapping", "tuple",
        "ArgumentDescriptor", "OptionDescriptor",
    }
    for cls in (ArgumentDescriptor, OptionDescriptor, CommandDescriptor,
                ExecuteCommandRequest, ExecutionEvent, ResultRecord):
        for f in dataclasses.fields(cls):
            tokens = {t for t in str(f.type).replace("[", " ").replace("]", " ")
                      .replace(",", " ").replace("|", " ").replace("...", " ").split()}
            assert tokens <= allowed, f"{cls.__name__}.{f.name} exposes {tokens - allowed}"


# ---------------------------------------------------------------------------
# Construction smoke tests
# ---------------------------------------------------------------------------

def test_execution_event_construction():
    now = datetime.now()
    event = ExecutionEvent(
        execution_id="abc", seq=0, time=now, type="execution.started",
        step_id=None, payload={"system": "asset-pipeline"},
    )
    assert event.seq == 0
    assert event.step_id is None
    assert event.payload["system"] == "asset-pipeline"


def test_result_record_construction():
    record = ResultRecord(
        kind="DiagnosticFinding", severity="error", title="t", summary=None, payload={},
    )
    assert record.kind == "DiagnosticFinding"
    assert record.summary is None
