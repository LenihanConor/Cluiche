"""Sim-hosted command help over the wire: the list_commands query returns command_details with real help for the
sim-hosted CellLayers names, each sim name exactly once next to the DiaAPI commands (Sim-Hosted Command Help AC 11, AC 9).
There is no per-command help query; list_commands.command_details is the wire surface.
"""
from client import AutomationError  # noqa: F401

_STAGE = "CellLayersViewerTestStage"
_CHECKPOINT = "test.cell_layers_viewer.passed"


def _by_name(details):
    out = {}
    for d in details:
        out.setdefault(d["name"], []).append(d)
    return out


def test_celllayers_command_help(dia_client):
    c = dia_client
    c.navigate_to(_STAGE)
    try:
        result = c.poll_checkpoint(_CHECKPOINT, timeout_s=30.0, expected_stage=_STAGE)
        assert result.get("passed"), f"checkpoint failed: {result}"

        listing = c.send_command("list_commands")
        details = listing["command_details"]
        names = listing["api_commands"]
        by_name = _by_name(details)

        # Every name appears exactly once, in both the merged name list and the details.
        assert len(names) == len(set(names)), "duplicate names in merged list"
        assert all(len(v) == 1 for v in by_name.values()), [k for k, v in by_name.items() if len(v) > 1]
        assert set(names) == set(by_name), set(names) ^ set(by_name)

        # DiaAPI commands are present alongside the sim-hosted ones.
        sources = {d["source"] for d in details}
        assert sources == {"api", "sim"}, sources

        # Viewer names are sim-hosted, never DiaAPI-sourced (AC 9 over the wire).
        sim_names = {d["name"] for d in details if d["source"] == "sim"}
        for n in ("cells.layer.list", "cells.select.cell", "cells.select.clear"):
            assert n in sim_names, n
        assert not [d for d in details if d["source"] == "api" and d["name"].startswith("cells.")]

        # Sim-hosted query with real help.
        q = by_name["cells.layer.list"][0]
        assert q["kind"] == "query"
        assert q["description"].strip()
        assert q["category"] == "cells" and q["owner"] == "DiaCellLayers"
        assert q["noArgs"] is True

        # Viewer command with params.
        cmd = by_name["cells.select.cell"][0]
        assert cmd["kind"] == "command"
        assert cmd["description"].strip()
        assert cmd["category"] == "cells" and cmd["owner"] == "DiaCellLayers"
        assert cmd["noArgs"] is False
        assert len(cmd["params"]) >= 1, cmd
        for p in cmd["params"]:
            assert p["name"] and p["type"], p
            assert isinstance(p["required"], bool), p
            assert p["description"].strip(), p
        assert any(p["required"] for p in cmd["params"]), cmd["params"]

        # noArgs command.
        clr = by_name["cells.select.clear"][0]
        assert clr["description"].strip()
        assert clr["noArgs"] is True
        assert clr["category"] == "cells" and clr["owner"] == "DiaCellLayers"
        assert not clr.get("params"), clr

        # sim.echo is registered by SimEchoTestModule when that module is loaded in the stage.
        if "sim.echo" in by_name:
            e = by_name["sim.echo"][0]
            assert e["source"] == "sim" and e["description"].strip()

        # Every sim-hosted entry carries non-empty help.
        for d in details:
            if d["source"] == "sim":
                assert d["description"].strip(), d["name"]
    finally:
        try:
            c.abort_stage()
            c.navigate_to("Boot")
        except Exception:  # the automation host may already have closed the connection after the stage resolved
            pass
