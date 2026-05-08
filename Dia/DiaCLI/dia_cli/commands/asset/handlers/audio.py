from .default import DefaultAssetHandler


class AudioHandler(DefaultAssetHandler):
    type_id = "audio"
    file_pattern = "*.audio.wav"
