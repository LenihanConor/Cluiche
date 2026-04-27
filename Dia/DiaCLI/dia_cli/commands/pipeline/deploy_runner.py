"""deploy subcommands: dia pipeline deploy <target>."""
import click
from pathlib import Path


@click.group("deploy")
def deploy_group():
    """Deploy runtime files for a build target to its output directory."""


def _make_deploy_cmd(target_name: str):
    @deploy_group.command(target_name)
    @click.option("--config", default="Debug", show_default=True,
                  help="Build configuration (Debug or Release).")
    @click.option("--out-dir", required=True, metavar="PATH",
                  help="Output directory — pass MSBuild $(TargetDir) here.")
    @click.option("--force", is_flag=True, default=False,
                  help="Re-copy files even if already up to date.")
    def _deploy(config, out_dir, force):
        from utils.repo_root import find_repo_root
        from commands.pipeline.pipeline_config import load_pipeline_config, PipelineConfigError
        from commands.pipeline.stages.package_stage import run_deploy

        # MSBuild $(TargetDir) ends with \, which escapes the closing " on Windows,
        # arriving as `C:\path\to\dir\"` — strip stray trailing quotes and slashes.
        out_dir = out_dir.rstrip('"').rstrip("\\/")

        repo_root = find_repo_root(__file__)
        try:
            pipeline_config = load_pipeline_config(repo_root)
        except PipelineConfigError as e:
            click.echo(f"ERROR: {e}", err=True)
            raise SystemExit(2)

        if target_name not in pipeline_config.targets:
            known = ", ".join(sorted(pipeline_config.targets))
            click.echo(f"ERROR: unknown target '{target_name}' (known: {known})", err=True)
            raise SystemExit(2)

        exit_code = run_deploy(
            config=pipeline_config,
            target=target_name,
            build_config=config,
            force=force,
            out_dir=Path(out_dir),
            repo_root=repo_root,
        )
        raise SystemExit(exit_code)

    _deploy.__name__ = f"deploy_{target_name}"


_DEPLOY_TARGETS = [
    "cluichetest",
    "googletest",
    "cluicheeditor",
    "diasfml",
    "diauiultralight",
]

for _target in _DEPLOY_TARGETS:
    _make_deploy_cmd(_target)
