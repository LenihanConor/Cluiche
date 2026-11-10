"""dia docs precommit runs dia validate schema on changed files (DiaValueSchema Feature 2 task 8, AC14)."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.commands.docs import precommit_cmd
from dia_cli.commands.validate import schema_discovery


def entry(key, **kw):
    e = {"key": key, "kind": "parameter", "label": key, "description": f"the {key}", "type": "float"}
    e.update(kw)
    return e


def put(root: Path, rel: str, doc) -> Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(doc if isinstance(doc, str) else json.dumps(doc), encoding="utf-8")
    return p


def tree(root: Path, rate=0.5, bad_schema=False):
    a = "Cluiche/Assets/App/"
    sch = {"format": "dia.value_schema", "version": 1, "for": "eco.params",
           "entries": [entry("rate", min=0, max=1, default=0.5)]}
    if bad_schema:
        sch["entries"][0]["type"] = "nonsense"
    return (put(root, a + "eco.schema.json", sch),
            put(root, a + "eco.json", {"format": "eco.params", "version": 1, "rate": rate}))


def test_good_schema_no_errors(tmp_path):
    s, d = tree(tmp_path)
    errors, warnings = precommit_cmd._check_schema(tmp_path, [s])
    assert errors == [] and warnings == []


def test_bad_schema_fails(tmp_path):
    s, d = tree(tmp_path, bad_schema=True)
    errors, _ = precommit_cmd._check_schema(tmp_path, [s])
    assert errors and errors[0].startswith("FAIL App/eco.schema.json")


def test_out_of_range_data_fails(tmp_path):
    s, d = tree(tmp_path, rate=7)
    errors, _ = precommit_cmd._check_schema(tmp_path, [d])
    assert any("App/eco.json" in e and "outside [0, 1]" in e for e in errors)


def test_only_changed_files_reported(tmp_path):
    s, d = tree(tmp_path, rate=7)
    other = put(tmp_path, "Cluiche/Assets/App/ok.json", {"format": "eco.params", "version": 1, "rate": 0.5})
    errors, warnings = precommit_cmd._check_schema(tmp_path, [other])
    assert errors == [] and warnings == []


def test_no_schema_is_warn_only(tmp_path):
    tree(tmp_path)
    m = put(tmp_path, "Cluiche/Assets/App/m.json", {"format": "mystery.thing", "version": 1})
    errors, warnings = precommit_cmd._check_schema(tmp_path, [m])
    assert errors == [] and len(warnings) == 1 and warnings[0].startswith("WARN App/m.json")


def test_no_relevant_files_skips_discovery(tmp_path, monkeypatch):
    tree(tmp_path)
    called = []
    monkeypatch.setattr(schema_discovery, "discover_and_validate", lambda *a, **k: called.append(1) or ([], []))
    cpp = put(tmp_path, "Dia/x.cpp", "int x;")
    plain = put(tmp_path, "Cluiche/Assets/Other/noformat.json", {"a": 1})  # folder without a schema
    outside = put(tmp_path, "docs/x.json", {"format": "eco.params"})
    assert precommit_cmd._check_schema(tmp_path, [cpp, plain, outside]) == ([], [])
    assert called == []


def _run(tmp_path, monkeypatch, files):
    monkeypatch.setattr(precommit_cmd, "find_repo_root", lambda _f: tmp_path)
    monkeypatch.setattr(precommit_cmd, "_get_modified_files", lambda _r: files)
    return CliRunner().invoke(precommit_cmd.precommit, [])


def test_precommit_blocks_on_fail(tmp_path, monkeypatch):
    s, d = tree(tmp_path, rate=7)
    r = _run(tmp_path, monkeypatch, [d])
    assert r.exit_code == 1, r.output
    assert "schema" in r.output and "FAIL App/eco.json" in r.output


def test_precommit_warn_does_not_block(tmp_path, monkeypatch):
    tree(tmp_path)
    m = put(tmp_path, "Cluiche/Assets/App/m.json", {"format": "mystery.thing", "version": 1})
    r = _run(tmp_path, monkeypatch, [m])
    assert r.exit_code == 0, r.output
    assert "WARN" in r.output and "WARN App/m.json" in r.output


def test_precommit_section_absent_without_relevant_files(tmp_path, monkeypatch):
    c = put(tmp_path, "Dia/x.cpp", "int x;")
    r = _run(tmp_path, monkeypatch, [c])
    assert r.exit_code == 0, r.output
    assert "Schema" not in r.output and "schema" not in r.output.lower().replace("precommit", "")
