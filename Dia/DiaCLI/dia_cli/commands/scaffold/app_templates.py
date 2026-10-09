"""Template registry for ``dia scaffold app`` (scaffold-app spec, task 2).

The Jinja2 templates live in ``dia_cli/templates/app/``. This module is the
machine-readable list the command imports: which template renders to which
output path, and how the render context is built.

Render context variables (every template uses only these; render with
``StrictUndefined`` so a missing one fails loudly):

    name          PascalCase application name, e.g. ``ScratchApp``, ``CoW``.
                  Used for the project, exe, C++ namespace, asset folder.
    lower         ``name.lower()``, e.g. ``scratchapp``, ``cow``. Used for the
                  pipeline target id and the .diagame/.diaapp/.diaobservation files.
    stage         PascalCase stage name (``--stage``, default ``Placeholder``).
                  The app-flow stage id and the ``<stage>Module`` C++ type.
    stage_snake   ``stage`` in snake_case using the AssetServiceModule
                  algorithm (``FrontEnd`` -> ``front_end``). The catalogue stage
                  id must be ``stage.<stage_snake>`` for AssetServiceModule to
                  find the stage's assets, so the files use it too.
    project_guid  ``{XXXXXXXX-...}`` ProjectGuid for the .vcxproj (and the
                  solution entry written by the command).
    references    List of project references (dicts with ``include``,
                  ``project``, ``extra``, ``link_lib``), see
                  ``baseline_references``. Not copied from CluicheTest: it is
                  read from CluicheGameBaseline.vcxproj so a growing baseline
                  does not leave the template behind (spec open question 1).

Output path patterns use ``str.format`` with the same context keys.
"""
from __future__ import annotations

import re
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional
from xml.sax.saxutils import escape, quoteattr

TEMPLATE_DIR = Path(__file__).resolve().parents[2] / "templates" / "app"

# (template file in TEMPLATE_DIR, output path relative to the repo root)
TEMPLATES: List[tuple] = [
    ("vcxproj.j2",               "Cluiche/{name}/{name}.vcxproj"),
    ("vcxproj.filters.j2",       "Cluiche/{name}/{name}.vcxproj.filters"),
    ("Main.cpp.j2",              "Cluiche/{name}/Main.cpp"),
    ("StageModule.h.j2",         "Cluiche/{name}/Modules/{stage}Module.h"),
    ("StageModule.cpp.j2",       "Cluiche/{name}/Modules/{stage}Module.cpp"),
    ("app.diagame.j2",           "Cluiche/Assets/{name}/{lower}.diagame"),
    ("main.diaapp.j2",           "Cluiche/Assets/{name}/{lower}_main.diaapp"),
    ("app.diaobservation.j2",    "Cluiche/Assets/{name}/{lower}.diaobservation"),
    ("assets.catalogue.json.j2", "Cluiche/Assets/{name}/assets.catalogue.json"),
    ("global.stage.json.j2",     "Cluiche/Assets/{name}/global.stage.json"),
    ("stage.diastage.j2",        "Cluiche/Assets/{name}/Stages/{stage}/{stage_snake}.diastage"),
    ("stage.diaapp.j2",          "Cluiche/Assets/{name}/Stages/{stage}/misc/ApplicationFlow/{stage_snake}.diaapp"),
]

# Rendered and appended to pipeline.toml (not a standalone file).
TOML_BLOCK_TEMPLATE = "pipeline_target.toml.j2"

DEFAULT_STAGE = "Placeholder"

BASELINE_VCXPROJ = Path("Cluiche") / "CluicheGameBaseline" / "CluicheGameBaseline.vcxproj"
_BASELINE_INCLUDE = r"..\CluicheGameBaseline\CluicheGameBaseline.vcxproj"
_MSBUILD_NS = "{http://schemas.microsoft.com/developer/msbuild/2003}"

# Fixed namespace so the default ProjectGuid is stable for a given app name.
_GUID_NAMESPACE = uuid.UUID("6f1c2d7e-3a54-4b8e-9c0d-5e2f71a9b3c4")


def pascal_to_snake(name: str) -> str:
    """PascalCase -> snake_case, matching AssetServiceModule::AssetStageIdFromAppStage."""
    out = []
    for i, ch in enumerate(name):
        if ch.isupper():
            if i > 0:
                prev = name[i - 1]
                nxt = name[i + 1] if i + 1 < len(name) else ""
                if prev.islower() or (prev.isupper() and nxt.islower()):
                    out.append("_")
            out.append(ch.lower())
        else:
            out.append(ch)
    return "".join(out)


def default_project_guid(name: str) -> str:
    """Deterministic ProjectGuid for an app name, in vcxproj form ``{UPPER-CASE}``."""
    return "{" + str(uuid.uuid5(_GUID_NAMESPACE, f"cluiche.app.{name}")).upper() + "}"


def baseline_references(repo_root: Path) -> List[Dict]:
    """Project references for a generated app: CluicheGameBaseline plus every
    project the baseline itself references.

    The app links the baseline with /WHOLEARCHIVE (every DIA_MODULE must
    register), so the app needs exactly what the baseline needs. Paths are
    copied unchanged: the app folder is a sibling of CluicheGameBaseline, so
    ``..\\..\\Dia\\X\\X.vcxproj`` resolves the same from both.
    """
    vcxproj = Path(repo_root) / BASELINE_VCXPROJ
    root = ET.parse(vcxproj).getroot()

    guid_el = root.find(f".//{_MSBUILD_NS}ProjectGuid")
    if guid_el is None or not (guid_el.text or "").strip():
        raise ValueError(f"No ProjectGuid in {vcxproj}")

    refs: List[Dict] = [{
        "include": _BASELINE_INCLUDE,
        "project": guid_el.text.strip(),
        "extra": [],
        "link_lib": "CluicheGameBaseline",
    }]

    for ref in root.iter(f"{_MSBUILD_NS}ProjectReference"):
        include = ref.get("Include", "")
        project = ""
        extra: List[str] = []
        output_referenced = True
        for child in ref:
            tag = child.tag.replace(_MSBUILD_NS, "")
            text = (child.text or "").strip()
            if tag == "Project":
                project = text
                continue
            attrs = "".join(f" {k}={quoteattr(v)}" for k, v in child.attrib.items())
            extra.append(f"<{tag}{attrs}>{escape(text)}</{tag}>")
            if tag == "ReferenceOutputAssembly" and not child.attrib and text.lower() == "false":
                output_referenced = False
        stem = re.split(r"[\\/]", include)[-1].rsplit(".", 1)[0]
        refs.append({
            "include": include,
            "project": project,
            "extra": extra,
            "link_lib": stem if output_referenced else None,
        })

    # The baseline's sources may include Dia libraries it never declares a
    # ProjectReference to (a static lib does not need one to compile). The app
    # links the baseline whole-archive, so those libraries must be linked too.
    known = {re.split(r"[\\/]", r["include"])[-1].rsplit(".", 1)[0] for r in refs}
    baseline_dir = vcxproj.parent
    included = set()
    for src in list(baseline_dir.rglob("*.cpp")) + list(baseline_dir.rglob("*.h")):
        included.update(re.findall(r"#\s*include\s*<(Dia\w+)/", src.read_text(encoding="utf-8", errors="ignore")))
    for lib in sorted(included - known):
        lib_proj = Path(repo_root) / "Dia" / lib / f"{lib}.vcxproj"
        if not lib_proj.exists():
            continue
        lib_guid = ET.parse(lib_proj).getroot().find(f".//{_MSBUILD_NS}ProjectGuid")
        if lib_guid is None or not (lib_guid.text or "").strip():
            continue
        refs.append({
            "include": f"..\\..\\Dia\\{lib}\\{lib}.vcxproj",
            "project": lib_guid.text.strip(),
            "extra": [],
            "link_lib": lib,
        })
    return refs


def render_context(
    name: str,
    references: List[Dict],
    stage: str = DEFAULT_STAGE,
    project_guid: Optional[str] = None,
) -> Dict:
    """Build the full render context. ``references`` comes from ``baseline_references``."""
    return {
        "name": name,
        "lower": name.lower(),
        "stage": stage,
        "stage_snake": pascal_to_snake(stage),
        "project_guid": project_guid or default_project_guid(name),
        "references": references,
    }


def make_environment():
    """Jinja2 environment for the app templates (strict, LF, no autoescape)."""
    from jinja2 import Environment, FileSystemLoader, StrictUndefined

    return Environment(
        loader=FileSystemLoader(str(TEMPLATE_DIR)),
        undefined=StrictUndefined,
        keep_trailing_newline=True,
        trim_blocks=True,
        lstrip_blocks=True,
        autoescape=False,
    )


def render_all(context: Dict) -> Dict[str, str]:
    """Render every file template. Returns {repo-relative output path: content}."""
    env = make_environment()
    return {
        out.format(**context): env.get_template(tpl).render(**context)
        for tpl, out in TEMPLATES
    }


def render_toml_block(context: Dict) -> str:
    """Render the ``[targets.<lower>]`` block to append to pipeline.toml."""
    return make_environment().get_template(TOML_BLOCK_TEMPLATE).render(**context)
