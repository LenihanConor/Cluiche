import pathlib
from typing import Union, Optional, List, Any

import invoke
from loguru import logger


def shell_run(
        cmd: Union[str, List[Any]],
        cwd: Optional[Union[str, pathlib.Path]] = None,
        **kwargs) -> invoke.runners.Result:
    if cwd is None:
        cwd = str(pathlib.Path.cwd())

    if isinstance(cmd, list):
        cmd = [str(v) for v in cmd if v]
        cmd = " ".join(cmd)

    context = invoke.Context()
    with context.cd(str(cwd)):
        result = context.run(cmd, warn=True, **kwargs)
        if result.failed:
            logger.error(f'{cmd}\n{result.stderr}')
    return result
