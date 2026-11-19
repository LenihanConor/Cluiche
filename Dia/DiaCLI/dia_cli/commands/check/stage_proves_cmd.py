"""Click command + rendering for `dia check stage-proves`."""
import json
from pathlib import Path

import click

from dia_cli.commands.check import stage_proves
from dia_cli.utils.repo_root import find_repo_root


def to_dict(res):
    return {
        "broken_links": [{"stage": b.stage, "game": b.game, "path": b.path} for b in res.broken_links],
        "unproven": list(res.unproven),
        "coverage": {k: list(v) for k, v in sorted(res.coverage.items())},
    }


def render_report(res):
    lines = ["# Stage Proves Report", "",
             "_Refresh by running `dia check stage-proves`._", "", "## Broken links", ""]
    lines += [f"- `{b.path}` (stage `{b.stage}`, game `{b.game}`)" for b in res.broken_links] or ["None."]
    lines += ["", "## Unproven specs", ""]
    lines += [f"- `{p}`" for p in res.unproven] or ["None."]
    lines += ["", "## Coverage", ""]
    lines += [f"- `{spec}` -> {', '.join(stages)}" for spec, stages in sorted(res.coverage.items())] or ["None."]
    return "\n".join(lines) + "\n"


@click.command("stage-proves")
@click.option("--game", default=None, metavar="NAME", help="Limit broken-link reporting to one game.")
@click.option("--strict", is_flag=True, default=False, help="Unproven specs also exit 1.")
@click.option("--json", "as_json", is_flag=True, default=False, help="Print the result as JSON.")
@click.option("--root", "root", default=None, hidden=True, type=click.Path(file_okay=False))
@click.pass_context
def stage_proves_cmd(ctx, game, strict, as_json, root):
    """Check stage `proves` links against specs that opt in to a testbed.

    A spec opts in with a `**Testbed:** required` line in its header block
    (before the first `## ` heading; matched case-insensitively). Approved,
    In Progress or Done specs with the marker that no stage names in `proves`
    are reported as unproven. Draft specs are never flagged.

    Exit 1 on any broken link; unproven specs exit 0 unless --strict.
    Writes Cluiche/out/check/stage-proves.md.
    """
    repo_root = Path(root) if root else find_repo_root(__file__)
    res = stage_proves.check(repo_root, game=game)

    out = repo_root / "Cluiche" / "out" / "check" / "stage-proves.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render_report(res), encoding="utf-8")

    if as_json:
        click.echo(json.dumps(to_dict(res), indent=2))
    else:
        click.echo(f"[dia check] stage-proves: {len(res.broken_links)} broken link(s), "
                   f"{len(res.unproven)} unproven spec(s), {len(res.coverage)} covered spec(s)")
        for b in res.broken_links:
            click.echo(f"  BROKEN   {b.path} (stage {b.stage}, game {b.game})")
        for p in res.unproven:
            click.echo(f"  UNPROVEN {p}")
        click.echo(f"[dia check] Report written to {out}")

    if res.broken_links or (strict and res.unproven):
        ctx.exit(1)
