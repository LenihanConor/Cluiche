"""dia validate schema: `valuesFrom` enum resolution (biome-registry plan task 13)."""
import json
from pathlib import Path

from click.testing import CliRunner

from dia_cli.cli.cli_validate import cli as validate_cli
from dia_cli.commands.validate.data_rules import (
    DataContext, build_schema_index, validate_data_document,
)
from dia_cli.commands.validate.schema_rules import validate_schema_document


def put(root: Path, rel: str, doc) -> Path:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(doc), encoding="utf-8")
    return p


def run(args):
    r = CliRunner().invoke(validate_cli, ["schema", *args, "--json"])
    return r, json.loads(r.output)


VF = {"format": "cow.biomes", "records": "biomes", "id": "id", "label": "name"}


def vf_schema(vf=VF, **kw):
    e = {"key": "biomeIndex", "kind": "field", "label": "B", "description": "D", "type": "enum", "valuesFrom": vf}
    e.update(kw)
    return {"format": "dia.value_schema", "version": 1, "for": "cow.test", "entries": [e]}


def biomes(*recs):
    return {"format": "cow.biomes", "version": 1, "biomes": list(recs)}


def tree(root, vf=VF, biome_doc=None, value="forest"):
    put(root, "A/test.schema.json", vf_schema(vf))
    put(root, "A/test.json", {"format": "cow.test", "version": 1, "biomeIndex": value})
    if biome_doc is not None:
        put(root, "A/biomes.json", biome_doc)


def fails(out):
    return [f for f in (out if isinstance(out, list) else out["findings"]) if f["severity"] == "FAIL"]


def test_resolves_ok(tmp_path):
    tree(tmp_path, biome_doc=biomes({"id": "forest", "name": "Forest"}))
    r, out = run([str(tmp_path)])
    assert not fails(out), out
    assert r.exit_code == 0


def test_no_file_for_format(tmp_path):
    tree(tmp_path)
    _, out = run([str(tmp_path)])
    f = [x for x in fails(out) if x["rule"] == "schema.values-from-no-file"]
    assert len(f) == 1 and f[0]["key"] == "biomeIndex" and "cow.biomes" in f[0]["message"]


def test_bad_records_missing_and_not_array(tmp_path):
    tree(tmp_path, biome_doc={"format": "cow.biomes", "version": 1, "other": []})
    _, out = run([str(tmp_path)])
    assert [x["key"] for x in fails(out) if x["rule"] == "schema.values-from-bad-records"] == ["biomeIndex"]
    put(tmp_path, "A/biomes.json", {"format": "cow.biomes", "version": 1, "biomes": {"id": "x"}})
    _, out = run([str(tmp_path)])
    assert [x["key"] for x in fails(out) if x["rule"] == "schema.values-from-bad-records"] == ["biomeIndex"]


def test_missing_id_and_label_field_name_record_and_field(tmp_path):
    tree(tmp_path, biome_doc=biomes({"id": "forest", "name": "F"}, {"name": "NoId"}, {"id": "bog"}))
    _, out = run([str(tmp_path)])
    msgs = [x["message"] for x in fails(out) if x["rule"] == "schema.values-from-missing-field"]
    assert any("biomes[1]" in m and "'id'" in m for m in msgs), msgs
    assert any("biomes[2]" in m and "'name'" in m for m in msgs), msgs


def test_data_value_checked_against_resolved_ids(tmp_path):
    tree(tmp_path, biome_doc=biomes({"id": "forest", "name": "F"}), value="swamp")
    _, out = run([str(tmp_path)])
    bad = [x for x in fails(out) if x["rule"] == "data.bad-enum"]
    assert len(bad) == 1 and "forest" in bad[0]["message"]


def test_unresolved_values_from_does_not_crash_or_flag_data():
    doc = vf_schema()
    idx = build_schema_index([doc])
    d = {"format": "cow.test", "version": 1, "biomeIndex": "anything"}
    assert validate_data_document(d, "d.json", idx, is_pack=False) == []
    ctx = DataContext.from_files([("b.json", {"format": "cow.biomes", "biomes": 3}, False)])
    assert validate_data_document(d, "d.json", idx, is_pack=False, context=ctx) == []


def test_enum_array_with_values_from_is_rejected():
    doc = vf_schema(type="enum[]")
    assert [f.rule for f in validate_schema_document(doc, "s.json")] == ["schema.values-from-misuse"]


def test_enum_without_values_or_values_from_message_mentions_values_from():
    doc = vf_schema()
    del doc["entries"][0]["valuesFrom"]
    f = validate_schema_document(doc, "s.json")
    assert f[0].rule == "schema.enum-no-values" and "valuesFrom" in f[0].message
