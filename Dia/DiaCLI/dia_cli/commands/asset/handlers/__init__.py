from ..registry import AssetHandlerRegistry
from .audio import AudioHandler
from .config import ConfigHandler
from .default import DefaultAssetHandler
from .entity import EntityHandler
from .folder import FolderHandler
from .sprite import SpriteHandler
from .stage import StageHandler
from .texture import TextureHandler
from .ui import UIHandler

__all__ = [
    "DefaultAssetHandler",
    "TextureHandler",
    "SpriteHandler",
    "AudioHandler",
    "ConfigHandler",
    "EntityHandler",
    "StageHandler",
    "UIHandler",
    "FolderHandler",
    "register_built_in_handlers",
]

_BUILT_IN_HANDLERS = [
    TextureHandler,
    SpriteHandler,
    AudioHandler,
    ConfigHandler,
    EntityHandler,
    StageHandler,
    UIHandler,
    FolderHandler,
]


def register_built_in_handlers(registry: AssetHandlerRegistry) -> None:
    """Register all 8 built-in type handlers with the given registry."""
    for handler_cls in _BUILT_IN_HANDLERS:
        registry.register(handler_cls())
