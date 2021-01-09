from pathlib import Path
from mdk_cli.utils.mdk_config import Config

## @addtogroup utils_grp
#  @{

## @package mdk_paths Path-related utilities.


class mdkPaths():
    """!Provides access to common mdk paths, returned as absolute, resolved pathlib.Path objects.
    Note that only public methods should call Path.resolve() before
    returning, and only when they need to.
    """
    def __init__(self, config, **kwargs):
        """!
        @param config a mdk_config.Config object
        """
        ## A mdk_config.Config object, to which this class delegates most of its responsibilities.
        self.config = config

    @staticmethod
    def from_mdk_context(mdk_context):
        """!Gets the mdkPaths instance from a mdk_context, which is Click's ctx.obj.
        """
        return mdkPaths(mdk_context)

    def root(self) -> (Path):
        """!Get the project's root folder as a Path object"""
        return self.config.root_path()

    def owd(self) -> (Path):
        """!Get the original working directory, as determined at startup time by `Path.cwd()`. I.e. the
        folder the command was called from. See `cli_main.py` to see how it is set.
        """
        return self.config.original_working_dir().resolve()

    def external(self, subfolder='') -> (Path):
        """!Get the folder where external tools/libraries are installed,
        which is `paths.external` in `mdk_prime_config.json`.
        @param [in] subfolder Optional. Will append this subfolder to the path."""
        return self._external().joinpath(subfolder).resolve()

    def _external(self) -> (Path):
        return self.config.path('external')

    def path(self, path_key) -> (Path):
        """!Retrieve a path based on its key in the prime config's `paths` dict.
        Prefer this way of getting paths over `mdk_config.Config.path()`.
        @param [in] path_key key in the prime config's `paths` map.
        """
        return self.config.path(path_key)

    def cli(self) -> (Path):
        """!Get the base folder of the mdk cli (mdk-cli)"""
        return self.config.cli_root_path()

    def console(self) -> (Path):
        """!Get the base folder of the mdk cli (mdk-cli)"""
        return self.config.console_root_path()

    def dot_mdk(self) -> (Path):
        """!Get the .mdk hidden folder path

        @return absolute pathlib.Path to folder
        """
        return self.config.root_path().joinpath(".mdk").resolve()

    def data(self) -> (Path):
        """!Get the .mdk hidden folder"""
        return self.dot_mdk().joinpath(".data").resolve()

    def prime_config(self) -> (Path):
        """!Get the path to mdk_prime_config.json"""
        return self.config.prime_config()

    def pyproject_toml(self) -> (Path):
        """!Get the path to the project's Poetery configuration file, pyproject.toml."""
        return self.config.root_path().joinpath('pyproject.toml').resolve()

## @}
