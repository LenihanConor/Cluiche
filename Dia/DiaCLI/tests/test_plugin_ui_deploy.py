"""Deploy-presence check: every editor plugin UI URL has a matching deploy entry in pipeline.toml.

A plugin loads its UI from ``dia://plugins/<name>/<path>``.  If ``pipeline.toml``'s cluicheeditor deploy
has no ``files`` entry landing under ``plugins/<name>/`` (and, when the URL goes through ``dist/``, under
``plugins/<name>/dist/``), the plugin opens to a blank page at runtime and nothing fails at build time.
Also checks that a deploy source built from ``UI/dist`` has its build in ``ui_builds``.
"""
from __future__ import annotations

import re
from pathlib import Path

import pytest
import toml

REPO = Path(__file__).resolve().parents[3]
URL_RE = re.compile(r'dia://plugins/([A-Za-z0-9_]+)/([A-Za-z0-9_./-]+)')

# Plugins that reference a UI which does not exist in the repo yet (needs a UI built, not a pipeline fix).
# Remove an entry when its UI lands; the test then enforces the deploy entry.
KNOWN_MISSING_UI = {"aiinspector": "DiaAIInspector has no UI/ folder"}


def _deploy():
    cfg = toml.load(str(REPO / "pipeline.toml"))
    return cfg["targets"]["cluicheeditor"]["deploy"]


def _plugin_urls():
    found = {}
    for path in (REPO / "Dia").rglob("*Plugin*.[hc]*"):
        if path.suffix not in (".h", ".cpp"):
            continue
        for name, rel in URL_RE.findall(path.read_text(encoding="utf-8", errors="ignore")):
            found.setdefault(name, set()).add(rel)
    return found


def _dests(files):
    out = [f["dest"].replace("\\", "/").lower() for f in files]
    # A recursive glob that deploys straight into plugins/ provides one folder per source subfolder
    # (Dia/DiaEditor/Plugin/Assets/** -> plugins/hello/, plugins/home/, ...).
    for f in files:
        src = f["src"].replace("\\", "/")
        dest = f["dest"].replace("\\", "/").lower()
        if dest.endswith("plugins/") and "/**" in src:
            root = REPO / src.split("/**")[0]
            if root.is_dir():
                for p in root.rglob("*"):
                    if p.is_file():
                        out.append(dest + p.relative_to(root).as_posix().lower())
    return out


def test_pipeline_has_urls_to_check():
    assert _plugin_urls(), "no dia://plugins/ URLs found; the scan is broken"


@pytest.mark.parametrize("name,rels", sorted(_plugin_urls().items()))
def test_plugin_url_has_deploy_entry(name, rels):
    if name in KNOWN_MISSING_UI:
        pytest.skip(KNOWN_MISSING_UI[name])
    dests = _dests(_deploy()["files"])
    for rel in rels:
        folder = f"plugins/{name.lower()}/"
        # The URL's directory part (e.g. "dist/") must be a prefix of some dest.
        sub = rel.rsplit("/", 1)[0] + "/" if "/" in rel else ""
        wanted = folder + sub.lower()
        assert any(d.endswith(wanted) or wanted in d for d in dests), (
            f"plugin '{name}' loads dia://plugins/{name}/{rel} but pipeline.toml has no deploy dest containing '{wanted}'"
        )


def test_dist_sources_have_a_ui_build():
    deploy = _deploy()
    builds = [b["cwd"].replace("\\", "/") for b in deploy["ui_builds"]]
    for f in deploy["files"]:
        src = f["src"].replace("\\", "/")
        m = re.match(r"(.+/UI)/dist/", src)
        if m:
            assert m.group(1) in builds, f"{src} is deployed but {m.group(1)} is not in ui_builds"
