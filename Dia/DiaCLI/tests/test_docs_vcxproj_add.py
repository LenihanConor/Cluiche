"""Regression tests for `dia docs vcxproj-add` anchor placement."""
from __future__ import annotations

from dia_cli.commands.docs.vcxproj_cmd import add_to_vcxproj

VCXPROJ = """<Project>
  <ItemGroup>
    <ClCompile Include="A.cpp" />
    <ClCompile Include="B.cpp" />
  </ItemGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
    </ClCompile>
  </ItemDefinitionGroup>
</Project>
"""


def test_cpp_entry_lands_in_item_group_not_item_definition_group(tmp_path):
    proj = tmp_path / "P.vcxproj"
    proj.write_text(VCXPROJ, encoding="utf-8")

    assert add_to_vcxproj(proj, "C.cpp", is_header=False) is True

    lines = proj.read_text(encoding="utf-8").splitlines()
    assert lines.index('    <ClCompile Include="C.cpp" />') == lines.index('    <ClCompile Include="B.cpp" />') + 1


def test_cpp_entry_after_multiline_include_entry(tmp_path):
    proj = tmp_path / "P.vcxproj"
    proj.write_text(
        '<Project>\n  <ItemGroup>\n    <ClCompile Include="A.cpp">\n'
        "      <Opt>x</Opt>\n    </ClCompile>\n  </ItemGroup>\n"
        "  <ItemDefinitionGroup>\n    <ClCompile>\n    </ClCompile>\n  </ItemDefinitionGroup>\n</Project>\n",
        encoding="utf-8",
    )

    add_to_vcxproj(proj, "B.cpp", is_header=False)

    lines = proj.read_text(encoding="utf-8").splitlines()
    assert lines.index('    <ClCompile Include="B.cpp" />') == lines.index("    </ClCompile>") + 1


FILTERS = r"""<Project>
  <ItemGroup>
    <ClCompile Include="Modules\TestStages\A.cpp">
      <Filter>Modules\TestStages</Filter>
    </ClCompile>
    <ClCompile Include="Modules\TestStages\Drawers\D.cpp">
      <Filter>Modules\TestStages\Drawers</Filter>
    </ClCompile>
    <ClCompile Include="Other\O.cpp">
      <Filter>Custom</Filter>
    </ClCompile>
  </ItemGroup>
</Project>
"""


def _infer(tmp_path, rel):
    from dia_cli.commands.docs.vcxproj_cmd import infer_filter

    f = tmp_path / "P.vcxproj.filters"
    f.write_text(FILTERS, encoding="utf-8")
    return infer_filter(f, rel)


def test_infer_filter_reuses_sibling_filter(tmp_path):
    assert _infer(tmp_path, r"Modules\TestStages\Drawers\X.cpp") == r"Modules\TestStages\Drawers"
    assert _infer(tmp_path, r"Other\X.cpp") == "Custom"


def test_infer_filter_falls_back_to_nearest_ancestor(tmp_path):
    assert _infer(tmp_path, r"Modules\TestStages\New\X.cpp") == r"Modules\TestStages"


def test_infer_filter_falls_back_to_first_segment_or_none(tmp_path):
    assert _infer(tmp_path, r"Brand\New\X.cpp") == "Brand"
    assert _infer(tmp_path, "X.cpp") is None


def test_infer_filter_cross_project_path_never_yields_dotdot(tmp_path):
    # Regression (2026-11-05): `..\..\CoW\...` produced a bogus `..` filter.
    assert _infer(tmp_path, r"..\..\CoW\Shared\WorldGeneration\X.cpp") == "CoW"
    assert _infer(tmp_path, r".\..\CoW\X.cpp") == "CoW"
    assert _infer(tmp_path, r"..\X.cpp") is None


def test_infer_filter_ignores_bare_dotdot_ancestor(tmp_path):
    from dia_cli.commands.docs.vcxproj_cmd import infer_filter

    f = tmp_path / "P.vcxproj.filters"
    f.write_text(
        r"""<Project>
  <ItemGroup>
    <ClCompile Include="..\..\Root.cpp">
      <Filter>CluicheTest</Filter>
    </ClCompile>
    <ClCompile Include="..\..\CoW\Shared\Y.cpp">
      <Filter>CoW</Filter>
    </ClCompile>
  </ItemGroup>
</Project>
""",
        encoding="utf-8",
    )
    # The bare `..\..` dir must not lend its filter to an unrelated project.
    assert infer_filter(f, r"..\..\NewGame\Z.cpp") == "NewGame"
    # A real shared ancestor still wins.
    assert infer_filter(f, r"..\..\CoW\Shared\New\Z.cpp") == "CoW"
