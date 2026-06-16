"""Smoke test: CLI entry point is importable and the main function exists."""
from dia_cli.cli_main import main


def test_main_is_callable():
    """The CLI entry point registered in pyproject.toml must be callable."""
    assert callable(main)
