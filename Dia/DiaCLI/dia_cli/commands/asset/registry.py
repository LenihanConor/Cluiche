from __future__ import annotations

from .handler import AssetHandler


class AssetHandlerRegistry:
    def __init__(self) -> None:
        self._handlers: dict[str, AssetHandler] = {}

    def register(self, handler: AssetHandler) -> None:
        if handler.type_id in self._handlers:
            raise ValueError(f"Duplicate handler for asset type '{handler.type_id}'")
        self._handlers[handler.type_id] = handler

    def get(self, type_id: str) -> AssetHandler | None:
        return self._handlers.get(type_id)
