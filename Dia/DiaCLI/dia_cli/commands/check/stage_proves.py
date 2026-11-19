"""Pure core of `dia check stage-proves`: stage `proves` links vs opt-in specs.

A spec opts in with a `**Testbed:** required` line in its header block (before the first `## `).
"""
import re
from dataclasses import dataclass, field
from pathlib import Path

from dia_cli.utils import stage_catalogue

TRACKED_STATUSES = ("Approved", "In Progress", "Done")

_MARKER_RE = re.compile(r"^\*\*Testbed:\*\*\s*(.*?)\s*$", re.IGNORECASE | re.MULTILINE)
_HEADER_STATUS_RE = re.compile(r"^\*\*Status:\*\*\s*(.+?)\s*$", re.MULTILINE)
_STATUS_SECTION_RE = re.compile(r"^##\s+Status\s*$(.*?)(?=^##\s|\Z)", re.MULTILINE | re.DOTALL)
_BACKTICK_RE = re.compile(r"`([^`]+)`")
_STATUS_WORDS = ("In Progress", "Approved", "Done", "Draft", "Blocked", "Superseded")


@dataclass
class BrokenLink:
    stage: str
    game: str
    path: str


@dataclass
class StageProvesResult:
    broken_links: list = field(default_factory=list)   # [BrokenLink]
    unproven: list = field(default_factory=list)       # [repo-relative spec path]
    coverage: dict = field(default_factory=dict)       # spec path -> [stage names]


def _header_block(text):
    m = re.search(r"^## ", text, re.MULTILINE)
    return text[:m.start()] if m else text


def parse_testbed_required(text):
    m = _MARKER_RE.search(_header_block(text))
    return bool(m) and m.group(1).strip().strip("`").strip().lower() == "required"


def _clean(value):
    value = value.replace("*", "").replace("`", "").strip()
    for w in _STATUS_WORDS:
        if value.lower().startswith(w.lower()):
            return w
    return value.split(" (")[0].strip() or "Unknown"


def parse_status(text):
    sec = _STATUS_SECTION_RE.search(text)
    if sec:
        m = _BACKTICK_RE.search(sec.group(1))
        if m:
            return _clean(m.group(1))
    m = _HEADER_STATUS_RE.search(_header_block(text))
    return _clean(m.group(1)) if m else "Unknown"


def _games(root, game=None):
    found = sorted((Path(root) / "Cluiche" / "Assets").rglob("*.diagame"))
    return [g for g in found if game is None or g.stem == game or g.parent.name == game]


def _load_stages(diagame):
    result, cat = stage_catalogue.load(diagame)
    return cat.imported if result == stage_catalogue.OK else []


def check(root, game=None):
    """Scan `root`. `game` limits broken-link reporting; coverage always spans every game."""
    root = Path(root)
    res = StageProvesResult()
    for gpath in _games(root):
        gname = gpath.parent.name
        for stage in _load_stages(gpath):
            for rel in stage.proves:
                res.coverage.setdefault(rel, []).append(stage.name)
                if (game is None or game in (gpath.stem, gname)) and not (root / rel).is_file():
                    res.broken_links.append(BrokenLink(stage.name, gname, rel))
    specs_dir = root / "docs" / "specs"
    if specs_dir.is_dir():
        for md in sorted(specs_dir.rglob("*.md")):
            if not md.is_file():
                continue
            text = md.read_text(encoding="utf-8", errors="replace")
            rel = md.relative_to(root).as_posix()
            if (parse_testbed_required(text) and parse_status(text) in TRACKED_STATUSES
                    and rel not in res.coverage):
                res.unproven.append(rel)
    return res
