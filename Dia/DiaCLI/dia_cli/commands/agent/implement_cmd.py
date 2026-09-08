"""dia agent implement — execute a plan autonomously."""
import sys
from pathlib import Path

import click

from dia_cli.utils.repo_root import find_repo_root
from .config import AgentConfig
from .loop import run_agent_loop
from .handoff import print_handoff


@click.command("implement")
@click.argument("plan_path", type=click.Path(exists=True))
@click.option("--spec", "spec_path", type=click.Path(exists=True), default=None,
              help="Path to the spec file. If omitted, inferred from plan filename.")
@click.option("--model", default=None, help="Override model (default: claude-sonnet-4-6-20250514)")
@click.option("--cost-limit", type=float, default=None, help="Override cost ceiling in dollars")
@click.option("--token-limit", type=int, default=None, help="Override total token budget")
@click.option("--timeout", type=int, default=None, help="Override timeout in seconds")
@click.option("--verbose", "-v", is_flag=True, default=False, help="Show agent reasoning and tool calls")
@click.option("--dry-run", is_flag=True, default=False,
              help="Print config and exit without running")
def implement(plan_path, spec_path, model, cost_limit, token_limit, timeout, verbose, dry_run):
    """Execute a plan file autonomously.

    Reads the plan, works through tasks in order, stops on failure.
    Never commits — leaves the working tree dirty for you to review.

    \b
    Examples:
        dia agent implement docs/specs/features/dia/foo.plan.md
        dia agent implement foo.plan.md --spec foo.md --verbose
        dia agent implement foo.plan.md --cost-limit 2.0 --dry-run
    """
    repo_root = find_repo_root(__file__)
    plan = Path(plan_path).resolve()

    # Infer spec path from plan if not provided
    if spec_path is None:
        inferred = Path(str(plan).replace(".plan.md", ".md"))
        if inferred.is_file() and inferred != plan:
            spec = inferred
        else:
            click.echo(f"ERROR: Could not infer spec path from plan. Use --spec.", err=True)
            sys.exit(1)
    else:
        spec = Path(spec_path).resolve()

    # Build config with overrides
    config = AgentConfig()
    if model:
        config.model = model
    if cost_limit is not None:
        config.cost_ceiling_dollars = cost_limit
    if token_limit is not None:
        config.max_total_tokens = token_limit
    if timeout is not None:
        config.timeout_seconds = timeout

    if dry_run:
        click.echo("DRY RUN — would execute with:")
        click.echo(f"  Plan:        {plan}")
        click.echo(f"  Spec:        {spec}")
        click.echo(f"  Model:       {config.model}")
        click.echo(f"  Cost limit:  ${config.cost_ceiling_dollars}")
        click.echo(f"  Token limit: {config.max_total_tokens:,}")
        click.echo(f"  Timeout:     {config.timeout_seconds}s")
        click.echo(f"  Allowed dia: {config.allowed_dia_commands}")
        return

    # Verify anthropic key is available
    import os
    if not os.environ.get("ANTHROPIC_API_KEY"):
        click.echo("ERROR: ANTHROPIC_API_KEY environment variable not set.", err=True)
        sys.exit(1)

    click.echo("=" * 60)
    click.echo("DIA AGENT — Autonomous Plan Executor")
    click.echo("=" * 60)

    state = run_agent_loop(
        plan_path=plan,
        spec_path=spec,
        repo_root=repo_root,
        config=config,
        verbose=verbose,
    )

    print_handoff(state, config)

    if state.last_error:
        sys.exit(1)
