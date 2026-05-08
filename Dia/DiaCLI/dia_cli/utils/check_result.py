"""Shared CheckResult dataclass used by all dia env verify utilities."""
from dataclasses import dataclass


@dataclass
class CheckResult:
    name: str
    category: str
    status: str  # "pass", "warn", "fail"
    detail: str = ""
    fix: str = ""
