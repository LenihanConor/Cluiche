import os
import stat
import typing
import pathlib


def chmod_file(path: typing.Union[str, pathlib.Path], perm):
    if isinstance(path, pathlib.Path):
        path = str(path)
    os.chmod(path, perm)


def set_file_exe(path: typing.Union[str, pathlib.Path]):
    if isinstance(path, pathlib.Path):
        path = str(path)

    st = os.stat(path)
    chmod_file(path, st.st_mode | stat.S_IEXEC)
