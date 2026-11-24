"""Validate a `--stage` / `--preset` launch selection against a game's own files (before build or launch)."""
from pathlib import Path

from dia_cli.utils import stage_catalogue as sc


def validate_launch_selection(repo_root, app_name, stage, preset):
    """Return an error message, or None when the selection is valid.

    Valid names come from the game's .diagame (stages listed in Boot's transitions) and each
    stage's own `presets`; the app name is the target's `app_name` (Assets/<app>/<app>.diagame).
    """
    if preset and not stage:
        return "--preset needs --stage"
    if not stage:
        return None
    if not app_name:
        return "--stage is only valid for game targets"
    path = Path(repo_root) / "Cluiche" / "Assets" / app_name / f"{app_name.lower()}.diagame"
    result, cat = sc.load(path)
    if result != sc.OK:
        return f"cannot read the stage catalogue ({result}): {path}"
    names = [e.name for e in cat.entries]
    i = cat.find(stage)
    if i < 0:
        return f"unknown stage '{stage}'. Valid stages: {', '.join(names) or '(none)'}"
    if preset:
        presets = [p["name"] for p in cat.entries[i].presets]
        if preset not in presets:
            return (f"unknown preset '{preset}' for stage '{stage}'. "
                    f"Valid presets: {', '.join(presets) or '(none declared)'}")
    return None
