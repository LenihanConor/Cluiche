"""clone_checker — detect copy-paste duplication via PMD CPD."""
from __future__ import annotations

import json
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


def _find_cpd_executable(repo_root: Path | None = None) -> str | None:
    """Return the PMD executable path, preferring the repo-local install.

    PMD 7.x removed standalone cpd.bat; CPD runs as `pmd cpd ...`.
    Returns the path to pmd.bat (or pmd on Unix) for use with _run_cpd().
    """
    if repo_root is not None:
        local = repo_root / "External" / "pmd" / "bin" / "pmd.bat"
        if local.exists():
            return str(local)
    for name in ("pmd", "pmd.bat", "cpd", "cpd.bat"):
        if shutil.which(name):
            return name
    return None


def _collect_source_files(repo_root: Path) -> list[Path]:
    """Collect all .cpp and .h files under Dia/ and Cluiche/, excluding noise."""
    _EXCLUDES = {"External", ".venv", "bin", "out", ".claude"}
    files: list[Path] = []
    for root_dir in (repo_root / "Dia", repo_root / "Cluiche"):
        if not root_dir.exists():
            continue
        for f in root_dir.rglob("*"):
            if f.suffix not in (".cpp", ".h"):
                continue
            parts = set(f.relative_to(repo_root).parts)
            if parts & _EXCLUDES:
                continue
            files.append(f)
    return files


def _run_cpd(
    cpd_exe: str,
    source_files: list[Path],
    minimum_tokens: int,
    out_xml: Path,
) -> int:
    """Invoke CPD; returns subprocess returncode."""
    filelist_path = out_xml.parent / "cpd-filelist.txt"
    filelist_path.write_text(
        "\n".join(str(f) for f in source_files), encoding="utf-8"
    )

    # PMD 7.x: `pmd cpd --minimum-tokens=N ...`
    # Older standalone cpd.bat (pre-7): `cpd --minimum-tokens=N ...`
    exe_name = Path(cpd_exe).stem.lower().rstrip(".")
    is_pmd_launcher = exe_name in ("pmd",)
    cmd = [cpd_exe]
    if is_pmd_launcher:
        cmd.append("cpd")
    cmd += [
        f"--minimum-tokens={minimum_tokens}",
        "--language=cpp",
        "--format=xml",
        f"--filelist={filelist_path}",
        f"--reportfile={out_xml}",
    ]

    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode


def _parse_cpd_xml(xml_path: Path) -> list[dict]:
    """Parse CPD XML output into a list of duplication records."""
    if not xml_path.exists():
        return []
    try:
        root = ET.parse(str(xml_path)).getroot()
    except ET.ParseError:
        return []

    duplicates = []
    for dup in root.findall("duplication"):
        tokens = int(dup.get("tokens", "0"))
        lines = int(dup.get("lines", "0"))
        files = [
            {"path": f.get("path", ""), "line": int(f.get("line", "1"))}
            for f in dup.findall("file")
        ]
        duplicates.append({"tokens": tokens, "lines": lines, "files": files})
    return duplicates


def _make_signature(dup: dict) -> str:
    """Stable key for a duplication record (sorted file paths + line numbers)."""
    pairs = sorted(f"{f['path']}:{f['line']}" for f in dup["files"])
    return "|".join(pairs)


def _load_baseline(baseline_path: Path) -> set[str]:
    """Load baseline signatures from a previously saved XML file."""
    if not baseline_path.exists():
        return set()
    dups = _parse_cpd_xml(baseline_path)
    return {_make_signature(d) for d in dups}


def _cpd_to_sarif(duplicates: list[dict], repo_root: Path) -> dict:
    results = []
    for dup in duplicates:
        locations = []
        for f in dup["files"]:
            try:
                uri = Path(f["path"]).resolve().relative_to(repo_root.resolve())
                uri_str = uri.as_posix()
            except ValueError:
                uri_str = f["path"].replace("\\", "/")
            locations.append({
                "physicalLocation": {
                    "artifactLocation": {"uri": uri_str, "uriBaseId": "%SRCROOT%"},
                    "region": {"startLine": f["line"]},
                }
            })
        results.append({
            "ruleId": "cpd/duplicate-code",
            "level": "warning",
            "message": {
                "text": f"Duplicated block: {dup['lines']} lines / {dup['tokens']} tokens"
            },
            "locations": locations[:1],
            "relatedLocations": [
                {"id": i + 1, "physicalLocation": loc["physicalLocation"]}
                for i, loc in enumerate(locations[1:])
            ],
        })
    return {
        "version": "2.1.0",
        "$schema": (
            "https://raw.githubusercontent.com/oasis-tcs/sarif-spec/"
            "master/Schemata/sarif-schema-2.1.0.json"
        ),
        "runs": [{
            "tool": {"driver": {
                "name": "PMD CPD",
                "rules": [{"id": "cpd/duplicate-code", "name": "DuplicateCode",
                            "shortDescription": {"text": "Copy-paste duplication"}}],
            }},
            "results": results,
        }],
    }


def run_clone_check(
    repo_root: Path,
    minimum_tokens: int = 100,
    accept_baseline: bool = False,
) -> tuple[list[dict], int]:
    """Run CPD clone detection.

    Returns (new_duplicates, exit_code).
    exit_code 0 = clean (or baseline accepted), 1 = new duplicates found,
    2 = CPD not available.
    """
    out_dir = repo_root / "Cluiche" / "out" / "check"
    out_dir.mkdir(parents=True, exist_ok=True)
    xml_path = out_dir / "clones.xml"
    baseline_path = out_dir / "clones-baseline.xml"
    sarif_path = out_dir / "clones.sarif"

    cpd_exe = _find_cpd_executable(repo_root)
    if not cpd_exe:
        return [], 2

    source_files = _collect_source_files(repo_root)
    if not source_files:
        return [], 0

    _run_cpd(cpd_exe, source_files, minimum_tokens, xml_path)

    all_dups = _parse_cpd_xml(xml_path)

    if accept_baseline:
        shutil.copy2(str(xml_path), str(baseline_path))
        sarif_path.write_text(
            json.dumps(_cpd_to_sarif(all_dups, repo_root), indent=2), encoding="utf-8"
        )
        return [], 0

    if not baseline_path.exists():
        # No baseline yet — write SARIF for visibility but don't fail.
        # Run with --accept-baseline to start tracking new violations.
        sarif_path.write_text(
            json.dumps(_cpd_to_sarif(all_dups, repo_root), indent=2), encoding="utf-8"
        )
        return [], 0

    baseline_sigs = _load_baseline(baseline_path)
    new_dups = [d for d in all_dups if _make_signature(d) not in baseline_sigs]

    sarif_path.write_text(
        json.dumps(_cpd_to_sarif(new_dups, repo_root), indent=2), encoding="utf-8"
    )

    exit_code = 1 if new_dups else 0
    return new_dups, exit_code
