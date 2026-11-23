import json
import re
from pathlib import Path

from dia_cli.commands.check.arch_checker import run_arch_check
from dia_cli.commands.check.arch_tower import build_tower_data, render_tower_html, write_tower_html


def _module(repo: Path, root: str, name: str, module_id: str, layer: str, source: str = "") -> None:
    d = repo / root / name
    d.mkdir(parents=True, exist_ok=True)
    (d / f"dia.{module_id.split('.', 1)[1]}.architecture.module.md").write_text(
        f"---\nschema: dia.module.v1\nmodule_id: {module_id}\nname: {name}\n"
        f"path: {root}/{name}\nlayer: {layer}\n---\n",
        encoding="utf-8",
    )
    if source:
        (d / "A.cpp").write_text(source, encoding="utf-8")


def _repo_with_upward_include(tmp_path: Path) -> Path:
    _module(tmp_path, "Dia", "DiaLow", "dia.low", "foundation/core", source="#include <DiaHigh/X.h>\n")
    _module(tmp_path, "Dia", "DiaHigh", "dia.high", "domain/visual/core")
    return tmp_path


def test_data_contains_layers_edges_and_violations(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    violations, _, _ = run_arch_check(repo)
    data = build_tower_data(repo, violations)
    assert data["layers"] == {"dia.low": "foundation/core", "dia.high": "domain/visual/core"}
    assert [(e["f"], e["t"], e["n"]) for e in data["edges"]] == [("dia.low", "dia.high", 1)]
    assert [(v["f"], v["t"], v["line"]) for v in data["violations"]] == [("dia.low", "dia.high", 1)]
    assert data["other"] == []


def test_non_layer_violations_are_listed_not_drawn(tmp_path):
    _module(tmp_path, "Draoi", "DraoiX", "draoi.x", "draoi")
    _module(tmp_path, "Dia", "DiaA", "dia.a", "foundation/core", source="#include <DraoiX/X.h>\n")
    violations, _, _ = run_arch_check(tmp_path)
    data = build_tower_data(tmp_path, violations)
    assert [o["kind"] for o in data["other"]] == ["boundary"]
    assert data["violations"] == []


def test_render_fills_placeholders_and_survives_script_tags(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    data = build_tower_data(repo, run_arch_check(repo)[0])
    data["other"].append({"f": "a", "t": "b", "kind": "x", "rule": "</script><b>", "file": "f", "line": 1, "inc": "i"})
    html = render_tower_html(data)
    assert "/*DATA*/" not in html and "/*GENERATED*/" not in html
    payload = re.search(r"const DATA=(\{.*?\});\nconst LANES", html, re.S).group(1)
    assert json.loads(payload)["other"][0]["rule"] == "</script><b>"


def test_write_creates_parent_dirs(tmp_path):
    repo = _repo_with_upward_include(tmp_path)
    out = write_tower_html(repo, run_arch_check(repo)[0], tmp_path / "out" / "check" / "tower.html")
    assert out.read_text(encoding="utf-8").startswith("<!DOCTYPE html>")


# ---------------------------------------------------------------------------
# Browser interaction test: drives the real page in headless Edge/Chrome.
# ---------------------------------------------------------------------------
import shutil
import subprocess

import pytest

_BROWSERS = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
]

_DRIVER = r"""
<script>
(function(){
  const out = {};
  const fire = (t, type, x, y) => t.dispatchEvent(new MouseEvent(type, {bubbles: true, clientX: x, clientY: y}));
  const center = el => { const r = el.getBoundingClientRect(); return [r.left + r.width / 2, r.top + r.height / 2]; };
  const box = id => document.querySelector('.box[data-id="' + id + '"]');
  const click = id => { const [x, y] = center(box(id)); fire(box(id), 'mousedown', x, y); fire(document, 'mouseup', x, y); };
  try {
    out.leaks0 = document.getElementById('count').textContent;
    click('dia.low');
    out.panel = document.getElementById('modpanel').textContent;
    document.getElementById('clearMod').click();
    out.cycleCards = document.querySelectorAll('#cycles .card').length;
    document.getElementById('vFull').click();
    out.fullBoxes = document.querySelectorAll('.box').length;
    const f = document.getElementById('filter'); f.value = 'high'; f.dispatchEvent(new Event('input'));
    out.filtered = [...document.querySelectorAll('.box')].map(b => b.dataset.id);
    f.value = '('; f.dispatchEvent(new Event('input'));
    out.badRegexMarked = f.className === 'err';
    f.value = ''; f.dispatchEvent(new Event('input'));
    document.getElementById('vLeaks').click();
    const [sx, sy] = center(box('dia.low'));
    const lane = document.querySelector('.lane[data-layer="domain/visual/core"]');
    const [lx, ly] = center(lane);
    fire(box('dia.low'), 'mousedown', sx, sy); fire(document, 'mousemove', lx, ly); fire(document, 'mouseup', lx, ly);
    out.leaks1 = document.getElementById('count').textContent;
    out.fixedCards = document.querySelectorAll('.card.fixed').length;
    const n = document.getElementById('notes'); n.value = 'move X.h'; n.dispatchEvent(new Event('input'));
    out.proposal = document.getElementById('proposal').value;
  } catch (e) { out.error = String(e); }
  document.body.insertAdjacentHTML('beforeend', '<pre id="result">' + JSON.stringify(out) + '</pre>');
})();
</script>
"""


def _browser():
    for p in _BROWSERS:
        if Path(p).exists():
            return p
    return shutil.which("msedge") or shutil.which("chrome")


@pytest.mark.skipif(_browser() is None, reason="needs Edge or Chrome")
def test_page_interactions_in_a_real_browser(tmp_path):
    _module(tmp_path, "Dia", "DiaLow", "dia.low", "foundation/core", source="#include <DiaHigh/X.h>\n")
    _module(tmp_path, "Dia", "DiaHigh", "dia.high", "domain/visual/core")
    _module(tmp_path, "Dia", "DiaCycA", "dia.cyca", "foundation/core", source="#include <DiaCycB/X.h>\n")
    _module(tmp_path, "Dia", "DiaCycB", "dia.cycb", "foundation/core", source="#include <DiaCycA/X.h>\n")
    html = render_tower_html(build_tower_data(tmp_path, run_arch_check(tmp_path)[0]))
    page = tmp_path / "tower.html"
    page.write_text(html.replace("</body>", _DRIVER + "</body>"), encoding="utf-8")

    dom = subprocess.run(
        [_browser(), "--headless=new", "--disable-gpu", f"--user-data-dir={tmp_path / 'profile'}",
         "--window-size=1500,900", "--virtual-time-budget=5000", "--dump-dom", page.as_uri()],
        capture_output=True, text=True, timeout=60,
    ).stdout
    result = json.loads(re.findall(r'<pre id="result">(.*?)</pre>', dom, re.S)[-1].replace("&quot;", '"').replace("&gt;", ">").replace("&lt;", "<").replace("&amp;", "&"))

    assert "error" not in result, result
    assert result["leaks0"] == "1 leak"
    assert "Uses (1)" in result["panel"] and "high" in result["panel"]
    assert result["cycleCards"] == 1
    assert result["fullBoxes"] == 4
    assert result["filtered"] == ["dia.high"]
    assert result["badRegexMarked"] is True
    assert result["leaks1"] == "0 leaks"
    assert result["fixedCards"] == 1
    assert "- dia.low: `foundation/core` -> `domain/visual/core`" in result["proposal"]
    assert "Leaks: 1 -> 0" in result["proposal"] and "move X.h" in result["proposal"]
    assert "DiaLow" in result["proposal"]  # points at the module doc to edit
