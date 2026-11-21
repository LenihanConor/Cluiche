"""'dia perf' Click group -- offline profiling analysis."""
import click

from .convert_cmd import convert


@click.group("perf")
def perf_group():
    """Offline profiling analysis (profile.jsonl -> Perfetto)."""


perf_group.add_command(convert)
