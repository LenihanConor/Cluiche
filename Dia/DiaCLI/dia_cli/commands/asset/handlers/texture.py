from .default import DefaultAssetHandler


class TextureHandler(DefaultAssetHandler):
    type_id = "texture"
    file_pattern = "*.texture.png"
