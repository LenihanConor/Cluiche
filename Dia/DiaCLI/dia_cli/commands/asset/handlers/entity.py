from .default import DefaultAssetHandler


class EntityHandler(DefaultAssetHandler):
    type_id = "entity"
    file_pattern = "*.entity.json"
