"""dia schema export (DiaValueSchema Feature 6 task 5)."""
import json
import subprocess
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.schema import cli as schema_cli
from dia_cli.commands.schema import export_cmd
from dia_cli.commands.validate import schema_codeside, schema_discovery


def put(root: Path, rel: str, doc) -> Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(doc if isinstance(doc, str) else json.dumps(doc), encoding="utf-8")
    return p


def sidecar(fmt, *entries):
    return {"format": "dia.value_schema", "version": 1, "for": fmt, "entries": list(entries)}


def make_repo(tmp_path: Path) -> Path:
    (tmp_path / "pipeline.toml").write_text('[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\n')
    assets = tmp_path / "Cluiche" / "Assets" / "CoW"
    put(assets, "Global/Ecology/eco.schema.json",
        sidecar("cow.ecology_params",
                {"key": "grazing.rate", "kind": "parameter", "label": "Rate", "description": "d",
                 "type": "float", "min": 0, "max": 1, "default": 0.5, "unit": "x"}))
    put(assets, "Global/cow.ramps.json",
        {"format": "dia.ramps", "version": 1,
         "ramps": [{"id": "cow.heat", "kind": "continuous", "stops": [{"t": 0, "colour": "#000000"}]}]})
    put(tmp_path, "Dia/DiaValueSchema/Data/ramps.json",
        {"format": "dia.ramps", "version": 1,
         "ramps": [{"id": "sequential.green", "kind": "continuous", "stops": [{"t": 0, "colour": "#ffffff"}]}]})
    return tmp_path


def patch_repo(monkeypatch, root):
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: root)
    monkeypatch.setattr(export_cmd, "get_repo_root", lambda: root)


def patch_git(monkeypatch, commit="abcdef1234567890", dirty=False):
    monkeypatch.setattr(export_cmd, "git_head", lambda: commit)
    monkeypatch.setattr(export_cmd, "git_dirty", lambda: dirty)


VALID = {"format": "dia.value_schema_export", "version": 1,
         "header": {"app": "cow", "config": "Debug", "generated_at": "2026-10-20T14:03:00Z", "code_side": True},
         "entries": [{"key": "cow.m", "kind": "metric", "origin": "code:metric"}], "ramps": [],
         "undescribed": {"metrics": [{"name": "cow.x", "owner": "cow"}], "commands": [], "component_fields": []}}


def stub_launcher(monkeypatch, calls, content=None, write=True):
    def fake(target, config, extra_args=None, **kw):
        calls.append((target, config, list(extra_args or [])))
        args = list(extra_args or [])
        path = Path(args[args.index("--describe-schema") + 1])
        if write:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content if isinstance(content, str) else json.dumps(content or VALID),
                            encoding="utf-8")
        return 0
    monkeypatch.setattr(export_cmd, "launch_target", fake)

    def boom(*a, **k):
        raise AssertionError("raw subprocess invoked")
    monkeypatch.setattr(subprocess, "run", boom)
    monkeypatch.setattr(subprocess, "Popen", boom)


def run(args):
    return CliRunner().invoke(schema_cli, ["export", *args])


def test_launch_mode_fills_git_and_writes_default_path(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch, dirty=True)
    calls = []
    stub_launcher(monkeypatch, calls)
    r = run(["--target", "cow"])
    assert r.exit_code == 0, r.output
    out = root / "Cluiche" / "out" / "CoW" / "valueschema.json"
    assert calls == [("cow", "Debug", ["--describe-schema", str(out)])]
    doc = json.loads(out.read_text(encoding="utf-8"))
    assert doc["header"]["git_commit"] == "abcdef12"
    assert doc["header"]["dirty"] is True
    assert doc["header"]["app"] == "cow" and doc["header"]["generated_at"]
    assert str(out) in r.output
    assert "1 entries" in r.output and "1 undescribed" in r.output


def test_clean_tree_dirty_false(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch, dirty=False)
    stub_launcher(monkeypatch, [])
    assert run(["--target", "cow"]).exit_code == 0
    doc = json.loads((root / "Cluiche/out/CoW/valueschema.json").read_text(encoding="utf-8"))
    assert doc["header"]["dirty"] is False


def test_stage_passes_flag_and_names_output(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    calls = []
    stub_launcher(monkeypatch, calls)
    r = run(["--target", "cow", "--stage", "arena", "--config", "Release"])
    assert r.exit_code == 0, r.output
    out = root / "Cluiche" / "out" / "CoW" / "valueschema.arena.json"
    assert calls == [("cow", "Release", ["--describe-schema", str(out), "--stage", "arena"])]
    assert out.is_file()
    assert not (root / "Cluiche/out/CoW/valueschema.json").exists()


def test_out_is_honoured(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    calls = []
    stub_launcher(monkeypatch, calls)
    dest = tmp_path / "elsewhere" / "x.json"
    r = run(["--target", "cow", "--out", str(dest)])
    assert r.exit_code == 0, r.output
    assert calls[0][2] == ["--describe-schema", str(dest)]
    assert json.loads(dest.read_text(encoding="utf-8"))["header"]["git_commit"] == "abcdef12"
    assert not (root / "Cluiche" / "out").exists()


def test_missing_file_is_clear_error(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    stub_launcher(monkeypatch, [], write=False)
    r = run(["--target", "cow"])
    assert r.exit_code != 0
    assert "was not written" in r.output


def test_launch_failure_exit_code_is_error(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    monkeypatch.setattr(export_cmd, "launch_target", lambda *a, **k: 3)
    r = run(["--target", "cow"])
    assert r.exit_code != 0
    assert "exit 3" in r.output


def test_invalid_json_and_wrong_format(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    stub_launcher(monkeypatch, [], content="{ not json")
    r = run(["--target", "cow"])
    assert r.exit_code != 0 and "not valid JSON" in r.output
    stub_launcher(monkeypatch, [], content={"format": "other"})
    r = run(["--target", "cow"])
    assert r.exit_code != 0 and "dia.value_schema_export" in r.output


def test_sidecars_only(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch, dirty=False)

    def no_launch(*a, **k):
        raise AssertionError("launched")
    monkeypatch.setattr(export_cmd, "launch_target", no_launch)
    r = run(["--target", "cow", "--sidecars-only"])
    assert r.exit_code == 0, r.output
    out = root / "Cluiche" / "out" / "CoW" / "valueschema.json"
    doc = json.loads(out.read_text(encoding="utf-8"))
    assert doc["format"] == "dia.value_schema_export" and doc["version"] == 1
    assert doc["header"]["code_side"] is False
    assert doc["header"]["app"] == "cow" and doc["header"]["git_commit"] == "abcdef12"
    assert "undescribed" not in doc
    (e,) = doc["entries"]
    assert e["key"] == "cow.ecology_params/grazing.rate"
    assert e["origin"] == "sidecar:Global/Ecology/eco.schema.json"
    assert e["for"] == "cow.ecology_params" and e["max"] == 1 and e["unit"] == "x"
    assert sorted(x["id"] for x in doc["ramps"]) == ["cow.heat", "sequential.green"]
    assert "code-side coverage omitted: --sidecars-only" in r.output
    assert "1 entries" in r.output and str(out) in r.output


def test_validate_skips_when_code_side_false(tmp_path):
    ex = dict(VALID)
    ex["header"] = dict(VALID["header"], code_side=False)
    p = put(tmp_path, "e.json", ex)
    findings, notice = schema_codeside.run_code_side(None, p)
    assert findings == []
    assert notice == schema_codeside.NOTICE
    ok = put(tmp_path, "ok.json", VALID)
    findings, notice = schema_codeside.run_code_side(None, ok)
    assert notice is None and findings


def test_launch_target_extra_args_appended(monkeypatch, tmp_path):
    from dia_cli.cli import launch
    seen = {}
    exe = tmp_path / "Cluiche" / "out" / "CoW" / "x.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    (tmp_path / "pipeline.toml").write_text(
        '[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\nexe = "Cluiche/out/CoW/x.exe"\n')
    monkeypatch.setattr(launch, "find_repo_root", lambda _f: tmp_path)

    def fake_run(cmd, **kw):
        seen["cmd"] = cmd
        return subprocess.CompletedProcess(cmd, 0)
    monkeypatch.setattr(launch.subprocess, "run", fake_run)
    assert launch.launch_target("cow", "Debug", extra_args=["--describe-schema", "p"]) == 0
    assert seen["cmd"][-2:] == ["--describe-schema", "p"]


PIPELINE_TOML = "\n".join(['[targets.cow]', 'project = "x.vcxproj"', 'app_name = "CoW"',
                           'exe = "Cluiche/out/CoW/x.exe"', ''])


def test_automation_only_for_targets_that_parse_it(monkeypatch, tmp_path):
    from dia_cli.cli import launch
    assert launch.supports_automation("cluichetest") and not launch.supports_automation("cow")
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    seen = []

    def fake(target, config, **kw):
        seen.append((target, kw.get("automation"), kw.get("timeout")))
        args = kw["extra_args"]
        path = Path(args[args.index("--describe-schema") + 1])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(VALID), encoding="utf-8")
        return 0
    monkeypatch.setattr(export_cmd, "launch_target", fake)
    assert run(["--target", "cow"]).exit_code == 0
    assert run(["--target", "cow", "--timeout", "5"]).exit_code == 0
    assert seen == [("cow", False, 180.0), ("cow", False, 5.0)]
    seen.clear()
    monkeypatch.setattr(export_cmd, "supports_automation", lambda t: t == "cow")
    assert run(["--target", "cow"]).exit_code == 0
    assert seen == [("cow", True, 180.0)]


def test_timeout_is_clear_error(monkeypatch, tmp_path):
    root = make_repo(tmp_path)
    patch_repo(monkeypatch, root)
    patch_git(monkeypatch)
    from dia_cli.cli import launch
    monkeypatch.setattr(export_cmd, "launch_target", lambda *a, **k: launch.TIMEOUT_EXIT_CODE)
    r = run(["--target", "cow", "--timeout", "7"])
    assert r.exit_code != 0
    assert "describe mode did not finish within 7s: is ValueSchemaModule hosted by cow?" in r.output


def test_launch_target_timeout_terminates_tree(monkeypatch, tmp_path):
    from dia_cli.cli import launch
    exe = tmp_path / "Cluiche" / "out" / "CoW" / "x.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    (tmp_path / "pipeline.toml").write_text(
        PIPELINE_TOML)
    monkeypatch.setattr(launch, "find_repo_root", lambda _f: tmp_path)
    state = {"terminated": 0, "waits": []}

    class FakeProc:
        pid = 4242

        def wait(self, timeout=None):
            state["waits"].append(timeout)
            raise subprocess.TimeoutExpired("x", timeout)

    monkeypatch.setattr(launch.subprocess, "Popen", lambda cmd, **kw: FakeProc())
    monkeypatch.setattr(launch, "_terminate_tree", lambda proc: state.__setitem__("terminated", state["terminated"] + 1))
    assert launch.launch_target("cow", "Debug", timeout=2.5) == launch.TIMEOUT_EXIT_CODE
    assert state == {"terminated": 1, "waits": [2.5]}


def test_launch_target_with_timeout_returns_exit_code(monkeypatch, tmp_path):
    from dia_cli.cli import launch
    exe = tmp_path / "Cluiche" / "out" / "CoW" / "x.exe"
    exe.parent.mkdir(parents=True)
    exe.write_text("")
    (tmp_path / "pipeline.toml").write_text(
        PIPELINE_TOML)
    monkeypatch.setattr(launch, "find_repo_root", lambda _f: tmp_path)

    class FakeProc:
        def wait(self, timeout=None):
            return 3
    monkeypatch.setattr(launch.subprocess, "Popen", lambda cmd, **kw: FakeProc())
    assert launch.launch_target("cow", "Debug", timeout=10) == 3
