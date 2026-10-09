"""dia scaffold app — generate a new Cluiche application skeleton."""
from __future__ import annotations

import re
from pathlib import Path
from typing import List

import click

from dia_cli.utils.repo_root import find_repo_root

from .app_templates import (
    DEFAULT_STAGE,
    baseline_references,
    render_all,
    render_context,
    render_toml_block,
)
from .sln_cmd import add_project_to_solution, solution_has_project

_IDENT = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def _validate(repo_root: Path, name: str, stage: str) -> None:
    if not _IDENT.match(name):
        raise click.ClickException(
            f"Invalid application name '{name}': use a C++ identifier "
            "(letters, digits, underscore; not starting with a digit), e.g. MyGame."
        )
    if not _IDENT.match(stage):
        raise click.ClickException(
            f"Invalid stage name '{stage}': use a PascalCase C++ identifier, e.g. FrontEnd."
        )
    if stage == "Boot":
        raise click.ClickException("Stage name 'Boot' is reserved for the baseline boot stage.")

    type_name = f"{stage}Module"
    clashes: List[Path] = []
    baseline_modules = repo_root / "Cluiche" / "CluicheGameBaseline" / "Modules"
    if baseline_modules.is_dir():
        clashes += list(baseline_modules.glob(f"{type_name}.h"))
    dia_dir = repo_root / "Dia"
    if dia_dir.is_dir():
        clashes += list(dia_dir.rglob(f"{type_name}.h"))
    if clashes:
        raise click.ClickException(
            f"Stage module type '{type_name}' clashes with existing {clashes[0].relative_to(repo_root).as_posix()}. "
            "Choose a different --stage."
        )


def _newline_of(data: bytes) -> bytes:
    return b"\r\n" if b"\r\n" in data else b"\n"


@click.command("app")
@click.argument("name")
@click.option("--stage", default=DEFAULT_STAGE, show_default=True,
              help="PascalCase name of the initial placeholder stage (not Boot).")
@click.option("--force", is_flag=True, default=False,
              help="Overwrite existing generated files (solution entry and pipeline target are never rewritten).")
@click.option("--dry-run", is_flag=True, default=False,
              help="Print the file list and what would change without writing anything.")
def app(name: str, stage: str, force: bool, dry_run: bool) -> None:
    """Generate a new Cluiche application (project, Main.cpp, manifests, stage).

    Also adds the project to Cluiche.sln (folder 'executables') and appends a
    [targets.<name>] block to pipeline.toml. Existing files are skipped unless
    --force is given; the solution entry and toml block are never duplicated.

    \b
    Example:
        dia scaffold app MyGame --stage FrontEnd
    """
    repo_root = find_repo_root(__file__)
    _validate(repo_root, name, stage)

    toml_path = repo_root / "pipeline.toml"
    sln_path = repo_root / "Cluiche" / "Cluiche.sln"
    if not toml_path.exists():
        raise click.ClickException(f"pipeline.toml not found: {toml_path}")
    if not sln_path.exists():
        raise click.ClickException(f"Solution not found: {sln_path}")

    try:
        refs = baseline_references(repo_root)
    except (OSError, ValueError) as exc:
        raise click.ClickException(f"Cannot read baseline references: {exc}")
    ctx = render_context(name, refs, stage=stage)
    files = render_all(ctx)
    toml_block = render_toml_block(ctx)

    toml_bytes = toml_path.read_bytes()
    has_target = re.search(
        rf"^\[targets\.{re.escape(ctx['lower'])}\]\s*$",
        toml_bytes.decode("utf-8"), re.MULTILINE) is not None
    has_sln = solution_has_project(sln_path.read_text(encoding="utf-8"), name)

    created: List[str] = []
    skipped: List[str] = []
    overwritten: List[str] = []
    for rel, content in files.items():
        path = repo_root / rel
        if path.exists():
            (overwritten if force else skipped).append(rel)
        else:
            created.append(rel)
        if dry_run or rel in skipped:
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")

    if not dry_run:
        if not has_target:
            nl = _newline_of(toml_bytes)
            block = toml_block.replace("\r\n", "\n").replace("\n", nl.decode()).encode("utf-8")
            sep = b"" if toml_bytes.endswith(nl) else nl
            toml_path.write_bytes(toml_bytes + sep + block)
        if not has_sln:
            add_project_to_solution(repo_root, name, folder="executables", root="Cluiche")

    prefix = "[dry-run] Would " if dry_run else ""
    click.echo(f"{prefix}scaffold application {name} (stage {stage}):")
    for label, items in (("created", created), ("overwritten", overwritten), ("skipped (exists)", skipped)):
        for rel in items:
            click.echo(f"  {label}: {rel}")
    click.echo(f"  Cluiche.sln: {'skipped (already present)' if has_sln else 'added'} ({name}, folder executables)")
    click.echo(f"  pipeline.toml: {'skipped (already present)' if has_target else 'added'} [targets.{ctx['lower']}]")
    click.echo(
        f"\n{len(created)} created, {len(overwritten)} overwritten, {len(skipped)} skipped."
    )
