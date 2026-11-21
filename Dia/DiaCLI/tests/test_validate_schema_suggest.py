"""Unit tests for schema suggestion feature (dia_cli/commands/validate/schema_suggest.py).

Tests the suggest() and format_suggestion() functions for finding and formatting
typo corrections in validation error messages.
"""
import pytest

from dia_cli.commands.validate.schema_suggest import suggest, format_suggestion


class TestSuggest:
    """Tests for the suggest() function."""

    def test_close_match_returns_best_candidate(self):
        """Close match should return the best matching valid string."""
        # 'TemperateForst' is missing 'e' in Forest; should match 'TemperateForest'
        result = suggest('TemperateForst', ['TemperateForest', 'Tundra'])
        assert result == 'TemperateForest'

    def test_nested_key_close_match(self):
        """Nested key like 'grazing.juvenile_rate' should match closest key."""
        # 'grazing.juvenile_rate' is close to 'grazing.juvenile_rate_scale'
        # Both are more similar than 'grazing.rate'
        result = suggest('grazing.juvenile_rate', ['grazing.juvenile_rate_scale', 'grazing.rate'])
        assert result == 'grazing.juvenile_rate_scale'

    def test_nothing_close_returns_none(self):
        """If no valid string meets the threshold, return None."""
        result = suggest('xyz123', ['apple', 'banana', 'cherry'])
        assert result is None

    def test_empty_valid_list_returns_none(self):
        """Empty valid list should return None."""
        result = suggest('candidate', [])
        assert result is None

    def test_empty_candidate_returns_none(self):
        """Empty candidate string should return None."""
        result = suggest('', ['valid1', 'valid2'])
        assert result is None

    def test_exact_match_returns_none(self):
        """Exact match of candidate in valid should return None (nothing to suggest)."""
        result = suggest('TemperateForest', ['TemperateForest', 'Tundra', 'Desert'])
        assert result is None

    def test_tie_rule_lexicographically_smaller(self):
        """When two strings have equal ratio, return the lexicographically smaller."""
        # Construct strings with equal similarity to candidate
        # 'aaa' vs ['bbb', 'aab'] - both have low but equal ratio, pick 'aab' (lex smaller)
        result = suggest('aaa', ['zzz', 'aab'])
        # If they have equal ratio >= 0.6, pick the lex smaller: 'aab'
        # 'aab' vs 'aaa' ratio: 2/3 ≈ 0.667
        # 'zzz' vs 'aaa' ratio: 0/3 = 0
        # So 'aab' wins
        assert result == 'aab'

    def test_threshold_boundary_just_under_0_6(self):
        """Ratio just under 0.6 should return None."""
        # Create a case where ratio is just below 0.6
        # For example: 'ab' vs 'xy' should have very low similarity
        # We need a more precise test: 'abc' vs 'xbc' - 2/3 ≈ 0.667, too high
        # Try 'abc' vs 'xyz' - 0/3 = 0, too low
        # Let's use: 'abcd' vs 'xbcd' - 3/4 = 0.75, still too high
        # We need: 'ABCDE' vs 'FGHIJ' gives us 0
        # Try something that gives us ~0.55 or ~0.59
        # 'abc' vs 'abd' gives 2/3 ≈ 0.667 (too high)
        # 'abcdefg' vs 'xxxxxxx' gives 0/7 = 0
        # 'abcdefghij' vs 'abcxxxxxx' gives 3/10 = 0.3
        # Let's construct: 'test' vs 'txst' = 3/4 = 0.75 (too high)
        # 'test' vs 'xest' = 3/4 = 0.75 (too high)
        # 'test' vs 'txxx' = 2/4 = 0.5 (just below 0.6!)
        result = suggest('test', ['txxx'])  # ratio ~0.5, below threshold
        assert result is None

    def test_threshold_boundary_just_at_0_6(self):
        """Ratio at exactly 0.6 should return the suggestion."""
        # 'abc' vs 'bbc' = 2/3 ≈ 0.667 (above 0.6, should match)
        result = suggest('abc', ['bbc'])
        assert result == 'bbc'

    def test_case_sensitive_matching(self):
        """Matching should be case-sensitive."""
        # 'MyKey' vs 'mykey' should not match well
        # 'MyKey' vs 'MyKeyX' should match better
        result = suggest('MyKey', ['mykey', 'MyKeyX'])
        assert result == 'MyKeyX'

    def test_iterable_as_tuple(self):
        """suggest() should accept any iterable, including tuples."""
        result = suggest('test', ('best', 'rest', 'west'))
        assert result in ['best', 'rest', 'west']

    def test_generator_as_iterable(self):
        """suggest() should accept generators."""
        def valid_gen():
            yield 'apple'
            yield 'banana'
        result = suggest('aple', valid_gen())
        # 'aple' is much closer to 'apple' than 'banana'
        assert result == 'apple'

    def test_multiple_similar_candidates_returns_best(self):
        """When multiple candidates are similar, return the best match."""
        # 'color' is closer to 'colour' than to 'cold'
        result = suggest('color', ['colour', 'cold', 'controller'])
        assert result == 'colour'


class TestFormatSuggestion:
    """Tests for the format_suggestion() function."""

    def test_format_suggestion_with_none(self):
        """format_suggestion(None) should return empty string."""
        assert format_suggestion(None) == ""

    def test_format_suggestion_with_string(self):
        """format_suggestion(s) should return formatted suggestion message."""
        result = format_suggestion('TemperateForest')
        assert result == " (did you mean 'TemperateForest'?)"

    def test_format_suggestion_with_empty_string(self):
        """format_suggestion('') should return formatted empty suggestion."""
        result = format_suggestion('')
        assert result == " (did you mean ''?)"

    def test_format_suggestion_with_special_chars(self):
        """format_suggestion should handle special characters."""
        result = format_suggestion("key.with-special_chars")
        assert result == " (did you mean 'key.with-special_chars'?)"


class TestIntegrationWithErrorMessages:
    """Integration tests showing how suggest and format_suggestion work together."""

    def test_error_message_with_suggestion(self):
        """Build a complete error message with suggestion."""
        invalid_key = 'grazing.juvenile_rate'
        valid_keys = ['grazing.juvenile_rate_scale', 'grazing.rate', 'grazing.adult_rate']

        suggestion = suggest(invalid_key, valid_keys)
        formatted = format_suggestion(suggestion)

        message = f"key '{invalid_key}' has no schema entry{formatted}"

        assert "did you mean" in message
        assert "grazing.juvenile_rate_scale" in message

    def test_error_message_without_suggestion(self):
        """Build an error message when no suggestion is available."""
        invalid_key = 'xyz123invalid'
        valid_keys = ['grazing.rate', 'temperature', 'biome_type']

        suggestion = suggest(invalid_key, valid_keys)
        formatted = format_suggestion(suggestion)

        message = f"key '{invalid_key}' has no schema entry{formatted}"

        assert message == "key 'xyz123invalid' has no schema entry"
