from glob import glob
from loguru import logger
import shutil
from blue_cli.utils.blue_paths import BluePaths
from blue_cli.utils import shell_utils


## @addtogroup console_grp
#  @{

## @package console_cmd Implementations for `console` commands

async def detect_changes():
    pass


async def handle_changes():
    await detect_changes()


def execute(blue_context):
    """!Implementation of `console` command

    @param [in] blue_context blue-specific context, whose value is Click's ctx.obj
    """
    blue_paths = BluePaths.from_blue_context(blue_context)
    source_folders = []
    dest_folder = blue_paths.console().joinpath("blue-console-app/src/.plugins").resolve()
    glob_pattern = f"{blue_paths.root()}/**/console/src"
    logger.debug(f"Searching for plugins matching pattern: {glob_pattern}")
    for src_folder in list(glob(f"{blue_paths.root()}/**/console/src", recursive=True)):
        source_folders.append(src_folder)
        shutil.copytree(src_folder, dest_folder, dirs_exist_ok=True)
    logger.debug(f"Copied plugins {source_folders} into {dest_folder}")

    cwd = blue_paths.console().joinpath('blue-console-app')
    shell_utils.shell_run('npm start', cwd=cwd)

## @}
