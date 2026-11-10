"""dia validate schema: discovery, pairing and end-to-end flows (DiaValueSchema Feature 2 task 6)."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli
from dia_cli.commands.validate import schema_discovery


def entry(key, **kw):
    e = {"key": key, "kind": "parameter", "label": key, "description": f"the {key}", "type": "float"}
    e.update(kw)
    return e


def schema(fmt, *entries):
    return {"format": "dia.value_schema", "version": 1, "for": fmt, "entries": list(entries)}


def put(root: Path, rel: str, doc) -> Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(doc if isinstance(doc, str) else json.dumps(doc), encoding="utf-8")
    return p


def run(args, *, json_out=False):
    return CliRunner().invoke(validate_cli, ["schema", *args, *(["--json"] if json_out else [])])


def basic_tree(root: Path, rate=0.5, default=0.5):
    put(root, "Eco/eco.schema.json", schema("eco.params", entry("rate", min=0, max=1, default=default)))
    put(root, "Eco/eco.json", {"format": "eco.params", "version": 1, "rate": rate})


def test_ok_tree_exit_0(tmp_path):
    basic_tree(tmp_path)
    r = run([str(tmp_path), "--verbose"])
    assert r.exit_code == 0, r.output
    assert "OK   Eco/eco.json (1 keys)" in r.output
    assert "OK   Eco/eco.schema.json" in r.output
    assert "0 FAIL, 0 WARN" in r.output


def test_out_of_range_fails_with_relative_path(tmp_path):
    basic_tree(tmp_path, rate=7, default=0.5)
    r = run([str(tmp_path)])
    assert r.exit_code == 1
    assert "FAIL Eco/eco.json" in r.output
    assert "outside [0, 1]" in r.output


def test_json_output(tmp_path):
    basic_tree(tmp_path, rate=7)
    r = run([str(tmp_path)], json_out=True)
    data = json.loads(r.output)
    assert r.exit_code == 1
    assert any(f["rule"] == "data.out-of-range" and f["file"] == "Eco/eco.json" for f in data)


def test_format_without_schema_is_warn_exit_0(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/other.json", {"format": "mystery.thing", "version": 1})
    r = run([str(tmp_path)])
    assert r.exit_code == 0
    assert "WARN format `mystery.thing` has no schema" in r.output
    assert "0 FAIL, 1 WARN" in r.output


def test_no_format_in_schemad_folder_fails(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/loose.json", {"rate": 1})
    r = run([str(tmp_path)])
    assert r.exit_code == 1
    assert "no `format`; cannot find its schema" in r.output
    assert "FAIL Eco/loose.json" in r.output


def test_no_format_in_subfolder_of_schemad_folder_fails(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/sub/deeper/loose.json", {"rate": 1})
    assert run([str(tmp_path)]).exit_code == 1


def test_unrelated_json_ignored(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Other/random.json", {"hello": "world"})
    put(tmp_path, "Other/list.json", [1, 2])
    put(tmp_path, "Other/broken.json", "{not json")
    r = run([str(tmp_path)])
    assert r.exit_code == 0, r.output
    assert "0 FAIL, 0 WARN" in r.output


def test_catalogue_and_out_ignored(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/assets.catalogue.json", {"records": []})
    put(tmp_path, "Eco/x.catalogue.json", {"records": []})
    put(tmp_path, "Eco/out/stale.json", {"nothing": 1})
    put(tmp_path, "Eco/node_modules/pkg/package.json", {"name": "x"})
    r = run([str(tmp_path)])
    assert r.exit_code == 0, r.output


def test_bad_schema_does_not_hide_others_and_for_not_used(tmp_path):
    basic_tree(tmp_path)
    put(tmp_path, "Eco/bad.schema.json", {"format": "dia.value_schema", "version": 1, "for": "bad.fmt"})
    put(tmp_path, "Eco/badfmt.json", {"format": "bad.fmt", "version": 1})
    put(tmp_path, "Eco/eco2.json", {"format": "eco.params", "version": 1, "rate": 9})
    r = run([str(tmp_path)])
    assert r.exit_code == 1
    assert "FAIL Eco/bad.schema.json" in r.output
    assert "FAIL Eco/eco2.json" in r.output
    assert "WARN format `bad.fmt` has no schema" in r.output


def test_pack_skips_default_check_defaults_file_does_not(tmp_path):
    basic_tree(tmp_path, rate=0.9, default=0.5)  # defaults file differs from default -> FAIL
    put(tmp_path, "Eco/Packs/p.json", {"format": "eco.params", "version": 1, "rate": 0.9})
    data = json.loads(run([str(tmp_path)], json_out=True).output)
    mism = [f for f in data if f["rule"] == "data.default-mismatch"]
    assert [f["file"] for f in mism] == ["Eco/eco.json"]


def test_packs_detection_case_insensitive(tmp_path):
    put(tmp_path, "Eco/eco.schema.json", schema("eco.params", entry("rate", default=0.5)))
    put(tmp_path, "Eco/packs/p.json", {"format": "eco.params", "version": 1, "rate": 0.9})
    assert run([str(tmp_path)]).exit_code == 0


def test_paths_file_checked_as_given(tmp_path):
    basic_tree(tmp_path, rate=7)
    r = run([str(tmp_path / "Eco" / "eco.json")])
    # a lone data file has no schema in scope -> warn only
    assert r.exit_code == 0 and "has no schema" in r.output
    r = run([str(tmp_path / "Eco" / "eco.schema.json"), str(tmp_path / "Eco" / "eco.json")])
    assert r.exit_code == 1 and "outside [0, 1]" in r.output


def test_duplicate_key_across_schema_files(tmp_path):
    put(tmp_path, "A/a.schema.json", schema("eco.params", entry("rate")))
    put(tmp_path, "B/b.schema.json", schema("eco.params", entry("rate")))
    data = json.loads(run([str(tmp_path)], json_out=True).output)
    dup = [f for f in data if f["rule"] == "schema.duplicate-key-across-files"]
    assert len(dup) == 1
    assert dup[0]["file"] == "B/b.schema.json"
    assert "A/a.schema.json" in dup[0]["message"] and "B/b.schema.json" in dup[0]["message"]


def test_unresolved_affects_reported(tmp_path):
    put(tmp_path, "A/a.schema.json", schema("eco.params", entry("rate", affects=["nope"])))
    r = run([str(tmp_path)])
    assert r.exit_code == 1 and "affects[0]" in r.output


def test_ref_across_formats(tmp_path):
    put(tmp_path, "A/s.schema.json", schema("eco.species", entry("species[].id", type="string")))
    put(tmp_path, "A/t.schema.json",
        schema("eco.biomes", entry("biomes[].species", type="ref", ref="eco.species/species[].id")))
    put(tmp_path, "A/species.json", {"format": "eco.species", "version": 1, "species": [{"id": "wolf"}]})
    put(tmp_path, "A/biomes.json", {"format": "eco.biomes", "version": 1, "biomes": [{"species": "wolfe"}]})
    r = run([str(tmp_path)])
    assert r.exit_code == 1 and "matches no id" in r.output and "wolf" in r.output


def test_deterministic_ordering(tmp_path):
    for n in ("b", "a", "c"):
        put(tmp_path, f"{n}/{n}.json", {"format": f"x.{n}", "version": 1})
    out1 = run([str(tmp_path)]).output
    out2 = run([str(tmp_path)]).output
    assert out1 == out2
    assert out1.index("x.a") < out1.index("x.b") < out1.index("x.c")


def test_unknown_target_exit_2(monkeypatch, tmp_path):
    (tmp_path / "pipeline.toml").write_text('[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\n')
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: tmp_path)
    r = run(["--target", "nonesuch"])
    assert r.exit_code == 2
    assert "nonesuch" in r.output


def test_target_resolves_assets_and_aliases(monkeypatch, tmp_path):
    (tmp_path / "pipeline.toml").write_text('[targets.cow]\nproject = "x.vcxproj"\napp_name = "CoW"\n')
    assets = tmp_path / "Cluiche" / "Assets" / "CoW"
    put(assets, "cow.diagame", {"config": {"path_aliases": {"root": ".", "extra": "../../Elsewhere"}}})
    basic_tree(assets / "Global")
    put(tmp_path / "Cluiche" / "Elsewhere", "e.json", {"format": "zzz", "version": 1})
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: tmp_path)
    r = run(["--target", "cow"])
    assert r.exit_code == 0, r.output
    assert "format `zzz` has no schema" in r.output
    roots = [p.resolve() for p in schema_discovery.target_roots("COW", tmp_path)]
    assert assets.resolve() in roots
    assert (tmp_path / "Cluiche" / "Elsewhere").resolve() in roots


def test_default_scans_every_app_asset_root(monkeypatch, tmp_path):
    (tmp_path / "pipeline.toml").write_text("")
    basic_tree(tmp_path / "Cluiche" / "Assets" / "AppA", rate=7)
    basic_tree(tmp_path / "Cluiche" / "Assets" / "AppB")
    monkeypatch.setattr(schema_discovery, "get_repo_root", lambda: tmp_path)
    r = run([])
    assert r.exit_code == 1
    assert "FAIL AppA/Eco/eco.json" in r.output


def test_smoke_real_repo_no_args():
    r = run([])
    assert r.exit_code in (0, 1), r.output
    assert r.exception is None or isinstance(r.exception, SystemExit)
    assert "Traceback" not in r.output
    assert "files checked" in r.output
