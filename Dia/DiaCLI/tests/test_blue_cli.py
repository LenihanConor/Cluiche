import importlib.metadata
import pytest


@pytest.mark.skip(reason="mdk-cli is not installed as a package in this dev environment; MDK runs DiaCLI from source")
def test_version():
    version = importlib.metadata.version('mdk-cli')
    assert version == '0.1.0'
