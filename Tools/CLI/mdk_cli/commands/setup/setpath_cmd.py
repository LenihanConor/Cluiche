from loguru import logger


## @addtogroup setup_grp
#  @{

## @package setpath_cmd Implementations for `setup paths` command and sub-commands

def show_paths(config):
    """!Prints a list of all configured paths in mdk_prime_config.json
    @param [in] config a mdk_config.Config object
    """
    _print_all_paths(config)


def _print_paths(items):
    paths = []
    for num, (path_id, path) in enumerate(items, start=1):
        paths.append(f"{num}) '{path_id}': '{path}'")
    return paths


def _print_all_paths(config):
    msg = ["\nRoot Paths:"]
    msg.extend(_print_paths(config.roots().items()))

    msg.append("Relative Paths:")
    msg.extend(_print_paths(config.paths().items()))
    logger.info("\n".join(msg))


def set_path(config, path_key, path):
    """!Sets a path with the given key in `mdk_prime_config.json` to `path`
    @param [in] config a mdk_config.Config object
    @param [in] path_key the path's key in the `paths` map in mdk_prime_config.json
    @param [in] path the path value. This is always interpreted as relative to the project root.
    """
    if path_key.startswith("roots.base"):
        logger.info("Changing roots.base is not allowed because it "
                    "must always be the project's root folder.")
        return
    if path_key.startswith("roots."):
        _set_roots_path(config, path_key, path)
    else:
        config.set_path(path_key, path)
    config.save()
    _print_all_paths(config)


def _set_roots_path(config, path_key, path):
    key = path_key.split('.', 1)[1]
    config.roots()[key] = path

## @}
