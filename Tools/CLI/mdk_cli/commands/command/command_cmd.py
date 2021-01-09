from loguru import logger
from pathlib import Path
from mdk_cli.utils.mdk_paths import mdkPaths
from mdk_cli.utils.mdk_template import CodeGenTemplate


## @addtogroup command_grp
#  @{

## @package command_cmd Implementations for `command` sub-commands

TEMPLATE_FOLDER = "mdk_cli/templates"
TREE_TEMPLATE_FILE = "command.tree_template.json"


def create_command(mdk_paths: mdkPaths, command_name: str, module_path: Path):
    """!Creates a new top-level mdk cli command.

    New command is called `command_name` and its base folder is `module_path`.

    Creates a `<module_path>/cli/<command_name>.py` file containing the Click command declaration.
    Also creates an implementation file, `<module_path>/commands/<command_name>_cmd.py`

    @param [in] mdk_paths a mdk_paths.mdkPaths object
    @param [in] command_name the name of the new command
    @param [in] module_path the `Path` where the command will live, relative to the project root.
    """
    template_path = mdk_paths.cli().joinpath(TEMPLATE_FOLDER).resolve()

    logger.debug(f"Creating command from templates folder: {template_path}")

    template = CodeGenTemplate(
        templates_folder_path=template_path,
        tree_template_filename=TREE_TEMPLATE_FILE,
        template_params={'command_name': command_name},
        dest_folder=module_path
    )

    template.generate()

    logger.info(f"Created command '{command_name}' in {str(module_path.resolve())}")


def _command_exists(name, path):
    # FIXME Implement this
    return False

## @}
