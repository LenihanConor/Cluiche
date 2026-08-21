"""dia codegen messages — generate C++ message structs, a Handlers binding
struct, and a RegisterMessages wiring function from a .diagamemessages file.
"""
from __future__ import annotations

import json
from pathlib import Path

import click


def _cpp_overflow(overflow: str) -> str:
    """Map the JSON 'overflow' string to its Dia::Mailbox::OverflowPolicy enumerator."""
    if overflow == "assert":
        return "Dia::Mailbox::OverflowPolicy::Assert"
    return "Dia::Mailbox::OverflowPolicy::DropOldest"


def _cpp_pass(pass_value: str) -> str:
    """Map the JSON 'pass' string to its Dia::MessageBus::Pass enumerator."""
    if pass_value == "reaction":
        return "Dia::MessageBus::Pass::Reaction"
    return "Dia::MessageBus::Pass::Primary"


def render_header(data: dict, source_filename: str) -> str:
    """Render the full generated C++ header text for a validated .diagamemessages payload."""
    namespace = data["namespace"]
    includes = data.get("includes", [])
    messages = data["messages"]

    lines: list[str] = []
    lines.append(f"// GENERATED — do not edit. Source: {source_filename}")
    lines.append("#pragma once")
    lines.append("#include <DiaCore/CRC/StringCRC.h>")
    lines.append("#include <DiaMailbox/Mailbox.h>        // OverflowPolicy")
    lines.append("#include <DiaMessageBus/Bus.h>         // Bus")
    for inc in includes:
        lines.append(f"#include <{inc}>")
    lines.append("#include <functional>")
    lines.append("")
    lines.append(f"namespace {namespace} {{")
    lines.append("")

    # (1) Message structs
    lines.append("    // ── (1) Message structs ─────────────────────────────────────────────")
    for msg in messages:
        msg_id = msg["id"]
        lines.append(f"    struct {msg_id} {{")
        # StringCRC's const-char* constructor is not constexpr (it CRC-hashes
        # at runtime), so kTypeId must be "inline const" rather than
        # "constexpr" or every generated header fails to compile.
        lines.append(f'        static inline const Dia::Core::StringCRC kTypeId{{ "{msg_id}" }};')
        for field in msg.get("fields", []):
            field_line = f"        {field['type']} {field['name']};"
            notes = field.get("notes")
            if notes:
                field_line += f"  // {notes}"
            lines.append(field_line)
        lines.append("    };")
        lines.append("")

    # (2) Handler binding struct
    lines.append("    // ── (2) Handler binding struct ──────────────────────────────────────")
    lines.append("    // One slot per message that has consumers. Bind before RegisterMessages.")
    lines.append("    struct Handlers {")
    for msg in messages:
        consumers = msg.get("consumers", [])
        if not consumers:
            continue
        msg_id = msg["id"]
        label = "consumer" if len(consumers) == 1 else "consumers"
        lines.append(
            f"        std::function<void(const {msg_id}&)> on{msg_id};  // {label}: {', '.join(consumers)}"
        )
    lines.append("    };")
    lines.append("")

    # (3) Registration wiring
    lines.append("    // ── (3) Registration wiring ─────────────────────────────────────────")
    lines.append("    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)")
    lines.append("    {")
    for msg in messages:
        msg_id = msg["id"]
        router = msg["router"]
        pass_value = msg["pass"]

        capacity = msg.get("capacity", 64)
        cap_suffix = " (default)" if "capacity" not in msg else ""

        overflow = msg.get("overflow", "assert")
        overflow_suffix = " (default)" if "overflow" not in msg else ""

        lines.append(
            f"        // {msg_id} — {router}, {pass_value}, capacity {capacity}{cap_suffix}, {overflow}{overflow_suffix}"
        )
        lines.append(f"        bus.RegisterType<{msg_id}, {capacity}>({_cpp_overflow(overflow)});")

        for producer in msg.get("producers", []):
            lines.append(f'        bus.RegisterProducer<{msg_id}>(Dia::Core::StringCRC{{ "{producer}" }});')

        consumers = msg.get("consumers", [])
        if consumers:
            handler_name = f"on{msg_id}"
            lines.append(f'        DIA_ASSERT(handlers.{handler_name}, "Handlers::{handler_name} is unbound");')
            for consumer in consumers:
                lines.append(
                    f'        bus.Subscribe<{msg_id}>(Dia::Core::StringCRC{{ "{consumer}" }}, '
                    f"handlers.{handler_name}, {_cpp_pass(pass_value)});"
                )

        lines.append("")

    lines.append("    }")
    lines.append("")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


@click.command("messages")
@click.argument("source", type=click.Path(exists=True, dir_okay=False))
@click.option(
    "--output", "-o", "output", default=None, type=click.Path(),
    help="Output header path. Defaults to <source-stem>.h alongside the source file.",
)
@click.pass_context
def messages(ctx: click.Context, source: str, output: str | None) -> None:
    """Generate a C++ header (message structs + Handlers + RegisterMessages) from a .diagamemessages file.

    Example:
        dia codegen messages Cluiche/CluicheTest/Stages/MessageBusTestStage/messagebus_test_messages.diagamemessages
    """
    # Reuse the already-implemented schema validator rather than duplicating its rules.
    from dia_cli.cli.cli_validate import _validate_diagamemessages

    source_path = Path(source)

    try:
        raw = json.loads(source_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        click.echo(f"[codegen messages] ERROR: invalid JSON in {source_path}: {e}", err=True)
        ctx.exit(1)
        return

    if not isinstance(raw, dict):
        click.echo(f"[codegen messages] ERROR: {source_path} must contain a JSON object at the root.", err=True)
        ctx.exit(1)
        return

    errors = _validate_diagamemessages(raw)
    if errors:
        click.echo(f"[codegen messages] ERROR: {source_path} failed schema validation:", err=True)
        for err in errors:
            click.echo(f"  - {err}", err=True)
        ctx.exit(1)
        return

    output_path = Path(output) if output else source_path.with_suffix(".h")
    header_text = render_header(raw, source_path.name)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(header_text, encoding="utf-8", newline="\n")

    click.echo(f"[codegen messages] Wrote {len(raw['messages'])} message type(s) to: {output_path}")
    ctx.exit(0)
