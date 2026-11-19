"""dia docs precommit runs the stage-proves check (proves-traceability AC 9)."""
import json

from click.testing import CliRunner

from dia_cli.commands.docs import precommit_cmd

SPECS = "docs/specs/applications/cow/systems"


def _w(root, rel, text):
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    return p


def _spec(root, name):
    _w(root, f"{SPECS}/{name}/{name}.md",
       f"# {name}\n\n**Testbed:** required\n\n## Summary\n\nx\n\n## Status\n\n`Approved` - n\n")


def _game(root, stages):
    base = "Cluiche/Assets/A"
    _w(root, f"{base}/A.diagame", json.dumps({"imports": [
        {"type": "manifest", "path": "m.diaapp"},
        *[{"type": "stage", "path": f"{n}.diastage"} for n in stages]]}))
    _w(root, f"{base}/m.diaapp", json.dumps({"stages": [{"name": "Boot", "transitions": list(stages)}]}))
    for n, proves in stages.items():
        _w(root, f"{base}/{n}.diastage", json.dumps({"name": n, "proves": proves}))


def _run(tmp_path, monkeypatch):
    dummy = tmp_path / "dummy.txt"
    dummy.write_text("x", encoding="utf-8")
    monkeypatch.setattr(precommit_cmd, "find_repo_root", lambda _f: tmp_path)
    monkeypatch.setattr(precommit_cmd, "_get_modified_files", lambda _r: [dummy])
    return CliRunner().invoke(precommit_cmd.precommit, [])


def test_broken_link_fails(tmp_path, monkeypatch):
    _game(tmp_path, {"S1": ["docs/specs/nope.md"]})
    r = _run(tmp_path, monkeypatch)
    assert r.exit_code == 1
    assert "FAIL  stage-proves" in r.output
    assert "S1" in r.output and "docs/specs/nope.md" in r.output


def test_unproven_only_passes(tmp_path, monkeypatch):
    _spec(tmp_path, "a")
    _game(tmp_path, {"S1": []})
    r = _run(tmp_path, monkeypatch)
    assert r.exit_code == 0, r.output
    assert "1 unproven" in r.output


def test_clean_passes(tmp_path, monkeypatch):
    _spec(tmp_path, "a")
    _game(tmp_path, {"S1": [f"{SPECS}/a/a.md"]})
    r = _run(tmp_path, monkeypatch)
    assert r.exit_code == 0, r.output
    assert "OK    stage-proves" in r.output
