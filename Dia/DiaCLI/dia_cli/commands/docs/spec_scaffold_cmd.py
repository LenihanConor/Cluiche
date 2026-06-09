"""dia docs spec-scaffold — generate spec file skeleton without AI."""
from __future__ import annotations

from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root


_FEATURE_TEMPLATE = """\
# Feature Spec: {name}

**Parent:** @{parent_spec_path}
**Status:** `Draft`

## Summary

TODO: One-paragraph description of what this feature does.

## Problem

TODO: What problem does this solve? Why is it needed?

## Acceptance Criteria

1. TODO: First acceptance criterion
2. TODO: Second acceptance criterion

## API Design

```cpp
namespace Dia::{namespace}
{{
    // TODO: key types and interfaces
}}
```

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | TODO | TODO |

## Binding Decisions

No binding constraints apply.

## Open Design Questions

1. TODO: First design question (or delete this section if straightforward)
"""


_SYSTEM_TEMPLATE = """\
# System Spec: {name}

**Parent:** @{parent_spec_path}
**Status:** `Draft`

## Summary

TODO: One-paragraph description of what this system provides.

## Problem

TODO: What gap does this system fill?

## Goals

1. TODO: Primary goal
2. TODO: Secondary goal

## Non-Goals

1. TODO: What this system explicitly does NOT do

## Features

| Feature | Spec | Status |
|---------|------|--------|
| TODO | TBD | Draft |

## Architecture

TODO: High-level architecture description.

## Binding Decisions

No binding constraints apply.

## Open Design Questions

1. TODO: First design question
"""


def _infer_parent_spec(repo_root: Path, spec_type: str, parent_system: str) -> str:
    if spec_type == "feature":
        candidates = list((repo_root / "docs" / "specs" / "systems").rglob(f"{parent_system.lower()}.md"))
        candidates = [c for c in candidates if ".plan." not in str(c)]
        if candidates:
            return str(candidates[0].relative_to(repo_root)).replace("\\", "/")
    elif spec_type == "system":
        candidates = list((repo_root / "docs" / "specs" / "applications").rglob("*.md"))
        if candidates:
            return str(candidates[0].relative_to(repo_root)).replace("\\", "/")
    return "docs/specs/applications/dia.md"


def _name_to_namespace(name: str) -> str:
    parts = name.replace("-", " ").replace("_", " ").split()
    return "::".join(p.capitalize() for p in parts)


@click.command("spec-scaffold")
@click.argument("spec_type", type=click.Choice(["feature", "system"]))
@click.argument("name")
@click.option("--parent", "parent_system", default=None,
              help="Parent system name (for features) or application (for systems).")
@click.option("--output-dir", default=None, help="Output directory (auto-detected if omitted).")
@click.option("--dry-run", is_flag=True, default=False, help="Print to stdout.")
def spec_scaffold(spec_type: str, name: str, parent_system: str, output_dir: str, dry_run: bool) -> None:
    """Generate a spec file skeleton with all required sections.

    SPEC_TYPE is 'feature' or 'system'.
    NAME is the spec name in kebab-case (e.g. camera3d-types, diacamera3d).

    Examples:
        dia docs spec-scaffold feature camera3d-types --parent DiaCamera3D
        dia docs spec-scaffold system DiaCamera3D
    """
    repo_root = find_repo_root(__file__)

    display_name = " ".join(
        w.capitalize() for w in name.replace("-", " ").replace("_", " ").split()
    )

    parent_spec = ""
    if parent_system:
        parent_spec = _infer_parent_spec(repo_root, spec_type, parent_system)
    else:
        parent_spec = "docs/specs/applications/dia.md"

    namespace = _name_to_namespace(name)

    if spec_type == "feature":
        content = _FEATURE_TEMPLATE.format(
            name=display_name,
            parent_spec_path=parent_spec,
            namespace=namespace,
        )
    else:
        content = _SYSTEM_TEMPLATE.format(
            name=display_name,
            parent_spec_path=parent_spec,
        )

    if dry_run:
        click.echo(content)
        return

    if output_dir:
        out_path = Path(output_dir) / f"{name}.md"
    else:
        if spec_type == "feature" and parent_system:
            parent_lower = parent_system.lower()
            out_dir = repo_root / "docs" / "specs" / "features" / "dia" / parent_lower
        elif spec_type == "system":
            out_dir = repo_root / "docs" / "specs" / "systems" / "dia"
        else:
            out_dir = repo_root / "docs" / "specs"
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / f"{name}.md"

    out_path.write_text(content, encoding="utf-8")
    click.echo(f"[dia docs] Created {out_path.relative_to(repo_root)}")
    click.echo(f"  Next: fill in TODOs, then run /spec-feature or /spec-system for AI review")
