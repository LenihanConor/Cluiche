"""Unit tests for dia codegen messages command."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.commands.codegen.group import codegen_group


def _write_messages(tmp_path: Path, name: str, data: dict) -> Path:
    p = tmp_path / name
    p.write_text(json.dumps(data), encoding="utf-8")
    return p


_VALID_DATA = {
    "schema": "diagamemessages/v1",
    "namespace": "CluicheTest::Messages",
    "includes": ["DiaEntity/EntityId.h"],
    "messages": [
        {
            "id": "NetworkPulseEvent",
            "router": "broadcast",
            "pass": "primary",
            "capacity": 128,
            "overflow": "drop_oldest",
            "producers": ["EmitterSystem"],
            "consumers": ["ReceiverSystem"],
            "fields": [
                {"name": "sequenceId", "type": "uint32_t", "notes": "monotonically increasing per emitter"},
                {"name": "emitterId", "type": "EntityId", "notes": "which emitter fired"},
            ],
        },
        {
            "id": "PongEvent",
            "router": "broadcast",
            "pass": "reaction",
            "producers": ["ReceiverSystem"],
            "consumers": ["EmitterSystem"],
            "fields": [
                {"name": "responderId", "type": "EntityId", "notes": "receiver that replied"},
            ],
        },
        {
            "id": "BurstEvent",
            "router": "broadcast",
            "pass": "primary",
            "producers": ["EmitterSystem"],
            "consumers": [],
            "fields": [
                {"name": "count", "type": "uint32_t"},
            ],
        },
    ],
}


# ---------------------------------------------------------------------------
# CLI discovery
# ---------------------------------------------------------------------------

def test_codegen_command_discovered():
    runner = CliRunner()
    result = runner.invoke(codegen_group, ["--help"])
    assert result.exit_code == 0
    assert "messages" in result.output


# ---------------------------------------------------------------------------
# Happy path
# ---------------------------------------------------------------------------

def test_codegen_messages_generates_header(tmp_path):
    source = _write_messages(tmp_path, "test_messages.diagamemessages", _VALID_DATA)
    runner = CliRunner()
    result = runner.invoke(codegen_group, ["messages", str(source)])

    assert result.exit_code == 0, result.output
    assert "Wrote 3 message type(s)" in result.output

    out_path = tmp_path / "test_messages.h"
    assert out_path.exists()
    text = out_path.read_text(encoding="utf-8")

    # Banner + always-on includes.
    assert "// GENERATED — do not edit. Source: test_messages.diagamemessages" in text
    assert "#include <DiaCore/CRC/StringCRC.h>" in text
    assert "#include <DiaMailbox/Mailbox.h>" in text
    assert "#include <DiaMessageBus/Bus.h>" in text
    assert "#include <DiaEntity/EntityId.h>" in text
    assert "#include <functional>" in text

    # Structs.
    assert "struct NetworkPulseEvent {" in text
    assert 'static inline const Dia::Core::StringCRC kTypeId{ "NetworkPulseEvent" };' in text
    assert "struct PongEvent {" in text
    assert "struct BurstEvent {" in text

    # Handlers: only messages with non-empty consumers get a slot.
    assert "onNetworkPulseEvent" in text
    assert "onPongEvent" in text
    assert "onBurstEvent" not in text

    # RegisterType / RegisterProducer / Subscribe, with capacity + overflow mapping.
    assert "bus.RegisterType<NetworkPulseEvent, 128>(Dia::Mailbox::OverflowPolicy::DropOldest);" in text
    assert "bus.RegisterType<PongEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);" in text
    assert "bus.RegisterProducer<NetworkPulseEvent>(Dia::Core::StringCRC{ \"EmitterSystem\" });" in text

    # Pass must be threaded explicitly through Subscribe — this is the correctness-critical bit.
    assert (
        "bus.Subscribe<NetworkPulseEvent>(Dia::Core::StringCRC{ \"ReceiverSystem\" }, "
        "handlers.onNetworkPulseEvent, Dia::MessageBus::Pass::Primary);"
    ) in text
    assert (
        "bus.Subscribe<PongEvent>(Dia::Core::StringCRC{ \"EmitterSystem\" }, "
        "handlers.onPongEvent, Dia::MessageBus::Pass::Reaction);"
    ) in text

    # BurstEvent has no consumers -> no Subscribe call for it at all.
    assert "bus.Subscribe<BurstEvent>" not in text


def test_codegen_messages_output_option(tmp_path):
    source = _write_messages(tmp_path, "test_messages.diagamemessages", _VALID_DATA)
    custom_out = tmp_path / "nested" / "custom_name.h"
    runner = CliRunner()
    result = runner.invoke(codegen_group, ["messages", str(source), "--output", str(custom_out)])

    assert result.exit_code == 0, result.output
    assert custom_out.exists()


# ---------------------------------------------------------------------------
# Invalid input
# ---------------------------------------------------------------------------

def test_codegen_messages_missing_schema_fails_cleanly(tmp_path):
    bad_data = {"namespace": "Bad::NoSchema", "messages": []}
    source = _write_messages(tmp_path, "bad.diagamemessages", bad_data)
    runner = CliRunner()
    result = runner.invoke(codegen_group, ["messages", str(source)])

    assert result.exit_code == 1
    assert "missing required key: schema" in result.output
    assert not (tmp_path / "bad.h").exists()


def test_codegen_messages_invalid_json_fails_cleanly(tmp_path):
    source = tmp_path / "broken.diagamemessages"
    source.write_text("{not valid json", encoding="utf-8")
    runner = CliRunner()
    result = runner.invoke(codegen_group, ["messages", str(source)])

    assert result.exit_code == 1
    assert "invalid JSON" in result.output
