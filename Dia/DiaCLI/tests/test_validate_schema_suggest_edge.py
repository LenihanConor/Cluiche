"""Edge tests for schema_suggest (S1-S6)."""
from __future__ import annotations

import itertools
import random
import time

import pytest

from dia_cli.commands.validate.schema_suggest import format_suggestion, suggest


@pytest.mark.parametrize("cand,valid,expected", [
    ("Wälder", ["Wälde", "Other"], "Wälde"),
    ("森林帯", ["森林带", "砂漠"], "森林带"),
    ("naïve", ["naive"], "naive"),
    ("😀😁😃", ["😀😁😂"], "😀😁😂"),
    ("é", ["é"], None),
    ("abc", ["xyz"], None),
])
def test_unicode(cand, valid, expected):
    assert suggest(cand, valid) == expected


@pytest.mark.parametrize("cand", ["", None, 5, [], b"abc"])
def test_non_string_or_empty_candidate(cand):
    assert suggest(cand, ["abc"]) is None


def test_empty_valid_and_generator():
    assert suggest("a", []) is None
    assert suggest("abcd", (v for v in ["abce"])) == "abce"


def test_valid_empty_string_member():
    assert suggest("abc", [""]) is None


@pytest.mark.parametrize("valid", [[1, "abce"], [None, "abce", 5.5], [["abce"], "abce"], [b"abce", "abce"]])
def test_non_str_valid_elements_ignored(valid):
    assert suggest("abcd", valid) == "abce"


def test_only_non_str_valid_elements():
    assert suggest("abc", [1, None, 2.5]) is None


def test_exact_match_returns_none_even_with_close_neighbours():
    assert suggest("abc", ["abc", "abd"]) is None


def test_ties_pick_lexicographically_smaller_both_orders():
    assert suggest("abcd", ["abce", "abcf"]) == "abce"
    assert suggest("abcd", ["abcf", "abce"]) == "abce"


def test_order_independent_and_duplicates():
    valid = ["alpha", "alphb", "alphc", "beta", "gamma", "alpha"]
    base = suggest("alphd", valid)
    for perm in itertools.islice(itertools.permutations(valid), 200):
        assert suggest("alphd", perm) == base
    assert suggest("alphd", valid + valid) == base


def test_idempotent_and_no_mutation():
    valid = ["one", "two", "three"]
    copy_ = list(valid)
    a = suggest("tre", valid)
    assert suggest("tre", valid) == a and valid == copy_


def test_result_never_equals_candidate_and_in_valid_random():
    rng = random.Random(1234)
    alphabet = "abcdeé中_.[]"
    for _ in range(400):
        valid = ["".join(rng.choice(alphabet) for _ in range(rng.randint(0, 8))) for _ in range(rng.randint(0, 6))]
        cand = "".join(rng.choice(alphabet) for _ in range(rng.randint(0, 8)))
        r = suggest(cand, valid)
        if r is not None:
            assert r in valid and r != cand and isinstance(r, str)
        if cand in valid:
            assert r is None


def test_threshold_boundary():
    # ratio is exactly 0.6 for 'abc' vs 'abcdefg' (2*3/10) -> qualifies; 'abcdefgh' is 6/11 -> does not
    assert suggest("abc", ["abcdefg"]) == "abcdefg"
    assert suggest("abc", ["abcdefgh"]) is None


def test_long_strings_complete_in_time():
    a, b = "ab" * 5000, "ab" * 4999 + "ba"
    t = time.time()
    r = suggest(a, [b])
    assert time.time() - t < 30
    assert r in (b, None)


def test_long_non_matching_strings():
    t = time.time()
    assert suggest("a" * 10000, ["b" * 10000]) is None
    assert time.time() - t < 30


def test_many_candidates():
    valid = [f"key{i}" for i in range(5000)]
    assert suggest("key4999x", valid) == "key4999"


@pytest.mark.parametrize("s,expected", [
    (None, ""), ("a", " (did you mean 'a'?)"), ("", " (did you mean ''?)"), ("é", " (did you mean 'é'?)"),
    ("it's", " (did you mean 'it's'?)"),
])
def test_format_suggestion(s, expected):
    assert format_suggestion(s) == expected
