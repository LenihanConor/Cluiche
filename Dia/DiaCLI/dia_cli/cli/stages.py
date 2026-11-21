"""dia stages -- read-only views of the stage catalogue and last results.

Reads files only; live data is `dia query stages.list`.
"""
import json
from pathlib import Path

import click

from dia_cli.utils import stage_catalogue as sc
from dia_cli.utils import stage_catalogue_view as view
from dia_cli.utils.repo_root import find_repo_root


def _games(repo_root):
    """{lowercase game name: (diagame path, app folder name)} from Cluiche/Assets/<App>/*.diagame."""
    out = {}
    assets = repo_root / "Cluiche" / "Assets"
    if assets.is_dir():
        for app in sorted(p for p in assets.iterdir() if p.is_dir()):
            for dg in app.glob("*.diagame"):
                out[dg.stem.lower()] = (dg, app.name)
    return out


def _resolve(game, diagame, results):
    app = None
    if diagame:
        dg = Path(diagame)
    else:
        repo_root = find_repo_root(__file__)
        games = _games(repo_root)
        if game is None:
            if len(games) != 1:
                raise click.UsageError(f"--game is required (known: {', '.join(sorted(games))})")
            game = next(iter(games))
        if game.lower() not in games:
            raise click.UsageError(f"unknown game '{game}' (known: {', '.join(sorted(games))})")
        dg, app = games[game.lower()]
    if results:
        rp = Path(results)
    elif app:
        rp = find_repo_root(__file__) / "Cluiche" / "out" / app / "stage_results.json"
    else:
        rp = dg.parent / "stage_results.json"
    return dg, rp


def _load(game, diagame, results):
    dg, rp = _resolve(game, diagame, results)
    result, cat = sc.load(dg)
    if result != sc.OK:
        raise click.ClickException(f"cannot load stage catalogue from {dg}: {result}")
    res = view.load_results(rp, warn=lambda m: click.secho(m, err=True, dim=True))
    return cat, res


def _options(f):
    f = click.option("--results", "results", hidden=True, default=None,
                     help="Test hook: path to stage_results.json.")(f)
    f = click.option("--diagame", "diagame", hidden=True, default=None,
                     help="Test hook: path to a .diagame.")(f)
    f = click.option("--game", default=None, metavar="NAME",
                     help="Game name (cow, cluichetest); required when more than one exists.")(f)
    return f


@click.group("stages")
def cli():
    """Stage catalogue and last results (reads files; see `dia query stages.list` for live data)."""


@cli.command("list")
@_options
@click.option("--tag", "tags", multiple=True, help="Require this tag (repeatable; all must match).")
@click.option("--kind", default=None, type=click.Choice(view.valid_kinds()), help="Only entries of this kind.")
@click.option("--json", "as_json", is_flag=True, help="Emit JSON (stages.list reply shape).")
def list_cmd(game, diagame, results, tags, kind, as_json):
    """List stages."""
    cat, res = _load(game, diagame, results)
    entries = view.list_entries(cat, res, tags, kind)
    click.echo(json.dumps({"stages": entries}, indent=2) if as_json else view.render_table(entries))


@cli.command("get")
@click.argument("stage")
@_options
@click.option("--json", "as_json", is_flag=True, help="Emit JSON (stages.get reply shape).")
def get_cmd(stage, game, diagame, results, as_json):
    """Show one stage."""
    cat, res = _load(game, diagame, results)
    e = view.get_entry(cat, res, stage)
    if e is None:
        raise click.ClickException(f"unknown stage '{stage}'")
    click.echo(json.dumps(e, indent=2) if as_json else view.render_detail(e))


@cli.command("results")
@click.argument("stage", required=False)
@_options
@click.option("--json", "as_json", is_flag=True, help="Emit JSON (stages.results reply shape).")
def results_cmd(stage, game, diagame, results, as_json):
    """Show last results (all stages, or one)."""
    cat, res = _load(game, diagame, results)
    if stage is not None:
        if cat.find(stage) < 0:
            raise click.ClickException(f"unknown stage '{stage}'")
        one = res.get(stage)
        if as_json:
            click.echo(json.dumps(one, indent=2))
        else:
            click.echo(f"{stage}: {one['label']} ({one.get('severity', '')})" if one else f"{stage}: no result")
        return
    if as_json:
        click.echo(json.dumps(res, indent=2))
    else:
        for name, r in res.items():
            click.echo(f"{name}  {r.get('label', '')}  {r.get('severity', '')}")
