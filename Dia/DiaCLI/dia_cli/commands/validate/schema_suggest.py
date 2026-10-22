"""Schema validation suggestion system for typo correction.

Provides functions to suggest valid strings that are similar to invalid ones,
and to format those suggestions into error messages.
"""
from typing import Iterable


def suggest(candidate: str, valid: Iterable[str]) -> str | None:
    """Suggest a correction for an invalid key.

    Returns the single best matching valid string if:
    - The match ratio (via difflib.SequenceMatcher) is >= 0.6
    - The candidate is not an exact match of any valid string

    On ties (equal ratio), returns the lexicographically smaller string.

    Args:
        candidate: The invalid string to find a suggestion for
        valid: Iterable of valid strings to search within

    Returns:
        The best matching valid string, or None if:
        - candidate is empty
        - valid is empty
        - candidate is an exact match in valid
        - no match ratio >= 0.6 is found
    """
    import difflib

    # Handle empty cases
    if not candidate or not isinstance(candidate, str):
        return None

    # Convert valid to a list if it's a generator or other iterable; non-str items are ignored
    valid_list = [v for v in valid if isinstance(v, str)]
    if not valid_list:
        return None

    # Check for exact match - if found, return None (nothing to suggest)
    if candidate in valid_list:
        return None

    # Find the best match using SequenceMatcher
    best_match: str | None = None
    best_ratio = 0.0

    for v in valid_list:
        ratio = difflib.SequenceMatcher(None, candidate, v).ratio()

        # Update best_match if:
        # 1. ratio >= 0.6 (meets threshold) and
        # 2. (ratio > best_ratio OR (ratio == best_ratio and v is lex smaller))
        if ratio >= 0.6:
            if ratio > best_ratio or (ratio == best_ratio and (best_match is None or v < best_match)):
                best_match = v
                best_ratio = ratio

    return best_match


def format_suggestion(s: str | None) -> str:
    """Format a suggestion into an error message fragment.

    Args:
        s: The suggestion string, or None

    Returns:
        Empty string if s is None, otherwise " (did you mean '<s>'?)"
    """
    if s is None:
        return ""
    return f" (did you mean '{s}'?)"
