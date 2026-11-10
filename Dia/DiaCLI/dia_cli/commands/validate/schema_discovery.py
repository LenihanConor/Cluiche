"""dia validate schema: root resolution, file discovery, pairing and the validation run."""
from __future__ import annotations

import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Set, Tuple

import click

from dia_cli.commands.validate.data_rules import (
    DataContext,
    build_schema_index,
    validate_data_document,
    validate_schema_references,
)
from dia_cli.commands.validate.schema_cmd import CheckedFiles, Finding
from dia_cli.commands.validate.schema_rules import validate_schema_text
from dia_cli.utils.repo_root import find_repo_root

ENVELOPE_KEYS = ("format", "version")
SKIP_DIRS = {"out", "node_modules", "bin", "obj", "__pycache__"}
MANIFEST_SUFFIXES = (".diagame", ".diaapp", ".diastage")
CONFORMANCE_REL = ("Dia", "DiaValueSchema", "Testing")


def get_repo_root() -> Path:
    return find_repo_root(__file__)


# ---------------------------------------------------------------------------
# Alias roots
# ---------------------------------------------------------------------------

def _assets_dir(repo_root: Path) -> Path:
    return repo_root / "Cluiche" / "Assets"


def _app_dirs(repo_root: Path) -> List[Path]:
    a = _assets_dir(repo_root)
    return sorted((p for p in a.iterdir() if p.is_dir()), key=lambda p: p.name.lower()) if a.is_dir() else []


def _pipeline_app_name(target: str, repo_root: Path) -> Optional[str]:
    from dia_cli.commands.pipeline.pipeline_config import PipelineConfigError, load_pipeline_config
    try:
        cfg = load_pipeline_config(repo_root)
    except PipelineConfigError:
        return None
    for name, t in cfg.targets.items():
        if name.lower() == target.lower():
            return t.app_name or name
    return None


def _manifest_alias_dirs(app_dir: Path) -> List[Path]:
    """Directories named by config.path_aliases in the app's .diagame/.diaapp/.diastage files."""
    out: List[Path] = []
    for manifest in sorted(p for p in app_dir.rglob("*") if p.is_file() and p.suffix in MANIFEST_SUFFIXES
                           and not _skip_dir(p.relative_to(app_dir).parts[:-1])):
        try:
            data = json.loads(manifest.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError, RecursionError):
            continue
        cfg = data.get("config") if isinstance(data, dict) else None
        aliases = cfg.get("path_aliases") if isinstance(cfg, dict) else None
        if not isinstance(aliases, dict):
            continue
        for value in aliases.values():
            if isinstance(value, str):
                d = (manifest.parent / value).resolve()
                if d.is_dir():
                    out.append(d)
    return out


def _under(path: Path, root: Path) -> bool:
    return path == root or root in path.parents


def target_roots(target: str, repo_root: Path) -> List[Path]:
    """Asset directory of a target plus any path-alias directories outside it.

    The target is a pipeline.toml target name (-> its app_name) or an app directory name
    under Cluiche/Assets. Unknown -> click.UsageError (exit 2).
    """
    app_name = _pipeline_app_name(target, repo_root) or target
    app_dir = next((d for d in _app_dirs(repo_root) if d.name.lower() == app_name.lower()), None)
    if app_dir is None:
        known = ", ".join(d.name for d in _app_dirs(repo_root)) or "none"
        raise click.UsageError(f"unknown --target '{target}' (no Cluiche/Assets/<App> directory; known apps: {known})")
    roots = [app_dir.resolve()]
    for d in _manifest_alias_dirs(app_dir):
        if not any(_under(d, r) for r in roots):
            roots = [r for r in roots if not _under(r, d)] + [d]
    return roots


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------

@dataclass
class _Unit:
    root: Path                      # scan root; names are relative to it
    files: Optional[List[Path]]     # explicit file list (PATHS file arguments) or None for a folder walk
    prefix: str = ""                # display prefix when several roots are scanned


def _is_catalogue(p: Path) -> bool:
    n = p.name.lower()
    return n.endswith(".catalogue.json")


def _skip_dir(rel_parts: Sequence[str]) -> bool:
    return any(part in SKIP_DIRS or part.startswith(".") for part in rel_parts)


def _walk(root: Path, exclude: Optional[Path]) -> List[Path]:
    out: List[Path] = []
    for p in sorted(root.rglob("*"), key=lambda q: q.as_posix().lower()):
        if not p.name.lower().endswith(".json") or not p.is_file() or _skip_dir(p.relative_to(root).parts[:-1]):
            continue
        if exclude is not None and _under(p.resolve(), exclude):
            continue
        out.append(p)
    return out


@dataclass
class _Scanned:
    path: Path
    name: str
    root: Path
    is_schema: bool
    is_pack: bool = False
    explicit: bool = False
    folders: Set[Path] = field(default_factory=set)


def _is_pack(path: Path, root: Path, explicit_file: bool) -> bool:
    """A file is a pack when a directory segment named `Packs` (any case) is above it.

    Folder scan: the segments below the scan root, plus the scan root folder itself.
    Explicit file: every directory segment of its path (there is no scan root to stop at).
    """
    if explicit_file:
        parts = list(path.parent.parts)
    else:
        parts = list(path.relative_to(root).parts[:-1]) + [root.name]
    return any(p.lower() == "packs" for p in parts)


def _schemad_folders(root: Path, files: List[Path]) -> Set[Path]:
    return {p.parent for p in files if p.name.lower().endswith(".schema.json")}


def _in_schemad_folder(path: Path, root: Path, folders: Set[Path]) -> bool:
    d = path.parent
    while True:
        if d in folders:
            return True
        if d == root or root not in d.parents:
            return False
        d = d.parent


def count_keys(doc: object) -> int:
    keys: Set[str] = set()

    def walk(node: object, prefix: str, top: bool) -> None:
        if isinstance(node, dict):
            for k, v in node.items():
                if top and k in ENVELOPE_KEYS:
                    continue
                if isinstance(v, dict):
                    walk(v, f"{prefix}{k}.", False)
                else:
                    walk_list_or_leaf(v, f"{prefix}{k}")

    def walk_list_or_leaf(v: object, key: str) -> None:
        if isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
            for rec in v:
                walk(rec, key + "[].", False)
        else:
            keys.add(key)

    walk(doc, "", True)
    return len(keys)


def run_scan(units: List[_Unit], exclude: Optional[Path] = None) -> Tuple[List[Finding], CheckedFiles]:
    findings: List[Finding] = []
    checked: CheckedFiles = []
    seen: Set[Path] = set()
    scanned: List[_Scanned] = []

    # Folders first (shallowest first), then explicit files: a file reachable both ways is always named
    # relative to its folder, whatever order PATHS were given in.
    units = sorted(units, key=lambda u: (u.files is not None, len(u.root.parts), str(u.root).lower(), str(u.root),
                                         [str(f) for f in (u.files or [])]))
    for unit in units:
        explicit = unit.files is not None
        files = unit.files if explicit else _walk(unit.root, exclude)
        folders = _schemad_folders(unit.root, files)
        for p in files:
            rp = p.resolve()
            if rp in seen:
                continue
            seen.add(rp)
            is_schema = p.name.lower().endswith(".schema.json")
            if not is_schema and _is_catalogue(p):
                continue
            name = unit.prefix + (p.name if explicit else p.relative_to(unit.root).as_posix())
            scanned.append(_Scanned(p, name, unit.root, is_schema, _is_pack(p, unit.root, explicit),
                                    explicit, folders))

    scanned.sort(key=lambda s: (s.name.lower(), s.name))

    # --- schemas
    ok_schemas: List[Tuple[_Scanned, dict]] = []
    for s in scanned:
        if not s.is_schema:
            continue
        try:
            text = s.path.read_text(encoding="utf-8-sig")
        except (OSError, UnicodeError) as ex:
            findings.append(Finding("FAIL", "schema.unreadable", s.name, "", f"cannot read file: {ex}"))
            checked.append((s.name, 0))
            continue
        try:
            problems = validate_schema_text(text, s.name)
        except RecursionError:
            problems = [Finding("FAIL", "schema.unparsable-json", s.name, "", "unparsable JSON: nested too deeply")]
        fails = [f for f in problems if f.severity == "FAIL"]
        findings.extend(problems)
        if fails:
            checked.append((s.name, 0))
            continue
        doc = json.loads(text)
        ok_schemas.append((s, doc))
        checked.append((s.name, len(doc["entries"])))

    # duplicate full key across schema files (first by sorted path wins)
    first_by_key: Dict[str, _Scanned] = {}
    usable: List[Tuple[_Scanned, dict]] = []
    file_by_doc: Dict[int, str] = {}
    for s, doc in ok_schemas:
        for e in doc["entries"]:
            full = f"{doc['for']}/{e['key']}"
            other = first_by_key.get(full)
            if other is None:
                first_by_key[full] = s
            elif other is not s:
                findings.append(Finding("FAIL", "schema.duplicate-key-across-files", s.name, e["key"],
                                        f"duplicate key '{full}' also defined in {other.name} (this file: {s.name})"))
        usable.append((s, doc))
        file_by_doc[id(doc)] = s.name
    index = build_schema_index(d for _, d in usable)
    findings.extend(validate_schema_references((d for _, d in usable), file_by_doc))

    # --- data candidates
    data_docs: List[Tuple[_Scanned, dict]] = []
    id_sources: List[Tuple[_Scanned, dict]] = []
    for s in scanned:
        if s.is_schema:
            continue
        in_schemad = (not s.explicit and _in_schemad_folder(s.path, s.root, s.folders)) or \
            (s.explicit and s.path.parent in s.folders)
        try:
            doc = json.loads(s.path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError, RecursionError) as ex:
            if in_schemad:
                findings.append(Finding("FAIL", "data.invalid-json", s.name, "", f"unparsable JSON: {ex}"))
                checked.append((s.name, 0))
            continue
        fmt = doc.get("format") if isinstance(doc, dict) else None
        if not isinstance(fmt, str):
            if in_schemad:
                findings.append(Finding("FAIL", "data.no-format", s.name, "",
                                        "no `format`; cannot find its schema"))
                checked.append((s.name, 0))
            continue
        id_sources.append((s, doc))  # every data document with a format is an id source for cross-format refs
        if not index.has_format(fmt):
            findings.append(Finding("WARN", "data.no-schema-for-format", s.name, None,
                                    f"format `{fmt}` has no schema ({s.name})"))
            continue
        data_docs.append((s, doc))

    context = DataContext.from_files((s.name, d, s.is_pack) for s, d in id_sources)
    for s, doc in data_docs:
        try:
            findings.extend(validate_data_document(doc, s.name, index, is_pack=s.is_pack, context=context))
            checked.append((s.name, count_keys(doc)))
        except RecursionError:
            findings.append(Finding("FAIL", "data.invalid-json", s.name, "", "unparsable JSON: nested too deeply"))
            checked.append((s.name, 0))

    findings.sort(key=lambda f: (f.file.lower(), f.file, 0 if f.severity == "FAIL" else 1, f.rule, f.key or "",
                                 f.path or "", f.message))
    checked.sort(key=lambda c: (c[0].lower(), c[0], c[1]))
    return findings, checked


# ---------------------------------------------------------------------------
# Entry point used by schema_cmd.run_validation
# ---------------------------------------------------------------------------

def discover_and_validate(paths: Sequence[Path], target: Optional[str]) -> Tuple[List[Finding], CheckedFiles]:
    repo_root: Optional[Path] = None
    roots: List[Path] = []
    if target is not None or not paths:
        repo_root = get_repo_root()
    if target is not None:
        roots = target_roots(target, repo_root)  # validates the target even when PATHS override
    exclude = (repo_root / Path(*CONFORMANCE_REL)).resolve() if repo_root else None

    units: List[_Unit] = []
    if paths:
        exclude = None
        for p in paths:
            if p.is_dir():
                units.append(_Unit(p.resolve(), None))
            else:
                units.append(_Unit(p.resolve().parent, [p.resolve()]))
    else:
        if not roots:
            roots = [d.resolve() for d in _app_dirs(repo_root)]  # default: every app's asset root
        multi = len(roots) > 1
        units = [_Unit(r, None, r.name + "/" if multi else "") for r in roots]
    return run_scan(units, exclude)
