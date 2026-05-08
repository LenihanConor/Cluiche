from .default import DefaultAssetHandler


class ConfigHandler(DefaultAssetHandler):
    type_id = "config"
    file_pattern = "*.config.json"
