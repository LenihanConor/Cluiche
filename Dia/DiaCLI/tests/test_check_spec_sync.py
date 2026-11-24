from dia_cli.cli.check import _extract_spec_symbols


def test_matches_level2_heading():
    t = "## Public Interfaces\n```cpp\nclass Foo {};\n```\n## Next\n"
    assert _extract_spec_symbols(t) == ["Foo"]


def test_matches_level3_heading_with_suffix():
    t = "### Public interfaces (sketch; finalised per feature)\n```cpp\nstruct Bar {};\n```\n### Next\n"
    assert _extract_spec_symbols(t) == ["Bar"]


def test_ignores_changes_heading():
    t = "## Public Interface Changes\n```cpp\nclass Nope {};\n```\n"
    assert _extract_spec_symbols(t) == []


def test_subsection_does_not_end_section():
    t = "## Public Interfaces\n### Sub\n```cpp\nclass Alpha {};\n```\n## End\n```cpp\nclass Beta {};\n```\n"
    assert _extract_spec_symbols(t) == ["Alpha"]


def test_source_dirs_follow_layer_order():
    from dia_cli.cli.check import _source_dirs_for
    assert _source_dirs_for("dia") == ["Dia"]
    assert _source_dirs_for("draoi") == ["Draoi", "Dia"]
    assert _source_dirs_for("cow")[:2] == ["Cluiche/CoW", "Draoi"]
    assert "Draoi" in _source_dirs_for("unknown-app")
    assert "Draoi" in _source_dirs_for("googletests")
