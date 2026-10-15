"""Command registry built by reflecting a Click command tree.

The registry is the *only* construction path for :class:`CommandDescriptor`
(SD-CONSOLE-001): descriptors are never hand-authored.  Reflection uses Click's
public ``MultiCommand`` API (``list_commands`` / ``get_command``) and recurses
structurally wherever a sub-command is itself a ``MultiCommand`` — no DiaCLI
internals, and no hardcoded list of group names, so new commands and new
nesting levels are picked up automatically.

Every :meth:`CommandRegistry.from_click_app` call re-walks the tree; there is no
cache of any kind (SD-CONSOLE-010).
"""
from __future__ import annotations

import time
from typing import Any, Iterable, Iterator

import click
from loguru import logger

from dia_console.model import (
    ArgumentDescriptor,
    ArgumentType,
    CommandCapabilities,
    CommandDescriptor,
    OptionDescriptor,
)

# Guard against a pathological/cyclic command tree.
_MAX_DEPTH = 12

# Click param types that map exactly onto an ArgumentType.  Matched on the
# concrete class, not isinstance, so subclasses such as click.IntRange
# deliberately fall through to the degrade-to-STRING path.
_EXACT_TYPE_MAP = {
    click.types.StringParamType: ArgumentType.STRING,
    click.types.IntParamType: ArgumentType.INT,
    click.types.FloatParamType: ArgumentType.FLOAT,
    click.types.BoolParamType: ArgumentType.BOOL,
}


def _normalise_default(value: Any) -> Any:
    """Reduce a Click default to a primitive the model is allowed to hold."""
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    if isinstance(value, (list, tuple, set, frozenset)):
        return tuple(_normalise_default(item) for item in value)
    if callable(value):
        # Callable defaults are resolved at parse time; not representable here.
        return None
    return str(value)


def _classify(param_type: click.ParamType) -> tuple[ArgumentType, tuple[str, ...]]:
    """Map a Click param type onto (ArgumentType, choices).

    Anything outside the closed :class:`ArgumentType` set degrades to
    ``STRING``; the caller preserves the real Click type name separately.  This
    never raises, whatever custom ``ParamType`` a command declares.
    """
    exact = _EXACT_TYPE_MAP.get(type(param_type))
    if exact is not None:
        return exact, ()
    if isinstance(param_type, click.Choice):
        try:
            choices = tuple(str(choice) for choice in param_type.choices)
        except Exception:  # pragma: no cover - defensive
            choices = ()
        return ArgumentType.CHOICE, choices
    if isinstance(param_type, click.Path):
        return ArgumentType.PATH, ()
    return ArgumentType.STRING, ()


def _raw_type_name(param: click.Parameter) -> str:
    try:
        return type(param.type).__name__
    except Exception:  # pragma: no cover - defensive
        return "unknown"


def _help_text(param: click.Parameter) -> str:
    return (getattr(param, "help", None) or "").strip()


def _long_flag(option: click.Option) -> str:
    """The flag a user would type, e.g. ``--module`` for param ``module_filter``.

    DiaCLI renames many params (``@click.option("--module", "module_filter")``),
    so a name-derived guess is wrong for ~half the tree; the declared opts are
    the only reliable source.
    """
    long_opts = [opt for opt in option.opts if opt.startswith("--")]
    if long_opts:
        return max(long_opts, key=len)
    if option.opts:
        return option.opts[0]
    return "--" + option.name.replace("_", "-")


def _describe_argument(param: click.Argument) -> ArgumentDescriptor:
    arg_type, _ = _classify(param.type)
    return ArgumentDescriptor(
        name=param.name,
        help=_help_text(param),
        type=arg_type,
        required=bool(param.required),
        default=_normalise_default(param.default),
        multiple=bool(getattr(param, "multiple", False)) or param.nargs == -1,
        raw_click_type=_raw_type_name(param),
    )


def _describe_option(param: click.Option) -> OptionDescriptor:
    opt_type, choices = _classify(param.type)
    return OptionDescriptor(
        name=param.name,
        help=_help_text(param),
        type=opt_type,
        required=bool(param.required),
        default=_normalise_default(param.default),
        multiple=bool(param.multiple),
        is_flag=bool(param.is_flag),
        choices=choices,
        raw_click_type=_raw_type_name(param),
        cli_flag=_long_flag(param),
    )


def _describe_command(command: click.Command, path: tuple[str, ...]) -> CommandDescriptor:
    arguments: list[ArgumentDescriptor] = []
    options: list[OptionDescriptor] = []
    for param in command.params:
        if isinstance(param, click.Argument):
            arguments.append(_describe_argument(param))
        elif isinstance(param, click.Option):
            options.append(_describe_option(param))
    description = (command.help or command.short_help or "").strip()
    return CommandDescriptor(
        id=".".join(path),
        path=path,
        name=path[-1],
        description=description,
        category=".".join(path[:-1]),
        arguments=tuple(arguments),
        options=tuple(options),
        capabilities=CommandCapabilities(),
        execution_binding="",
    )


class CommandRegistry:
    """An immutable snapshot of the runnable commands in a Click tree.

    Only leaf commands get descriptors — groups are navigation structure and are
    recoverable from :attr:`CommandDescriptor.path` / ``category``.
    """

    def __init__(
        self,
        commands: Iterable[CommandDescriptor] = (),
        reflection_errors: Iterable[tuple[str, str]] = (),
    ) -> None:
        self._commands: tuple[CommandDescriptor, ...] = tuple(commands)
        self._by_id: dict[str, CommandDescriptor] = {c.id: c for c in self._commands}
        self._reflection_errors: tuple[tuple[str, str], ...] = tuple(reflection_errors)

    # -- construction ----------------------------------------------------- #

    @classmethod
    def from_click_app(cls, app: click.Command) -> "CommandRegistry":
        """Reflect ``app``'s command tree into a fresh registry.

        ``app`` is normally DiaCLI's top-level ``dia`` app.  A command that cannot be
        loaded (import error in its module, for example) is recorded in
        :attr:`reflection_errors` rather than raising, so one broken command
        cannot take down the whole console.
        """
        started = time.monotonic()
        commands: list[CommandDescriptor] = []
        errors: list[tuple[str, str]] = []
        cls._reflect(app, (), commands, errors, set())
        commands.sort(key=lambda descriptor: descriptor.id)
        elapsed_ms = (time.monotonic() - started) * 1000
        logger.debug(
            "CommandRegistry reflection: {} commands, {} errors, {:.1f}ms",
            len(commands), len(errors), elapsed_ms,
        )
        return cls(commands, errors)

    @classmethod
    def _reflect(
        cls,
        group: click.Command,
        path: tuple[str, ...],
        commands: list[CommandDescriptor],
        errors: list[tuple[str, str]],
        seen: set[int],
    ) -> None:
        if len(path) >= _MAX_DEPTH or id(group) in seen:
            return
        seen.add(id(group))
        try:
            names = group.list_commands(click.Context(group))
        except Exception as exc:  # pragma: no cover - defensive
            message = f"list_commands failed: {exc!r}"
            errors.append((".".join(path), message))
            logger.warning("Command reflection failed for {!r}: {}", ".".join(path), message)
            return
        for name in names:
            child_path = path + (name,)
            child_id = ".".join(child_path)
            try:
                child = group.get_command(click.Context(group), name)
            except Exception as exc:
                message = f"get_command failed: {exc!r}"
                errors.append((child_id, message))
                logger.warning("Command reflection failed for {!r}: {}", child_id, message)
                continue
            if child is None:
                errors.append((child_id, "get_command returned None"))
                logger.warning("Command reflection failed for {!r}: get_command returned None", child_id)
                continue
            if isinstance(child, click.MultiCommand):
                cls._reflect(child, child_path, commands, errors, seen)
            else:
                try:
                    commands.append(_describe_command(child, child_path))
                except Exception as exc:  # pragma: no cover - defensive
                    message = f"reflection failed: {exc!r}"
                    errors.append((child_id, message))
                    logger.warning("Command reflection failed for {!r}: {}", child_id, message)

    # -- lookup ----------------------------------------------------------- #

    @property
    def commands(self) -> tuple[CommandDescriptor, ...]:
        return self._commands

    @property
    def reflection_errors(self) -> tuple[tuple[str, str], ...]:
        """``(command_id, message)`` for every command that failed to reflect."""
        return self._reflection_errors

    def ids(self) -> tuple[str, ...]:
        return tuple(descriptor.id for descriptor in self._commands)

    def get(self, command_id: str) -> CommandDescriptor | None:
        return self._by_id.get(command_id)

    def require(self, command_id: str) -> CommandDescriptor:
        try:
            return self._by_id[command_id]
        except KeyError:
            raise KeyError(f"Unknown command id: {command_id!r}") from None

    def __len__(self) -> int:
        return len(self._commands)

    def __iter__(self) -> Iterator[CommandDescriptor]:
        return iter(self._commands)

    def __contains__(self, command_id: object) -> bool:
        return command_id in self._by_id
