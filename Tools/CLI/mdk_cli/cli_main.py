import os
import click
import importlib
import importlib.machinery
from loguru import logger
from pathlib import Path
import sys
from mdk_cli.utils.mdk_config import Config


## @defgroup cli_main_grp cli_main
# The cli's main module
# @{

## Default list of folders to ignore when searching for commands.
# FIXME use a configurable ignore list?
IGNORED = ['.venv', 'dist', 'node_modules', 'tests']
CLI_CMD_PREFIX = 'cli_'


def _generate_sys_path():
    """Searches for folders named 'cli' and add those folders' parents as packages."""
    def _find_cli_packages(root_path, package_name: str = 'cli'):
        packages = []
        for root, dirs, _ in os.walk(str(root_path), followlinks=True):
            dirs[:] = [d for d in dirs if d not in IGNORED]
            if package_name not in dirs:
                continue
            packages.append(Path(root) / package_name)

        return packages

    for path in _find_cli_packages(_root_path):
        parent = path.parent
        # This will remove entries that look like cli/cli so we don't have duplicate modules.
        # Only the parent of the deepest 'cli/' will be be a package. This is the behavior that
        # would be least surprising.
        if parent.name != "cli":
            sys.path.append(str(parent.resolve()))


def _remove_cli_cmd_prefix(cmd_name: str) -> str:
    if cmd_name.startswith(CLI_CMD_PREFIX):
        return cmd_name[len(CLI_CMD_PREFIX):]
    return cmd_name


# FIXME When we are distributing as a package, we should always distribute with a mdk_prime_config.json.
# At that time we can get rid of any handling of it being absent.
# See https://jaas.ea.com/browse/PBLU-298
_prime_config_present = False
_root_path = Path.cwd()
_cwd_and_ancestors = [Path.cwd()]
_cwd_and_ancestors.extend(Path.cwd().parents)
for parent in _cwd_and_ancestors:
    if any(parent.glob(Config.prime_config_filename)):
        _prime_config_present = True
        _prime_config_file_path = parent.joinpath(Config.prime_config_filename)
        _root_path = parent
        _prime_config = Config.read_json(_prime_config_file_path)
        break


if _prime_config_present:
    # We don't need to look for other modules if we're only allowing 'setup' to be run.
    _generate_sys_path()


class mdkCLI(click.MultiCommand):
    """!Custom Click MultiCommand that is the thrust of the mdk cli.
    It executes a command as follows:
    1. searches for python files matching the pattern `cli/**/*.py`
    2. looks for a file whose stem matches the invoked command's name
    3. loads and executes the file as a module
    4. calls function called `cli()` in the module

    See [Click's documentation](https://click.palletsprojects.com/en/7.x/commands/#custom-multi-commands) for more details. # noqa
    """

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # a dict mapping module names (i.e. top-level commands) to their paths
        self._module_paths = {}
        # Need to import here to avoid the isinstance from returning false from within the click code
        from utils.mdk_config import Config
        self.context_settings['obj'] = Config(value=_prime_config, root_path=_root_path, cwd=Path.cwd())

    def list_commands(self, ctx):
        """!Click API implementation to retrieve a list of all available commands.
        @param [in] ctx The Click context
        """
        keys = [_remove_cli_cmd_prefix(k) for k in list(self._find_modules().keys())]
        keys.sort()
        return keys

    def get_command(self, ctx, name):
        """!Click API implementation to load a command module lazily.
        Note that the top-level command function's name must be 'cli'
        @param [in] ctx The Click context
        @param [in] name The name of the top-level command
        """
        try:
            modules = self._find_modules()
            mod = _load_module_as_command(name, modules[name])
        except KeyError:
            raise click.UsageError(f"No such command: {name}.", ctx)
        except ImportError as ie:
            logger.error(f"Error importing command. Reason: {ie}")
            logger.error("Run 'mdk --help' for a list of valid commands.")
        try:
            cli_cmd = getattr(mod, 'cli')
            self.clean_doc_str(ctx, cli_cmd)
            return cli_cmd
        except AttributeError as ae:
            logger.error(f"Error loading '{name}' module from {modules[name]}")
            logger.error(f"Reason: {ae}")
            logger.error("Do you need to run 'poetry install'?")

    def _find_modules(self):
        """Searches for CLI plugin modules recursively, starting at the project root."""
        def _find_cli_modules(root_path):
            modules = []
            for root, dirs, files in os.walk(str(root_path), followlinks=True):
                dirs[:] = [d for d in dirs if d not in IGNORED]
                root_path = Path(root)
                if 'cli' not in root_path.parts:
                    continue
                modules += [root_path / f for f in files if f.endswith(".py") and not f.startswith('_')]

            return modules

        if not self._module_paths:
            for path in _find_cli_modules(_root_path):
                module_name = path.stem
                module_name = _remove_cli_cmd_prefix(module_name)
                self._module_paths[module_name] = path.resolve()
        return self._module_paths

    def clean_doc_str(self, ctx, c):
        def clean(cmd):
            doc = cmd.help
            if doc is None:
                return

            cmd.help = doc.lstrip("!").lstrip()

        clean(c)
        cmd_names = c.list_commands(ctx)
        for cmd_name in cmd_names:
            child_cmd = c.get_command(ctx, cmd_name)
            if hasattr(child_cmd, 'list_commands') and hasattr(child_cmd, 'get_command'):
                self.clean_doc_str(ctx, child_cmd)
            else:
                clean(child_cmd)


class OnlySetupCli(click.MultiCommand):
    """!A special command containing only `setup`. Used when it looks like no project exists
    yet. I.e. there is no `mdk_prime_config.json` present in cwd's ancestor folders."""

    def list_commands(self, ctx):
        return ["setup"]

    def get_command(self, ctx, name):
        # FIXME How will we handle this with packaging??
        setup_module_singleton_list = [str(pp) for pp in Path.cwd().glob("**/mdk_cli/cli/setup.py")]
        print(Path.cwd())
        # setup_module_singleton_list = list(glob(f"{Path.cwd()}/**/mdk-cli/mdk_cli/cli/setup.py"))
        print(f"SMSL: {setup_module_singleton_list}")
        if not setup_module_singleton_list:
            logger.error("mdk_cli/cli/setup.py not found in the folder tree. Cannot run setup.")
            exit(1)
        mod = _load_module_as_command(name, Path(setup_module_singleton_list[0]).resolve())
        return getattr(mod, 'cli')


def _load_module_as_command(name: str, module_path: Path):
    if module_path.stem.startswith(CLI_CMD_PREFIX):
        name = f'{CLI_CMD_PREFIX}{name}'
    loader = importlib.machinery.SourceFileLoader(name, str(module_path))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    loader.exec_module(mod)
    return mod


## A reference to our custom Click multi-command. Invoked by `main()`.
if _prime_config_present:
    cli = mdkCLI(name='mdk', help="mdk's command line interface.")
else:
    cli = OnlySetupCli(name='mdk', help="mdk's command line interface.")

if __name__ == '__main__':
    cli()


def main():
    """The cli entry point."""
    cli()

## @}
