import importlib.metadata


def test_version():
    version = importlib.metadata.version('mdk-cli')
    assert version == '0.1.0'
