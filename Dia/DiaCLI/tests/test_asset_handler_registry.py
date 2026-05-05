"""Unit tests for AssetHandlerRegistry."""
import pytest

from dia_cli.commands.asset.handler import AssetError, AssetHandler, DeployResult, TransformResult
from dia_cli.commands.asset.registry import AssetHandlerRegistry


class _TextureHandler(AssetHandler):
    type_id = "texture"


class _AudioHandler(AssetHandler):
    type_id = "audio"


class _DuplicateTextureHandler(AssetHandler):
    type_id = "texture"


# ---------------------------------------------------------------------------
# register / get
# ---------------------------------------------------------------------------

def test_register_and_get_returns_handler():
    reg = AssetHandlerRegistry()
    h = _TextureHandler()
    reg.register(h)
    assert reg.get("texture") is h


def test_get_unknown_type_returns_none():
    reg = AssetHandlerRegistry()
    assert reg.get("texture") is None


def test_register_multiple_types():
    reg = AssetHandlerRegistry()
    t = _TextureHandler()
    a = _AudioHandler()
    reg.register(t)
    reg.register(a)
    assert reg.get("texture") is t
    assert reg.get("audio") is a


def test_register_duplicate_raises():
    reg = AssetHandlerRegistry()
    reg.register(_TextureHandler())
    with pytest.raises(ValueError, match="Duplicate handler for asset type 'texture'"):
        reg.register(_DuplicateTextureHandler())


def test_get_after_duplicate_rejected_still_returns_original():
    reg = AssetHandlerRegistry()
    original = _TextureHandler()
    reg.register(original)
    try:
        reg.register(_DuplicateTextureHandler())
    except ValueError:
        pass
    assert reg.get("texture") is original


# ---------------------------------------------------------------------------
# AssetHandler base defaults
# ---------------------------------------------------------------------------

def test_base_handler_validate_returns_empty():
    h = AssetHandler()
    assert h.validate({}, None) == []


def test_base_handler_transform_returns_success():
    result = AssetHandler().transform({}, None)
    assert isinstance(result, TransformResult)
    assert result.success is True
    assert result.output_path is None
    assert result.errors == []


def test_base_handler_deploy_returns_success():
    result = AssetHandler().deploy({}, None)
    assert isinstance(result, DeployResult)
    assert result.success is True
    assert result.deploy_path is None
    assert result.errors == []


# ---------------------------------------------------------------------------
# AssetError dataclass
# ---------------------------------------------------------------------------

def test_asset_error_fields():
    err = AssetError(asset_id="texture.foo", phase="validate", message="missing file")
    assert err.asset_id == "texture.foo"
    assert err.phase == "validate"
    assert err.message == "missing file"


def test_transform_result_with_errors():
    err = AssetError(asset_id="texture.foo", phase="transform", message="bad data")
    result = TransformResult(success=False, errors=[err])
    assert result.success is False
    assert len(result.errors) == 1


def test_deploy_result_with_path():
    result = DeployResult(success=True, deploy_path="stages/menu/misc/bg.png")
    assert result.deploy_path == "stages/menu/misc/bg.png"
