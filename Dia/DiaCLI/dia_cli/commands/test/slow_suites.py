"""Single definition of what counts as a SLOW_ GoogleTest suite.

A parameterized suite is named ``Prefix/SLOW_Name`` (INSTANTIATE_TEST_SUITE_P), so a bare ``-SLOW_*``
exclusion lets those run in the default pass.
"""

SLOW_EXCLUDE_FILTER = "-SLOW_*:*/SLOW_*"
DEFAULT_GTEST_FILTER_ARG = f"--gtest_filter={SLOW_EXCLUDE_FILTER}"


def is_slow_suite(name: str) -> bool:
    """True for ``SLOW_X`` and for parameterized ``Prefix/SLOW_X``."""
    return name.startswith("SLOW_") or "/SLOW_" in name
