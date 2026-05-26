"""dia check — static analysis and sanitizer checks."""
import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
import click
from pathlib import Path

from dia_cli.utils.repo_root import find_repo_root


_CONFIG_ALIASES = {"Asan": "Debug-Asan", "Ubsan": "Debug-Ubsan"}

_SANITIZER_OUTPUT_NAMES = {
    "Debug-Asan": "sanitizer-asan.txt",
    "Debug-Ubsan": "sanitizer-ubsan.txt",
}


@click.command()
@click.option("--tool", default=None, metavar="TOOL",
              help="Check tool to run: cppcheck (default) or sanitizer.")
@click.option("--config", default="Asan", metavar="CONFIG",
              help="Sanitizer config: Asan or Ubsan (default: Asan). Only used with --tool=sanitizer.")
@click.option("--accept-baseline", "accept_baseline", is_flag=True, default=False,
              help="Promote current findings.sarif to baseline.sarif.")
def cli(tool, config, accept_baseline):
    """Run a code-quality check against the codebase.

    With no --tool, runs cppcheck and writes SARIF to Cluiche/out/check/findings.sarif.
    With --tool=sanitizer, builds and runs googletest under ASan or UBSan and
    writes findings to Cluiche/out/check/sanitizer-{asan|ubsan}.txt.
    With --accept-baseline, promotes findings.sarif to baseline.sarif.
    """
    if accept_baseline:
        _accept_baseline()
        return

    if tool is None or tool == "cppcheck":
        _run_cppcheck()
        return

    if tool == "sanitizer":
        _run_sanitizer(config)
    else:
        click.echo(f"ERROR: unknown tool '{tool}'. Known tools: cppcheck, sanitizer", err=True)
        raise SystemExit(2)


def _accept_baseline() -> None:
    repo_root = find_repo_root(__file__)
    out_dir = repo_root / "Cluiche" / "out" / "check"
    findings_path = out_dir / "findings.sarif"
    baseline_path = out_dir / "baseline.sarif"

    if not findings_path.exists():
        click.echo(
            "ERROR: findings.sarif not found. Run 'dia check' first to generate it.",
            err=True,
        )
        raise SystemExit(1)

    try:
        data = json.loads(findings_path.read_text(encoding="utf-8"))
        runs = data.get("runs", [])
        finding_count = len(runs[0].get("results", [])) if runs else 0
    except (json.JSONDecodeError, IndexError):
        finding_count = 0

    shutil.copy2(str(findings_path), str(baseline_path))
    click.echo(
        f"[dia check] Accepted {finding_count} finding(s) as baseline → {baseline_path}"
    )


def _run_cppcheck() -> None:
    if not shutil.which("cppcheck"):
        click.echo(
            "ERROR: cppcheck not found on PATH. "
            "Install with: winget install Cppcheck.Cppcheck",
            err=True,
        )
        raise SystemExit(1)

    repo_root = find_repo_root(__file__)

    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    xml_path = out_dir / "findings.xml"

    cmd = [
        "cppcheck",
        "--enable=warning,performance,portability",
        f"--output-file={xml_path}",
        "--xml", "--xml-version=2",
        "--suppress=*:External/*",
        "--suppress=*:Cluiche/bin/*",
        "--suppress=*:Cluiche/out/*",
        "--max-ctu-depth=4",
        str(repo_root / "Dia"),
        str(repo_root / "Cluiche"),
    ]

    suppressions_file = repo_root / ".cppcheck-suppressions.xml"
    if suppressions_file.exists():
        cmd.insert(1, f"--suppress-xml={suppressions_file}")

    click.echo("[dia check] Running cppcheck (this may take a while)...")
    try:
        result = subprocess.run(
            cmd,
            timeout=300,
        )
    except subprocess.TimeoutExpired:
        click.echo("ERROR: cppcheck timed out after 300 seconds", err=True)
        raise SystemExit(1)

    xml_text = xml_path.read_text(encoding="utf-8", errors="replace") if xml_path.exists() else ""

    sarif = _cppcheck_xml_to_sarif(xml_text)

    sarif_path = out_dir / "findings.sarif"
    sarif_path.write_text(json.dumps(sarif, indent=2), encoding="utf-8")

    finding_count = len(sarif["runs"][0]["results"]) if sarif.get("runs") else 0
    click.echo(f"[dia check] {finding_count} finding(s) written to {sarif_path}")


def _cppcheck_xml_to_sarif(xml_text: str) -> dict:
    _SEVERITY_MAP = {
        "error": "error",
        "warning": "warning",
        "performance": "note",
        "portability": "note",
        "style": "note",
    }

    _EMPTY_SARIF = {
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

    if not xml_text or not xml_text.strip():
        return _EMPTY_SARIF

    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError:
        return _EMPTY_SARIF

    cppcheck_version = "unknown"
    cppcheck_el = root.find("cppcheck")
    if cppcheck_el is not None:
        cppcheck_version = cppcheck_el.get("version", "unknown")

    errors_el = root.find("errors")
    if errors_el is None:
        return _EMPTY_SARIF

    rules_by_id: dict = {}
    results = []

    for error in errors_el.findall("error"):
        rule_id = error.get("id", "unknown")
        severity = error.get("severity", "warning")
        msg = error.get("msg", "")

        level = _SEVERITY_MAP.get(severity, "warning")

        if rule_id not in rules_by_id:
            rules_by_id[rule_id] = {
                "id": rule_id,
                "name": rule_id,
                "shortDescription": {"text": error.get("verbose", msg) or msg},
                "defaultConfiguration": {"level": level},
            }

        location_el = error.find("location")
        if location_el is not None:
            raw_file = location_el.get("file", "")
            line_str = location_el.get("line", "1")
            col_str = location_el.get("column", "1")

            # Convert absolute path to forward-slash URI relative to repo root
            try:
                repo_root = find_repo_root(__file__)
                uri = Path(raw_file).resolve().relative_to(repo_root.resolve())
                uri_str = uri.as_posix()
            except (ValueError, TypeError):
                uri_str = raw_file.replace("\\", "/")

            try:
                start_line = int(line_str)
            except (ValueError, TypeError):
                start_line = 1

            locations = [{
                "physicalLocation": {
                    "artifactLocation": {
                        "uri": uri_str,
                        "uriBaseId": "%SRCROOT%",
                    },
                    "region": {"startLine": start_line},
                }
            }]
        else:
            locations = []

        results.append({
            "ruleId": rule_id,
            "level": level,
            "message": {"text": msg},
            "locations": locations,
        })

    return {
        "version": "2.1.0",
        "$schema": "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/master/Schemata/sarif-schema-2.1.0.json",
        "runs": [{
            "tool": {
                "driver": {
                    "name": "Cppcheck",
                    "version": cppcheck_version,
                    "rules": list(rules_by_id.values()),
                }
            },
            "results": results,
        }],
    }


def _run_sanitizer(config: str) -> None:
    normalized = _CONFIG_ALIASES.get(config, config)
    if normalized not in _SANITIZER_OUTPUT_NAMES:
        click.echo(
            f"ERROR: unknown sanitizer config '{config}'. Use Asan or Ubsan.",
            err=True,
        )
        raise SystemExit(2)

    repo_root = find_repo_root(__file__)

    # Step 1: Build
    click.echo(f"[dia check] Building googletest with config {normalized} ...")
    build_result = subprocess.run(
        ["dia", "run", "googletest", f"--config={normalized}", "--build-only"],
        cwd=str(repo_root),
    )
    if build_result.returncode != 0:
        click.echo(
            f"[dia check] Build failed (exit {build_result.returncode})", err=True
        )
        raise SystemExit(build_result.returncode)

    # Step 2: Run and capture stderr
    exe_path = (
        repo_root
        / "Cluiche"
        / "bin"
        / "GoogleTests"
        / normalized
        / "x64"
        / "GoogleTests.exe"
    )
    if not exe_path.exists():
        click.echo(f"ERROR: {exe_path} not found after build.", err=True)
        raise SystemExit(1)

    click.echo(f"[dia check] Running {exe_path.name} under {normalized} ...")
    run_result = subprocess.run(
        [str(exe_path)],
        cwd=str(exe_path.parent),
        capture_output=True,
        text=True,
    )

    # Step 3: Write captured stderr to output file
    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / _SANITIZER_OUTPUT_NAMES[normalized]
    out_file.write_text(run_result.stderr, encoding="utf-8")

    # Step 4: Count and report sanitizer findings
    findings = [
        line for line in run_result.stderr.splitlines()
        if "ERROR:" in line or "runtime error:" in line
    ]
    count = len(findings)

    click.echo(f"[dia check] Output written to {out_file}")
    if count == 0:
        click.echo("[dia check] No sanitizer findings detected.")
    else:
        click.echo(f"[dia check] {count} sanitizer finding(s) detected.")

    if run_result.returncode != 0:
        raise SystemExit(run_result.returncode)
