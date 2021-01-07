import json
from loguru import logger
from pathlib import Path
from mdk_cli.utils.mdk_config import Config


## @addtogroup setup_grp
#  @{

## @package setup_cmd Implementations for `setup` commands


def execute(ctx_obj):
    """!Implementation of `setup` command

    Does the following:
    1. creates a `mdk_prime_config.json` project configuration file in the cwd if it does not already exist.

    @param [in] ctx_obj the Click context's obj member
    """
    _create_prime_config(Path.cwd())


# FIXME convert the content to a config object and use its save() method instead.
def _create_prime_config(root_folder):
    """!Creates a primary project configuration file in the specified folder."""
    filename = Config.prime_config_filename
    file_path = root_folder.joinpath(filename)
    if not file_path.exists():
        file_path_string = str(file_path.resolve())
        logger.info("Creating config file: " + file_path_string)
        content = _prime_config_content()
        json_object = json.dumps(content, indent=4)

        with open(file_path_string, "w") as outfile:
            outfile.write(json_object)


def _prime_config_content():
    """Defines the default primary configuration file content"""
    config = {
        'name': 'NotImplemented',
        'roots':
        {
            'base': '.',
            'cli': 'mdk_cli'
        },
        'paths':
        {
            'external': 'mdk/external'
        }
    }
    return config

## @}
