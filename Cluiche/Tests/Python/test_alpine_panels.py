"""
Alpine panel structural validation tests.

For each .html file under Cluiche/CluicheTest/AlpinePanels/ (excluding template.html),
verify:
  1. data-theme attribute is present on <html>
  2. window.GameBridge_dispatch is referenced in the file
  3. All External/ href/src paths resolve to real files on disk
  4. Alpine, Tailwind, and DaisyUI vendor scripts are loaded
  5. No CDN URLs (cdn.jsdelivr.net, unpkg.com, cdn.tailwindcss.com) remain

Run from repo root: python -m pytest Cluiche/Tests/Python/test_alpine_panels.py -v
"""
import os
import re
from pathlib import Path
from html.parser import HTMLParser

import pytest

REPO_ROOT = Path(__file__).resolve().parents[3]
PANELS_DIR = REPO_ROOT / "Cluiche" / "CluicheTest" / "AlpinePanels"

CDN_PATTERNS = [
    "cdn.jsdelivr.net",
    "unpkg.com",
    "cdn.tailwindcss.com",
    "cdnjs.cloudflare.com",
]

REQUIRED_VENDORS = [
    "alpine.min.js",
    "tailwind.min.js",
    "daisyui.min.css",
]


class ExternalRefCollector(HTMLParser):
    """Collect all src= and href= attribute values from script/link tags."""

    def __init__(self):
        super().__init__()
        self.refs: list[str] = []
        self.has_data_theme: bool = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]):
        attr_dict = dict(attrs)
        if tag == "html" and "data-theme" in attr_dict:
            self.has_data_theme = True
        if tag in ("script", "link"):
            for key in ("src", "href"):
                val = attr_dict.get(key)
                if val:
                    self.refs.append(val)


def _panel_files() -> list[Path]:
    return [
        p for p in PANELS_DIR.glob("*.html")
        if p.name != "template.html"
    ]


def _panel_ids() -> list[str]:
    return [p.name for p in _panel_files()]


@pytest.fixture(scope="module", params=_panel_files(), ids=_panel_ids())
def panel(request) -> Path:
    return request.param


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

def test_data_theme_present(panel: Path):
    parser = ExternalRefCollector()
    parser.feed(panel.read_text(encoding="utf-8"))
    assert parser.has_data_theme, f"{panel.name}: <html> is missing data-theme attribute"


def test_gamebridge_dispatch_referenced(panel: Path):
    content = panel.read_text(encoding="utf-8")
    assert "window.GameBridge_dispatch" in content, (
        f"{panel.name}: window.GameBridge_dispatch not found — panel cannot receive data from C++"
    )


def test_no_cdn_urls(panel: Path):
    content = panel.read_text(encoding="utf-8")
    for cdn in CDN_PATTERNS:
        assert cdn not in content, (
            f"{panel.name}: CDN URL '{cdn}' found — all assets must be vendored via External/"
        )


def test_vendor_scripts_loaded(panel: Path):
    content = panel.read_text(encoding="utf-8")
    for vendor in REQUIRED_VENDORS:
        assert vendor in content, (
            f"{panel.name}: required vendor file '{vendor}' not referenced"
        )


def test_external_refs_resolve(panel: Path):
    """All src/href paths that reference External/ must resolve to real files."""
    parser = ExternalRefCollector()
    parser.feed(panel.read_text(encoding="utf-8"))

    panel_dir = panel.parent
    missing = []
    for ref in parser.refs:
        if "External/" not in ref:
            continue
        # Resolve relative to the panel file's directory
        resolved = (panel_dir / ref).resolve()
        if not resolved.exists():
            missing.append(f"  {ref!r} → {resolved}")

    assert not missing, (
        f"{panel.name}: the following External/ references do not resolve to real files:\n"
        + "\n".join(missing)
    )


def test_all_panels_accounted_for():
    """Ensures no panel file was silently skipped (glob found at least the 8 widgets)."""
    panels = _panel_files()
    assert len(panels) >= 8, (
        f"Expected at least 8 widget panels in {PANELS_DIR}, found {len(panels)}: "
        + str([p.name for p in panels])
    )
