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
