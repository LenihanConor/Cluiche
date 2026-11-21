"""Shared helpers for the test_validate_schema_* files (not a test module)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import List, Optional

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli


def entry(key, **kw):
    e = {"key": key, "kind": "parameter", "label": key, "description": f"the {key}", "type": "float"}
    e.update(kw)
    return e


def schema(fmt, *entries):
    return {"format": "dia.value_schema", "version": 1, "for": fmt, "entries": list(entries)}


def put(root: Path, rel: str, doc) -> Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    if isinstance(doc, bytes):
        p.write_bytes(doc)
    else:
        p.write_text(doc if isinstance(doc, str) else json.dumps(doc), encoding="utf-8", newline="")
    return p


def invoke(args: List[str], *, json_out: bool = False):
    """Real (unmocked) run of `dia validate schema`; stdout and stderr kept separate."""
    return CliRunner(mix_stderr=False).invoke(validate_cli, ["schema", *args, *(["--json"] if json_out else [])])


def findings(args: List[str]) -> list:
    r = invoke(args, json_out=True)
    assert r.exit_code in (0, 1), (r.exit_code, r.stdout, r.stderr)
    return json.loads(r.stdout)


def rules_by_file(data: list) -> dict:
    out: dict = {}
    for f in data:
        out.setdefault(f["file"], []).append(f["rule"])
    return out


def basic_tree(root: Path, rate=0.5, default=0.5, folder="Eco"):
    put(root, f"{folder}/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=default)))
    put(root, f"{folder}/eco.json", {"format": "eco.params", "version": 1, "rate": rate})


def repo(tmp_path: Path, apps=("App",), pipeline: Optional[str] = "") -> Path:
    """A fake repo root with Cluiche/Assets/<app> directories."""
    (tmp_path / "pipeline.toml").write_text(pipeline or "", encoding="utf-8")
    for a in apps:
        (tmp_path / "Cluiche" / "Assets" / a).mkdir(parents=True, exist_ok=True)
    return tmp_path
