"""static-analysis stage: runs cppcheck, diffs against baseline, gates on new errors."""
import copy
import json
import time
from pathlib import Path


def _finding_key(result: dict) -> tuple:
    """Return the (ruleId, uri, startLine) identity tuple for a SARIF result."""
    rule_id = result.get("ruleId", "")
    locations = result.get("locations", [])
    if locations:
        phys = locations[0].get("physicalLocation", {})
        uri = phys.get("artifactLocation", {}).get("uri", "")
        start_line = phys.get("region", {}).get("startLine", 0)
    else:
        uri = ""
        start_line = 0
    return (rule_id, uri, start_line)


def _load_sarif_results(path: Path) -> list:
    """Load the results list from a SARIF file; return [] if file absent or malformed."""
    if not path.exists():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        runs = data.get("runs", [])
        if not runs:
            return []
        return runs[0].get("results", [])
    except (json.JSONDecodeError, KeyError, IndexError):
        return []


def _load_sarif(path: Path) -> dict:
    """Load a full SARIF document; return an empty SARIF structure if absent or malformed."""
    _EMPTY = {
        "version": "2.1.0",
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "runs": [{
            "tool": {
                "driver": {
                    "name": "Cppcheck",
                    "version": "unknown",
                    "rules": [],
                }
            },
            "results": [],
        }],
    }
    if not path.exists():
        return _EMPTY
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return _EMPTY


def _diff_findings(findings: list, baseline: list) -> list:
    """Return findings that have no matching entry in baseline (by ruleId + uri + startLine)."""
    baseline_keys = {_finding_key(r) for r in baseline}
    return [r for r in findings if _finding_key(r) not in baseline_keys]


def _build_delta_sarif(findings_doc: dict, new_results: list) -> dict:
    """Build a valid SARIF 2.1.0 document containing only new_results, copying tool info."""
    delta = copy.deepcopy(findings_doc)
    if delta.get("runs"):
        delta["runs"][0]["results"] = new_results
    return delta


def _result_summary_line(result: dict) -> str:
    locations = result.get("locations", [])
    if locations:
        phys = locations[0].get("physicalLocation", {})
        uri = phys.get("artifactLocation", {}).get("uri", "")
        line = phys.get("region", {}).get("startLine", "?")
        location_str = f"{uri}:{line}"
    else:
        location_str = "<no location>"
    rule_id = result.get("ruleId", "?")
    message = result.get("message", {}).get("text", "")
    return f"  {location_str}  {rule_id}  {message}"


def run(config, target, build_config, force, repo_root: Path, output=None, system: str = "pipeline") -> int:
    """Static-analysis pipeline stage.

    Runs cppcheck (via dia_cli.cli.check), diffs findings against baseline.sarif,
    writes delta.sarif, and gates on new error-level findings.
    """
    stage = "static-analysis"
    t_start = time.monotonic()

    # --- Step 1: run cppcheck via the check module ---
    if output:
        output.step_started(system=system, stage=stage, step="cppcheck")

    try:
        from dia_cli.cli.check import _run_cppcheck
        _run_cppcheck()
    except SystemExit as exc:
        code = exc.code if isinstance(exc.code, int) else 1
        err = f"cppcheck invocation failed (exit {code})"
        if output:
            output.step_failed(system=system, stage=stage, step="cppcheck", error=err)
        return code

    if output:
        output.step_completed(system=system, stage=stage, step="cppcheck")

    # --- Step 2: load findings and baseline ---
    out_dir = repo_root / "Cluiche" / "out" / "check"
    findings_path = out_dir / "findings.sarif"
    baseline_path = out_dir / "baseline.sarif"
    delta_path = out_dir / "delta.sarif"

    findings_doc = _load_sarif(findings_path)
    findings_results = findings_doc["runs"][0].get("results", []) if findings_doc.get("runs") else []
    baseline_results = _load_sarif_results(baseline_path)

    # --- Step 3: diff ---
    new_results = _diff_findings(findings_results, baseline_results)

    # --- Step 4: write delta.sarif ---
    out_dir.mkdir(parents=True, exist_ok=True)
    delta_doc = _build_delta_sarif(findings_doc, new_results)
    delta_path.write_text(json.dumps(delta_doc, indent=2), encoding="utf-8")

    elapsed = time.monotonic() - t_start

    # --- Step 5: gate ---
    new_errors = [r for r in new_results if r.get("level") == "error"]
    new_advisories = [r for r in new_results if r.get("level") != "error"]

    if new_advisories:
        advisory_lines = "\n".join(_result_summary_line(r) for r in new_advisories)
        print(
            f"[static-analysis] {len(new_advisories)} new advisory finding(s) "
            f"(warning/note — not blocking):\n{advisory_lines}"
        )

    if new_errors:
        error_lines = "\n".join(_result_summary_line(r) for r in new_errors)
        print(
            f"[static-analysis] GATE FAILED — {len(new_errors)} new error finding(s):\n"
            f"{error_lines}"
        )
        return 1

    total_findings = len(findings_results)
    baseline_count = len(baseline_results)
    print(
        f"[static-analysis] GATE PASSED — {len(new_results)} new finding(s) "
        f"({total_findings} total, {baseline_count} baselined) in {elapsed:.1f}s"
    )
    return 0
