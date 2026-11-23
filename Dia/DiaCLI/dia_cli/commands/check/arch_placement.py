"""arch_placement: PD-012 rule, library packages hold no application wiring.

Dia and Draoi packages are plain libraries. Wiring (module classes, processing-unit
affinity, module registration) is where games differ, so it lives in the application
or in CluicheGameBaseline. This catches the cause of a layer violation that would
otherwise only surface as "a service depends on the application layer": a module
class sitting inside a library package pins the whole package up to the application
layer.

The framework itself (DiaApplicationFlow) defines these concepts and is exempt, as
are Testing/ helpers.
"""
from __future__ import annotations

import re
from pathlib import Path

_FRAMEWORK = "Dia/DiaApplicationFlow"

_MODULE_BASES = ("SimModule", "RenderModule", "MainModule", "Module")

_CLASS_HEADER_RE = re.compile(r"\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?:\s*([^{;]*)\{")
_BASE_RE = re.compile(
    r"(?:^|[\s,:])(?:Dia::)?(?:ApplicationFlow::)?(" + "|".join(_MODULE_BASES) + r")\b"
)
_DIA_MODULE_RE = re.compile(r"\bDIA_MODULE\s*\(")
_AFFINITY_RE = re.compile(r"\b(?:PUAffinity|kAllowedPUs)\b")
_PU_INCLUDE_RE = re.compile(r'#\s*include\s+[<"]DiaApplicationFlow/ProcessingUnit\.h[>"]')

_COMMENT_RE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)

RULE = "library packages hold no application wiring (PD-012); move it to the application or CluicheGameBaseline"


def is_exempt(package_path: str, source: Path) -> bool:
    """True for the framework itself and for Testing/ helpers."""
    p = package_path.replace("\\", "/").rstrip("/")
    if p == _FRAMEWORK or p.startswith(_FRAMEWORK + "/"):
        return True
    return "Testing" in source.parts


def _blank_comments(text: str) -> str:
    """Replace comments with spaces, keeping newlines so line numbers still match."""
    return _COMMENT_RE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), text)


def find_wiring(text: str) -> list[tuple[int, str]]:
    """Return (line, symbol description) for each wiring use in one source file."""
    clean = _blank_comments(text)
    hits: list[tuple[int, str]] = []

    def line_of(offset: int) -> int:
        return clean.count("\n", 0, offset) + 1

    for m in _CLASS_HEADER_RE.finditer(clean):
        base = _BASE_RE.search(m.group(2))
        if base:
            hits.append((line_of(m.start()), f"class {m.group(1)} derives from {base.group(1)}"))
    registration = _DIA_MODULE_RE.search(clean)
    if registration:
        hits.append((line_of(registration.start()), "DIA_MODULE registration"))
    # Affinity appears many times per file (declaration, includes, uses); once is enough.
    affinity = _AFFINITY_RE.search(clean)
    if affinity:
        hits.append((line_of(affinity.start()), "processing-unit affinity"))
    pu = _PU_INCLUDE_RE.search(clean)
    if pu:
        hits.append((line_of(pu.start()), "reaches into a ProcessingUnit"))
    return hits
