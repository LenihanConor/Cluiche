"""Seeded property tests (F1-F7) for schema_rules / data_rules / schema_suggest."""
from __future__ import annotations

import copy
import json
import random
from collections import Counter
from dataclasses import asdict

import pytest

from dia_cli.commands.validate.data_rules import (
    DataContext, build_schema_index, validate_data_document, validate_schema_references)
from dia_cli.commands.validate.schema_cmd import Finding, _to_json
from dia_cli.commands.validate.schema_rules import validate_schema_document, validate_schema_text
from dia_cli.commands.validate.schema_suggest import format_suggestion, suggest

SEEDS = [1234 + i for i in range(8)]
ALPHA = ["a", "b", "growth", "x.y", "a[]", "é", "中", "", "k1", "id", "format", "version", "ui", "default"]
TYPES = ["float", "int", "bool", "enum", "string", "ref", "ref[]", "map"]
KINDS = ["parameter", "field", "metric", "arg", "command", "event", "component_field", "zzz"]


def rjson(rng: random.Random, depth: int = 0):
    leaf = rng.random() < 0.35 or depth >= 6
    if leaf:
        c = rng.randrange(9)
        if c == 0:
            return None
        if c == 1:
            return rng.random() < 0.5
        if c == 2:
            return rng.randint(-5, 5)
        if c == 3:
            return rng.choice([0.5, -0.0, 1e308, 2.0, 1e-320])
        if c == 4:
            return rng.choice([float("nan"), float("inf"), -float("inf")])
        if c == 5:
            return 10 ** rng.randint(0, 400)
        return rng.choice(ALPHA + ["dia.value_schema", "forest", " "])
    if rng.random() < 0.5:
        return [rjson(rng, depth + 1) for _ in range(rng.randint(0, 4))]
    return {rng.choice(ALPHA): rjson(rng, depth + 1) for _ in range(rng.randint(0, 4))}


def rentry(rng: random.Random) -> dict:
    t = rng.choice(TYPES)
    e = {"key": rng.choice(ALPHA[:9] + ["a[].b", "a[].c", "g.h"]), "kind": rng.choice(KINDS[:3] * 3 + KINDS),
         "label": "L", "description": rng.choice(["d", "d", " "]), "type": t}
    if rng.random() < 0.4:
        e["min"] = rng.choice([0, -1, 2.5, "x", float("nan"), None])
    if rng.random() < 0.4:
        e["max"] = rng.choice([1, 10, 3.5, float("inf"), True])
    if t == "enum" or rng.random() < 0.1:
        e["values"] = [{"id": rng.choice(["red", "green", 5, "blue"])} for _ in range(rng.randint(0, 3))]
    if t in ("ref", "ref[]"):
        e["ref"] = rng.choice(["f/p", "x", "cow.biomes/biomes[].id", 5])
    if t == "map":
        e["keyRef"] = rng.choice(["f/p", "x"])
        e["valueType"] = rng.choice(["float", "int", "enum", "ref"])
    if rng.random() < 0.3:
        e["default"] = rjson(rng, 5)
    for flag in ("unique", "required"):
        if rng.random() < 0.2:
            e[flag] = rng.choice([True, False, 1])
    if rng.random() < 0.2:
        e["ui"] = rng.choice([{"group": "g"}, {"zz": 1}, [], {"ramp": "nope"}, {"ramp": "sequential.blue"}])
    if rng.random() < 0.2:
        e["affects"] = rng.choice([[], ["a"], ["zzz"], ["f/a"], "x", [1]])
    return e


def rschema(rng: random.Random, fmt: str = "x") -> dict:
    return {"format": "dia.value_schema", "version": 1, "for": fmt,
            "entries": [rentry(rng) for _ in range(rng.randint(0, 8))]}


def mutate(rng: random.Random, node, p=0.15):
    if rng.random() < p:
        return rjson(rng, 4)
    if isinstance(node, dict):
        return {k: mutate(rng, v, p) for k, v in node.items()}
    if isinstance(node, list):
        return [mutate(rng, v, p) for v in node]
    return node


def rdata(rng: random.Random, fmt: str = "x") -> dict:
    d = {"format": fmt, "version": 1}
    for _ in range(rng.randint(0, 6)):
        d[rng.choice(ALPHA[:9] + ["a", "g"])] = rng.choice([
            lambda: rjson(rng, 3),
            lambda: [{"b": rjson(rng, 5), "c": rjson(rng, 5), "id": rng.choice(["i", 5, None])}
                     for _ in range(rng.randint(0, 3))],
            lambda: {"h": rjson(rng, 4)},
        ])()
    return d


def shuffle_keys(rng: random.Random, node):
    if isinstance(node, dict):
        items = list(node.items())
        rng.shuffle(items)
        return {k: shuffle_keys(rng, v) for k, v in items}
    if isinstance(node, list):
        return [shuffle_keys(rng, v) for v in node]
    return node


def ident(findings):
    return Counter((f.severity, f.rule, f.key, f.path) for f in findings)


def deep_equal(a, b):
    """== that treats NaN as equal to NaN."""
    return json.dumps(a, sort_keys=True, default=repr) == json.dumps(b, sort_keys=True, default=repr)


def check_findings(findings):
    assert isinstance(findings, list)
    for f in findings:
        assert isinstance(f, Finding)
        assert f.severity in ("FAIL", "WARN")
        assert isinstance(f.rule, str) and f.rule
        assert isinstance(f.message, str)


def schema_docs(seed, n=40):
    rng = random.Random(seed)
    for _ in range(n):
        yield rjson(rng)
        yield mutate(rng, rschema(rng))
        yield rschema(rng)


def data_cases(seed, n=40):
    rng = random.Random(seed)
    for _ in range(n):
        schemas = [rschema(rng) for _ in range(rng.randint(1, 2))]
        if rng.random() < 0.6:  # mostly-sane schemas so the data rules (range/type/records) are exercised
            for sch in schemas:
                for e in sch["entries"]:
                    e["kind"] = "parameter"
                    e["key"] = rng.choice(["a", "b", "g.h", "a[].b", "a[].c", "a[].id"])
                    e["min"] = rng.choice([0, -2, 1])
                    e["max"] = rng.choice([1, 5])
        yield schemas, rdata(rng), rng.random() < 0.3


# ---------------------------------------------------------------- F1 never raise
@pytest.mark.parametrize("seed", SEEDS)
def test_f1_schema_document_never_raises(seed):
    for doc in schema_docs(seed):
        check_findings(validate_schema_document(doc, "f.json"))


@pytest.mark.parametrize("seed", SEEDS)
def test_f1_schema_text_never_raises(seed):
    rng = random.Random(seed)
    for _ in range(40):
        text = json.dumps(mutate(rng, rschema(rng)), allow_nan=True)
        check_findings(validate_schema_text(text, "f"))
        cut = text[:rng.randint(0, len(text))]
        out = validate_schema_text(cut, "f")
        check_findings(out)


@pytest.mark.parametrize("seed", SEEDS)
def test_f1_data_document_never_raises(seed):
    for schemas, data, pack in data_cases(seed):
        idx = build_schema_index(schemas)
        files = [("d.json", data, pack)]
        for ctx in (None, DataContext.from_files(files)):
            check_findings(validate_data_document(data, "d.json", idx, is_pack=pack, context=ctx))
        check_findings(validate_schema_references(schemas, {id(s): "s" for s in schemas}))


@pytest.mark.parametrize("seed", SEEDS)
def test_f1_data_with_garbage_documents(seed):
    rng = random.Random(seed)
    schemas = [rschema(rng) for _ in range(3)] + [rjson(rng) for _ in range(5)]
    idx = build_schema_index(schemas)
    for _ in range(60):
        data = rjson(rng)
        check_findings(validate_data_document(data, "d", idx, is_pack=False))
        if isinstance(data, dict):
            data["format"] = "x"
            check_findings(validate_data_document(data, "d", idx, is_pack=rng.random() < 0.5,
                                                  context=DataContext.from_files([("d", data, False)])))
    check_findings(validate_schema_references(schemas, {}))


# ---------------------------------------------------------------- F2 key-order independence
@pytest.mark.parametrize("seed", SEEDS)
def test_f2_schema_results_independent_of_key_order(seed):
    rng = random.Random(seed * 7)
    for doc in schema_docs(seed, 25):
        shuffled = shuffle_keys(rng, doc)
        assert ident(validate_schema_document(doc, "f")) == ident(validate_schema_document(shuffled, "f")), doc


@pytest.mark.parametrize("seed", SEEDS)
def test_f2_data_results_independent_of_key_order(seed):
    rng = random.Random(seed * 11)
    for schemas, data, pack in data_cases(seed, 25):
        idx = build_schema_index(schemas)
        a = validate_data_document(data, "d", idx, is_pack=pack)
        b = validate_data_document(shuffle_keys(rng, data), "d", idx, is_pack=pack)
        assert ident(a) == ident(b), data


# ---------------------------------------------------------------- F4 suggestions
@pytest.mark.parametrize("seed", SEEDS)
def test_f4_suggestion_in_valid_set_and_in_message(seed):
    rng = random.Random(seed)
    for schemas, data, pack in data_cases(seed, 40):
        idx = build_schema_index(schemas)
        files = [("d.json", data, False), ("e.json", {"format": "f", "version": 1, "p": ["growth", "forest"]}, False)]
        out = validate_data_document(data, "d", idx, is_pack=pack, context=DataContext.from_files(files))
        out += validate_schema_references(schemas, {})
        for f in out:
            if f.suggestion is not None:
                assert f.suggestion in f.message, (f.rule, f.message)
                assert format_suggestion(f.suggestion) in f.message
    alphabet = "abcdefg"
    for _ in range(300):
        valid = ["".join(rng.choice(alphabet) for _ in range(rng.randint(1, 7))) for _ in range(rng.randint(0, 5))]
        cand = "".join(rng.choice(alphabet) for _ in range(rng.randint(0, 7)))
        s = suggest(cand, valid)
        assert s is None or (s in valid and s != cand)


# ---------------------------------------------------------------- F5 no mutation / idempotent
@pytest.mark.parametrize("seed", SEEDS)
def test_f5_schema_validation_is_pure(seed):
    for doc in schema_docs(seed, 25):
        before = copy.deepcopy(doc)
        r1 = validate_schema_document(doc, "f")
        r2 = validate_schema_document(doc, "f")
        assert deep_equal(doc, before)
        assert r1 == r2 or deep_equal([asdict(x) for x in r1], [asdict(x) for x in r2])


@pytest.mark.parametrize("seed", SEEDS)
def test_f5_data_validation_is_pure(seed):
    for schemas, data, pack in data_cases(seed, 25):
        bs, bd = copy.deepcopy(schemas), copy.deepcopy(data)
        idx = build_schema_index(schemas)
        ctx = DataContext.from_files([("d", data, pack)])
        r1 = validate_data_document(data, "d", idx, is_pack=pack, context=ctx)
        r2 = validate_data_document(data, "d", idx, is_pack=pack, context=ctx)
        v1 = validate_schema_references(schemas, {})
        v2 = validate_schema_references(schemas, {})
        assert deep_equal(schemas, bs) and deep_equal(data, bd)
        assert [asdict(x) for x in r1] == [asdict(x) for x in r2] or deep_equal(r1, r2)
        assert deep_equal([asdict(x) for x in v1], [asdict(x) for x in v2])


# ---------------------------------------------------------------- F6 defaults file from schema defaults
def _default_for(rng, t):
    if t == "float":
        return rng.choice([0, 0.5, 1.0, 2])
    if t == "int":
        return rng.randint(0, 5)
    if t == "bool":
        return rng.random() < 0.5
    if t == "string":
        return rng.choice(["s", "é", ""])
    return "red"


@pytest.mark.parametrize("seed", SEEDS)
def test_f6_defaults_document_validates_clean(seed):
    rng = random.Random(seed)
    for _ in range(30):
        entries, doc, n = [], {"format": "x", "version": 1}, 0
        for i in range(rng.randint(1, 8)):
            t = rng.choice(["float", "int", "bool", "string", "enum"])
            key = rng.choice([f"k{i}", f"g{i}.k", f"g{i}.h.k"])
            d = _default_for(rng, t)
            e = {"key": key, "kind": rng.choice(["parameter", "field"]), "label": key, "description": "d",
                 "type": t, "default": d}
            if t in ("float", "int"):
                e["min"], e["max"] = -10, 10
            if t == "enum":
                e["values"] = [{"id": "red"}, {"id": "blue"}]
            entries.append(e)
            node = doc
            parts = key.split(".")
            for p in parts[:-1]:
                node = node.setdefault(p, {})
            node[parts[-1]] = d
        schema = {"format": "dia.value_schema", "version": 1, "for": "x", "entries": entries}
        assert [f for f in validate_schema_document(schema, "s") if f.severity == "FAIL"] == []
        idx = build_schema_index([schema])
        assert validate_data_document(doc, "d", idx, is_pack=False) == []
        assert validate_data_document(doc, "d", idx, is_pack=True) == []
        assert validate_schema_references([schema], {}) == []


@pytest.mark.parametrize("seed", SEEDS)
def test_f6_perturbed_default_is_mismatch(seed):
    rng = random.Random(seed)
    for _ in range(20):
        e = {"key": "k", "kind": "parameter", "label": "k", "description": "d", "type": "int", "default": 3,
             "min": 0, "max": 10}
        idx = build_schema_index([{"format": "dia.value_schema", "version": 1, "for": "x", "entries": [e]}])
        v = rng.choice([v for v in range(0, 11) if v != 3])
        out = validate_data_document({"format": "x", "k": v}, "d", idx, is_pack=False)
        assert [f.rule for f in out] == ["data.default-mismatch"]
        assert validate_data_document({"format": "x", "k": v}, "d", idx, is_pack=True) == []


# ---------------------------------------------------------------- F7 Finding JSON round trip
def _rstr(rng):
    pool = ["a", "é", "中", "\x00", "\x1f", "\n", "\t", '"', "\\", "\ud800", "\udfff", "\U0001f600", "'", "/"]
    return "".join(rng.choice(pool) for _ in range(rng.randint(0, 12)))


@pytest.mark.parametrize("seed", SEEDS)
def test_f7_finding_json_round_trip(seed):
    rng = random.Random(seed)
    for _ in range(100):
        f = Finding(rng.choice(["FAIL", "WARN"]), "data.x", _rstr(rng), rng.choice([None, _rstr(rng)]), _rstr(rng),
                    rng.choice([None, _rstr(rng)]), rng.choice([None, _rstr(rng)]), rng.choice([None, _rstr(rng)]))
        text = json.dumps([_to_json(f)], indent=2)
        back = json.loads(text)[0]
        expect = _to_json(f)
        # an adjacent high+low surrogate pair legitimately re-joins into one astral code point on decode,
        # so compare the canonical (ensure_ascii) encodings
        assert json.dumps(back) == json.dumps(expect)
        assert back.keys() == expect.keys()


@pytest.mark.parametrize("seed", SEEDS)
def test_f7_real_findings_round_trip(seed):
    for schemas, data, pack in data_cases(seed, 25):
        idx = build_schema_index(schemas)
        out = validate_data_document(data, "d", idx, is_pack=pack, context=DataContext.from_files([("d", data, False)]))
        out += validate_schema_document(schemas[0], "s")
        for f in out:
            blob = json.dumps(_to_json(f))
            assert json.dumps(json.loads(blob)) == blob
