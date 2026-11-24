import pytest
import click

from dia_cli.commands.scaffold.app_cmd import check_stage_type_clash


def test_stage_clash_detects_draoi_header(tmp_path):
    h = tmp_path / "Draoi" / "DraoiX" / "ThingModule.h"
    h.parent.mkdir(parents=True)
    h.write_text("", encoding="utf-8")
    with pytest.raises(click.ClickException, match="Draoi/DraoiX/ThingModule.h"):
        check_stage_type_clash(tmp_path, "Thing")


def test_stage_clash_still_detects_dia_and_passes_when_free(tmp_path):
    h = tmp_path / "Dia" / "DiaX" / "OtherModule.h"
    h.parent.mkdir(parents=True)
    h.write_text("", encoding="utf-8")
    with pytest.raises(click.ClickException):
        check_stage_type_clash(tmp_path, "Other")
    check_stage_type_clash(tmp_path, "Free")
