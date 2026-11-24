"""Results file in the exact format the C++ StageResultsFile writes must be accepted by load_results."""
import json

from dia_cli.utils.stage_catalogue_view import load_results

SAMPLE = {
    "format": "dia.stage_results",
    "version": 1,
    "results": {
        "Alpha": {"severity": "warn", "label": "0123456789abcdef", "utc": 1700000000123,
                  "session_id": "sess-1", "log_path": "out/CluicheTest/session/log.txt"},
        "Beta": {"severity": "none", "label": "", "utc": 0, "session_id": "", "log_path": ""},
    },
}


def test_load_results_accepts_cpp_format(tmp_path):
    p = tmp_path / "results.json"
    p.write_text(json.dumps(SAMPLE, indent=4), encoding="utf-8")
    got = load_results(p)
    assert set(got) == {"Alpha", "Beta"}
    assert got["Alpha"]["severity"] == "warn"
    assert got["Alpha"]["utc"] == 1700000000123
    assert got["Alpha"]["session_id"] == "sess-1"
    assert got["Alpha"]["log_path"].endswith("log.txt")
    assert got["Alpha"]["label"] == "0123456789abcdef"


def test_load_results_rejects_other_version(tmp_path):
    p = tmp_path / "results.json"
    p.write_text(json.dumps({**SAMPLE, "version": 2}), encoding="utf-8")
    assert load_results(p) == {}
