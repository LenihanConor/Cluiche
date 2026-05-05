from .default import DefaultAssetHandler


class SpriteHandler(DefaultAssetHandler):
    type_id = "sprite"
    file_pattern = "*.sprite.json"
