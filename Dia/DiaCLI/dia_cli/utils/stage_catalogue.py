"""Stage catalogue loader: reads a .diagame, its root .diaapp and imported .diastage files.

Mirrors the C++ loader; both must match Cluiche/Tests/Fixtures/StageCatalogue/expected_catalogue.json.
"""
import json
from dataclasses import dataclass, field
from pathlib import Path

OK = "Ok"
MISSING_DIAGAME = "MissingDiagame"
MALFORMED = "Malformed"
NO_ROOT_MANIFEST = "NoRootManifest"

MAX_STAGES = 64
BOOT_STAGE = "Boot"
GROUP_ORDER = ("showcase", "tool", "test", "other")
_KINDS_FILE = Path(__file__).resolve().parents[2].parent / "DiaGame" / "stage_kinds.json"


def _load_kinds():
    with open(_KINDS_FILE, encoding="utf-8") as f:
        return list(json.load(f)["kinds"])


@dataclass
class StageEntry:
    name: str
    tags: list = field(default_factory=list)
    kinds: list = field(default_factory=list)
    description: str = ""
    diastage_path: Path = None
    proves: list = field(default_factory=list)
    presets: list = field(default_factory=list)  # [{"name", "description"}]; override JSON is not carried


@dataclass
class StageCatalogue:
    entries: list = field(default_factory=list)
    omitted_count: int = 0
    dropped_count: int = 0
    imported: list = field(default_factory=list)  # every imported stage, listed in Boot or not

    def find(self, name):
        """Index of the entry with this name, or -1."""
        for i, e in enumerate(self.entries):
            if e.name == name:
                return i
        return -1

    def kinds(self, entry):
        return list(self.entries[entry].kinds)

    def filter(self, tags):
        """Indices of entries carrying every tag (exact, case-sensitive match)."""
        return [i for i, e in enumerate(self.entries) if all(t in e.tags for t in tags)]

    def group_by_kind(self):
        rows = []
        for group in GROUP_ORDER:
            for i, e in enumerate(self.entries):
                if group == "other":
                    if not e.kinds:
                        rows.append({"group": group, "entry": i})
                elif group in e.kinds:
                    rows.append({"group": group, "entry": i})
        return rows


def _read_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _resolve(base, rel):
    """Import paths are written for the deployed layout. In a source tree the file may sit elsewhere
    (CoW's cow_main.diaapp is flat), so fall back to a unique file-name match, like `dia validate manifest`."""
    direct = base / rel
    if direct.exists():
        return direct
    matches = list(base.rglob(Path(rel).name))
    return matches[0] if len(matches) == 1 else direct


def _read_presets(raw):
    """Names and descriptions only. Malformed entries are skipped, like the C++ composer (the stage still loads)."""
    if not isinstance(raw, list):
        return []
    out = []
    for p in raw:
        if isinstance(p, dict) and isinstance(p.get("name"), str) and p["name"]:
            out.append({"name": p["name"], "description": str(p.get("description", ""))})
    return out


def load(diagame_path):
    """Return (result, catalogue); catalogue is None unless result is OK."""
    diagame_path = Path(diagame_path)
    if not diagame_path.is_file():
        return MISSING_DIAGAME, None
    try:
        game = _read_json(diagame_path)
        imports = game.get("imports", [])
        base = diagame_path.parent
        manifest = next((i for i in imports if i.get("type") == "manifest"), None)
        if manifest is None:
            return NO_ROOT_MANIFEST, None
        app = _read_json(_resolve(base, manifest["path"]))
        boot = next((s for s in app.get("stages", []) if s.get("name") == BOOT_STAGE), None)
        transitions = list(boot.get("transitions", [])) if boot else []

        vocab = _load_kinds()
        by_name = {}
        for imp in imports:
            if imp.get("type") != "stage":
                continue
            path = _resolve(base, imp["path"])
            data = _read_json(path)
            tags = [str(t) for t in data.get("tags", [])]
            by_name[data["name"]] = StageEntry(
                name=data["name"], tags=tags,
                kinds=[k for k in vocab if k in tags],
                description=data.get("description", ""), diastage_path=path,
                proves=[str(p) for p in data.get("proves", [])],
                presets=_read_presets(data.get("presets")))
    except (OSError, ValueError, KeyError, TypeError, AttributeError):
        return MALFORMED, None

    cat = StageCatalogue(imported=list(by_name.values()))
    for name in transitions:
        entry = by_name.get(name, StageEntry(name=name))
        if len(cat.entries) >= MAX_STAGES:
            cat.dropped_count += 1
        else:
            cat.entries.append(entry)
    listed = set(transitions)
    cat.omitted_count = sum(1 for n in by_name if n not in listed)
    return OK, cat


def to_expected_json(result, catalogue, summary_only=False):
    """Shape used by expected_catalogue.json (summary_only for the over_cap case)."""
    out = {"result": result}
    if catalogue is None:
        return out
    if summary_only:
        out["entryCount"] = len(catalogue.entries)
        out["droppedCount"] = catalogue.dropped_count
        out["omittedCount"] = catalogue.omitted_count
        return out
    out["entries"] = [{"name": e.name, "tags": list(e.tags), "kinds": list(e.kinds),
                       "description": e.description} for e in catalogue.entries]
    out["rows"] = [{"group": r["group"], "entry": r["entry"]} for r in catalogue.group_by_kind()]
    out["omittedCount"] = catalogue.omitted_count
    out["droppedCount"] = catalogue.dropped_count
    return out
