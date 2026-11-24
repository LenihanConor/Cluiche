"""Pure logic behind `dia stages`: merge catalogue + results, filter, shape JSON, render tables."""
import json
from pathlib import Path

from dia_cli.utils import stage_catalogue as sc

RESULTS_FORMAT = "dia.stage_results"
RESULTS_VERSION = 1
KIND_OTHER = "other"


def valid_kinds():
    return list(sc._load_kinds()) + [KIND_OTHER]


def load_results(path, warn=None):
    """Return {stage: result dict}. Missing/malformed/wrong-format/wrong-version -> {} (never raises).

    `warn(msg)` is called for malformed or wrong-version files (not for a missing file).
    """
    path = Path(path)
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if not isinstance(data, dict) or data.get("format") != RESULTS_FORMAT:
            raise ValueError("wrong format")
    except (OSError, ValueError):
        if warn:
            warn(f"stage results ignored: {path} is malformed")
        return {}
    if data.get("version") != RESULTS_VERSION:
        if warn:
            warn(f"stage results ignored: {path} has unsupported version {data.get('version')}")
        return {}
    results = data.get("results")
    if not isinstance(results, dict):
        return {}
    return {k: v for k, v in results.items() if isinstance(v, dict)}


def entry_dict(entry, results, full=False):
    d = {"name": entry.name, "kinds": list(entry.kinds), "tags": list(entry.tags),
         "description": entry.description,
         "presets": [dict(p) for p in entry.presets],
         "last_result": results.get(entry.name)}
    if full:
        d["proves"] = []
    return d


def select(catalogue, tags=(), kind=None):
    """Entries matching all tags and (optionally) kind; kind 'other' means no kind."""
    out = []
    for i in catalogue.filter(list(tags)):
        e = catalogue.entries[i]
        if kind == KIND_OTHER and e.kinds:
            continue
        if kind and kind != KIND_OTHER and kind not in e.kinds:
            continue
        out.append(e)
    return out


def list_entries(catalogue, results, tags=(), kind=None):
    return [entry_dict(e, results) for e in select(catalogue, tags, kind)]


def get_entry(catalogue, results, name):
    """Full entry dict, or None for an unknown stage."""
    i = catalogue.find(name)
    return None if i < 0 else entry_dict(catalogue.entries[i], results, full=True)


def _trunc(s, n):
    s = " ".join(str(s).split())
    return s if len(s) <= n else s[: n - 3] + "..."


def render_table(entries, desc_width=50):
    headers = ["Stage", "Kinds", "Tags", "Result", "Description"]
    rows = [[e["name"], ",".join(e["kinds"]), ",".join(e["tags"]),
             (e["last_result"] or {}).get("label", ""), _trunc(e["description"], desc_width)]
            for e in entries]
    widths = [max(len(r[i]) for r in [headers] + rows) for i in range(len(headers))]
    lines = ["  ".join(c.ljust(w) for c, w in zip(r, widths)).rstrip() for r in [headers] + rows]
    return "\n".join(lines)


def render_detail(e):
    lines = [f"Stage:       {e['name']}",
             f"Kinds:       {', '.join(e['kinds'])}",
             f"Tags:        {', '.join(e['tags'])}",
             f"Description: {e['description']}",
             f"Presets:     {', '.join(p['name'] for p in e['presets'])}",
             f"Proves:      {', '.join(e['proves'])}"]
    r = e["last_result"]
    lines.append("Result:      " + (f"{r.get('label', '')} ({r.get('severity', '')})" if r else ""))
    return "\n".join(lines)
