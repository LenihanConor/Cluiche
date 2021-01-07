from loguru import logger
import subprocess
import toml

## @addtogroup setup_grp
#  @{

## @package executable_cmd Implementations for `setup executable` sub-commands

def customize_name(mdk_paths, name):
    """!Sets a custom name for the cli executable.
    Creates alias by adding an entry to `[tool.poetry.scripts]` in `pyproject.toml` and rerunning `poetry install`
    @param [in] mdk_paths a mdk_paths.mdkPaths object
    @param [in] name the name of the new custom alias   
    """
    _add_to_pyproject_toml(mdk_paths, name)
    _poetry_install(mdk_paths)
    logger.info(f"Finished setting executable name to {name}")


def _add_to_pyproject_toml(mdk_paths, name):
    logger.info("Adding poetry.scripts entry to pyproject.toml")
    pyproject = toml.load(mdk_paths.pyproject_toml())
    poetry_scripts = pyproject['tool']['poetry']['scripts']
    run_command = poetry_scripts['mdk']
    poetry_scripts[name] = run_command
    with open(mdk_paths.pyproject_toml(), 'w') as f:
        toml.dump(pyproject, f)


def _poetry_install(mdk_paths):
    logger.info("Reinitializing Poetry to add new entry point.")
    subprocess.run(['poetry', 'install'], cwd=mdk_paths.cli())

## @}
