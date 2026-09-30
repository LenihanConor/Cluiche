"""Typed, UI-independent data model for DiaConsole.

Every type in this module is a plain frozen dataclass or ``str`` enum built out
of primitives (``str`` / ``int`` / ``float`` / ``bool`` / ``tuple`` /
``Mapping``).  No Click objects, subprocess handles, file objects or UI types
are ever stored here — a front end (or a test) can depend on this module alone
without importing Click or spawning anything (SD-CONSOLE-005).
"""
from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from enum import Enum
from typing import Any, Mapping


class ArgumentType(str, Enum):
    """Closed set of argument/option value types the console understands.

    Any Click parameter type outside this set degrades to :attr:`STRING`, with
    the real Click type name preserved in ``raw_click_type`` for debugging.
    """

    STRING = "string"
    INT = "int"
    FLOAT = "float"
    BOOL = "bool"
    PATH = "path"
    CHOICE = "choice"


@dataclass(frozen=True)
class ArgumentDescriptor:
    """A positional argument of a command (Click ``Argument``)."""

    name: str
    help: str
    type: ArgumentType
    required: bool
    default: Any | None
    multiple: bool
    raw_click_type: str


@dataclass(frozen=True)
class OptionDescriptor:
    """A named option of a command (Click ``Option``).

    ``name`` is the *Python* parameter name (``module_filter``), which is the
    key callers use in :class:`ExecuteCommandRequest.options`.  ``cli_flag`` is
    the actual long flag as typed on the command line (``--module``).  The two
    frequently differ in DiaCLI, so ``cli_flag`` — not a name-derived guess —
    is what argv serialization uses.
    """

    name: str
    help: str
    type: ArgumentType
    required: bool
    default: Any | None
    multiple: bool
    is_flag: bool
    choices: tuple[str, ...] = ()
    raw_click_type: str = ""
    cli_flag: str = ""


@dataclass(frozen=True)
class CommandCapabilities:
    """Reserved capability flags for a command.

    Intentionally empty: neither the DiaConsole system spec nor the
    console-command-model feature spec defines any capability field yet.  It
    exists so ``CommandDescriptor``'s shape is stable when capabilities are
    introduced by a later feature.
    """


@dataclass(frozen=True)
class CommandDescriptor:
    """A single runnable DiaCLI command, reflected from Click.

    ``id`` is the dotted path (``check.arch``); ``path`` is the same chain as a
    tuple of argv words (``("check", "arch")``); ``category`` is the dotted
    path of the owning group (``"check"``, empty for a top-level command).
    """

    id: str
    path: tuple[str, ...]
    name: str
    description: str
    category: str
    arguments: tuple[ArgumentDescriptor, ...] = ()
    options: tuple[OptionDescriptor, ...] = ()
    capabilities: CommandCapabilities = CommandCapabilities()
    execution_binding: str = ""


@dataclass(frozen=True)
class ExecuteCommandRequest:
    """A request to run one command with concrete argument/option values.

    Keys in ``arguments``/``options`` are descriptor ``name``s.  Only keys
    present here are serialized into argv.
    """

    command_id: str
    project_id: str | None
    arguments: Mapping[str, Any]
    options: Mapping[str, Any]


@dataclass(frozen=True)
class ExecutionEvent:
    """One event in a running execution's lifecycle.

    ``type`` is one of: ``execution.started``, ``execution.completed``,
    ``execution.failed``, ``execution.cancelled``, ``step.started``,
    ``step.progress``, ``step.completed``, ``step.failed``, ``step.skipped``,
    ``log``, ``result.produced``.  It is translated from DiaCLI's
    ``dia.output.v1`` NDJSON ``event`` field (SD-CONSOLE-002).
    """

    execution_id: str
    seq: int
    time: datetime
    type: str
    step_id: str | None
    payload: Mapping[str, Any]


@dataclass(frozen=True)
class ResultRecord:
    """A structured result extracted from an execution.

    Shape only in this feature — no adapters construct these yet; the
    ``console-typed-results`` feature builds them.
    """

    kind: str
    severity: str | None
    title: str
    summary: str | None
    payload: Mapping[str, Any]
