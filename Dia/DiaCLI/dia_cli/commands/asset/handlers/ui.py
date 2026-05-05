from .default import DefaultAssetHandler


class UIHandler(DefaultAssetHandler):
    type_id = "ui"
    file_pattern = "*.ui.json"
