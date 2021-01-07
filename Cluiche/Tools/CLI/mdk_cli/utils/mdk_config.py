from __future__ import annotations
import json
from pathlib import Path

import click

## @defgroup utils_grp utils
# A collection of useful utilities for use by all commands
#  @{

## @package mdk_config Configuration-related utilities.

CONFIG_FILENAME_TEMPLATE = 'mdk_{profile}_config.json'
CONFIG_KEY = 'config'


class Config:
    """!Contains configuration values for use by CLI commands.
    Also contains convenience methods for accessing many values.
    Currently provides access to `mdk_prime_config.json` but will be extended to other configurations as they arise.

    Note: All paths returned by this class are absolute pathlib.Path objects unless otherwise specified.
    """

    def __init__(self, value, root_path, cwd, **kwargs):
        self.value = value
        self._root_path = root_path
        self._cwd = cwd

    DEFAULT_FEATURE_CONFIG_FILENAME = 'mdk_config.json'
    prime_config_filename = CONFIG_FILENAME_TEMPLATE.format(profile='prime')

    @staticmethod
    def from_context(ctx) -> Config:
        """!Gets the Config instance from a Click context.
        The Config object is set on the Context in cli_main.py.
        """
        return ctx.obj

    @staticmethod
    def from_mdk_context(mdk_context) -> Config:
        """!Gets the Config instance from the mdk context, which is Click's ctx.obj.
        The Config object is set on the Context in cli_main.py.
        """
        return mdk_context

    @classmethod
    def read_json(cls, file_path):
        """!Convenience method to read a json configuration from the given path.
        @param [in] file_path a pathlib.Path to the file.
        @returns a dict containing the parsed json or an empty dict if the file does not exist.
        """
        content = {}
        if file_path.exists():
            with open(str(file_path.resolve())) as f:
                content = json.load(f)
        return content

    def save(self):
        """!Overwrites the contents of `mdk_prime_config.json` with the contents of this Config object."""
        json_object = json.dumps(self.value, indent=4)
        file_path = self.prime_config()
        with open(str(file_path), "w") as outfile:
            outfile.write(json_object)

    def __getitem__(self, key):
        """!Retrieve a configuration value using bracket ([]) notation. E.g. `my_value = config["some_key"]`"""
        return self.value[key]

    def root_path(self):
        """!Path to the project's root folder."""
        return self._root_path.resolve()

    def cli_root_path(self):
        """!Path to the cli's folder (i.e. mdk-cli)."""
        return self.root_path().joinpath(self.roots()['cli']).resolve()

    def console_root_path(self):
        """!Path to the Console's folder (i.e. mdk-console)."""
        return self.root_path().joinpath(self.roots()['console']).resolve()

    def original_working_dir(self) -> Path:
        """!Path to the folder where the command was run."""
        return self._cwd

    def prime_config(self):
        """!Path to `mdk_prime_config.json`."""
        return self._root_path.joinpath(Config.prime_config_filename).resolve()

    def roots(self):
        """!Value contained in the prime config's `root` key."""
        return self.value['roots']

    def paths(self):
        """!Value contained in the prime config's `paths` key.
        Provides access to all configured (non-roots) paths.
        If you want a specific path, use the path(self, path_key) method instead.
        """
        return self.value['paths']

    def path(self, path_key):
        """!Retrieve a path based on its key in the prime config's `paths` dict.
        Prefer `mdk_paths.mdkPaths.path()` over this method to retrieve a specific path.
        @param [in] path_key key in the prime config's `paths` map.
        """
        return self._root_path.joinpath(self.paths()[path_key]).resolve()

    def set_path(self, path_key, path):
        """!Sets a path in the prime config's `paths` dict.
        @param [in] path_key the path's key in the `paths` map in mdk_prime_config.json
        @param [in] path the path value. This is always interpreted as relative to the project root.
        """
        self.paths()[path_key] = path

    def has_path(self, path_key):
        """!Checks to see whether the path for `path_key` has been configured in `mdk_prime_config.json`
        @param [in] path_key the path's key in the `paths` map in `mdk_prime_config.json`
        """
        return path_key in self.paths()


"""!Allows users to pass in the mdk config instance into a click commands without needing to convert the context"""
pass_mdk_config = click.make_pass_decorator(Config)

## @}
