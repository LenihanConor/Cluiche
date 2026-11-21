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


def _rel(repo_root: Path, path: Path) -> str:
    return str(path.relative_to(repo_root)).replace("\\", "/")


def _app_spec(repo_root: Path, app: str) -> Path:
    """Application spec: docs/specs/applications/<app>/<app>.md."""
    path = repo_root / "docs" / "specs" / "applications" / app.lower() / f"{app.lower()}.md"
    if not path.is_file():
        raise click.ClickException(f"Application spec not found: {_rel(repo_root, path)}")
    return path


def _find_system_spec(repo_root: Path, app: str, system: str) -> Path:
    """System spec: docs/specs/applications/<app>/systems/**/<system>.md."""
    systems_dir = repo_root / "docs" / "specs" / "applications" / app.lower() / "systems"
    matches = [
        c for c in systems_dir.rglob(f"{system.lower()}.md")
        if ".plan." not in c.name
    ]
    if not matches:
        raise click.ClickException(
            f"System spec '{system}' not found under {_rel(repo_root, systems_dir)}"
        )
    return matches[0]


def _name_to_namespace(name: str) -> str:
    parts = name.replace("-", " ").replace("_", " ").split()
    return "::".join(p.capitalize() for p in parts)


@click.command("spec-scaffold")
@click.argument("spec_type", type=click.Choice(["feature", "system"]))
@click.argument("name")
@click.option("--app", default="dia", show_default=True,
              help="Application the spec belongs to (dia, cow, ...).")
@click.option("--parent", "parent_system", default=None,
              help="Parent system name (required for features).")
@click.option("--output-dir", default=None,
              help="Output directory (auto-detected if omitted; relative paths resolve against the cwd).")
@click.option("--dry-run", is_flag=True, default=False, help="Print to stdout.")
def spec_scaffold(spec_type: str, name: str, app: str, parent_system: str,
                  output_dir: str, dry_run: bool) -> None:
    """Generate a spec file skeleton with all required sections.

    SPEC_TYPE is 'feature' or 'system'.
    NAME is the spec name in kebab-case (e.g. camera3d-types, diacamera3d).

    Features land beside their parent system spec; systems land in
    docs/specs/applications/<app>/systems/<name>/<name>.md.

    Examples:
        dia docs spec-scaffold feature camera3d-types --parent DiaCamera3D
        dia docs spec-scaffold system DiaCamera3D
        dia docs spec-scaffold feature bus-recorder --app cow --parent f1-harness
    """
    repo_root = find_repo_root(__file__)

    display_name = " ".join(
        w.capitalize() for w in name.replace("-", " ").replace("_", " ").split()
    )

    if spec_type == "feature":
        if not parent_system:
            raise click.UsageError("--parent is required for feature specs.")
        parent_path = _find_system_spec(repo_root, app, parent_system)
        default_dir = parent_path.parent
        content = _FEATURE_TEMPLATE.format(
            name=display_name,
            parent_spec_path=_rel(repo_root, parent_path),
            namespace=_name_to_namespace(name),
        )
    else:
        parent_path = _app_spec(repo_root, app)
        default_dir = parent_path.parent / "systems" / name.lower()
        content = _SYSTEM_TEMPLATE.format(
            name=display_name,
            parent_spec_path=_rel(repo_root, parent_path),
        )

    if dry_run:
        click.echo(content)
        return

    out_dir = Path(output_dir).resolve() if output_dir else default_dir
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / (f"{name.lower()}.md" if spec_type == "system" else f"{name}.md")
    if out_path.exists():
        raise click.ClickException(f"Refusing to overwrite existing {out_path}")

    out_path.write_text(content, encoding="utf-8")
    try:
        shown = out_path.relative_to(repo_root)
    except ValueError:
        shown = out_path
    click.echo(f"[dia docs] Created {shown}")
    click.echo("  Next: fill in TODOs, then run /spec-feature or /spec-system for AI review")
