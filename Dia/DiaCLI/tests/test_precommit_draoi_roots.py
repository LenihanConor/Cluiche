from dia_cli.commands.docs.precommit_cmd import _check_vcxproj_sync


def _project(tmp_path, root, name, listed):
    d = tmp_path / root / name
    d.mkdir(parents=True)
    (d / f"{name}.vcxproj").write_text(
        f'<Project><ItemGroup><ClCompile Include="{listed}" /></ItemGroup></Project>', encoding="utf-8")
    return d


def test_draoi_file_missing_from_vcxproj_is_reported(tmp_path):
    d = _project(tmp_path, "Draoi", "DraoiX", "Known.cpp")
    (d / "Known.cpp").write_text("", encoding="utf-8")
    (d / "New.cpp").write_text("", encoding="utf-8")
    missing = _check_vcxproj_sync(tmp_path, [d / "Known.cpp", d / "New.cpp"])
    assert missing == ["New.cpp not in DraoiX.vcxproj"]


def test_file_outside_code_roots_is_ignored(tmp_path):
    d = _project(tmp_path, "Tools", "Dia", "Known.cpp")  # path contains "Dia" but root is Tools
    (d / "New.cpp").write_text("", encoding="utf-8")
    assert _check_vcxproj_sync(tmp_path, [d / "New.cpp"]) == []
