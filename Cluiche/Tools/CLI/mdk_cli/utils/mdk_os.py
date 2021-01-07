import os
import sys
import math
import tempfile
import importlib
import contextlib
import pathlib
import shutil

from typing import Union

from loguru import logger

PathType = Union[str, pathlib.Path]


#TODO: rename to remove camel case
def createDirectory(path):
    if os.path.exists(path):
        logger.debug(f'Folder already exists at: {str(path)}')
    else:
        os.makedirs(path)
        logger.debug(f'Created folder at: {str(path)}')


#TODO: rename to remove camel case
def deleteDirectoryAndFiles(path):
    if os.path.exists(path):
        shutil.rmtree(path)
        logger.debug(f'Deleting directory and contents at: {str(path)}')


#TODO: rename to remove camel case
def getAllDirectoriesAtPath(path):
    folders = []

    # r=root, d=directories, f = files
    for r, d, f in os.walk(path):
        for folder in d:
            folders.append(os.path.join(r, folder))

    return folders


def copy_path(src_path: PathType, dst_path: PathType, exists_ok: bool = False):
    src_path = pathlib.Path(src_path)
    if not src_path.exists():
        logger.warning(f"Path does not exist trying to delete: {src_path}")
        return

    dst_path = pathlib.Path(dst_path)
    if dst_path.exists():
        if exists_ok:
            delete_path(dst_path)
        else:
            logger.warning(f"Path exists trying to copy: {dst_path}")
            return

    if src_path.is_dir():
        shutil.copytree(str(src_path), str(dst_path))
        return
    elif src_path.is_file() or src_path.is_symlink():
        shutil.copyfile(str(src_path), str(dst_path))


def delete_path(path: PathType):
    path = pathlib.Path(path)
    if not path.exists():
        logger.warning(f"Path does not exist trying to delete: {path}")
        return

    if path.is_file():
        path.unlink()
        return

    for sub_path in path.iterdir():
        delete_path(sub_path)
    path.rmdir()


@contextlib.contextmanager
def chdir(d):
    cwd = os.getcwd()
    try:
        os.chdir(d)
        yield
    finally:
        os.chdir(cwd)


@contextlib.contextmanager
def temp_directory():
    """!A context manager that creates a temporary folder and returns the path.
    Once it goes out of scope the directory is deleted
    """
    t = tempfile.mkdtemp()
    try:
        yield pathlib.Path(t)
    finally:
        try:
            shutil.rmtree(t)
        except (OSError, IOError):  # noqa: B014
            pass


@contextlib.contextmanager
def import_module_path(module_name: str, module_path: PathType):
    """! Imports a module from the `module_path` and returns the loaded ModuleType

    @param [in] string of the module name to import
    @param [in] the absolute path to the module file

    Example:
        with import_module_path(script_name, script.resolve()) as mod:
            ...
        # module will be unloaded if it wasn't there before.
    """
    module_path = str(module_path)
    loader = importlib.machinery.SourceFileLoader(module_name, module_path)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    mod = importlib.util.module_from_spec(spec)
    loader.exec_module(mod)
    yield mod


def can_use_symlinks():
    """! Returns if the system can use symlinks or Junctions Windows

    This can happen on Windows if the workstation does not have Developer mode enabled.
    https://blogs.windows.com/windowsdeveloper/2016/12/02/symlinks-windows-10/
    """
    from utils.mdk_platform import is_windows
    if is_windows():
        if sys.getwindowsversion()[:2] >= (6, 0):
            tmp_folder = pathlib.Path(tempfile.mkdtemp())
            # noinspection PyBroadException
            try:
                tmp_file = tmp_folder / 'tmp'
                tmp_link_file = tmp_folder / 'tmp_link'
                tmp_link_file.symlink_to(tmp_file)
                return tmp_link_file.is_symlink()
            except:
                return False
            finally:
                try:
                    shutil.rmtree(str(tmp_folder))
                except (OSError, IOError):  # noqa: B014
                    pass
        else:
            return False
    else:
        # Linux and Darwin can use symlinks by default.
        return True


def is_pytest_running() -> bool:
    """!Checks to see if pytest is currently running the project"""
    return 'pytest' in sys.modules and 'PYTEST_CURRENT_TEST' in os.environ


def sizeof_file_fmt(filepath: PathType, suffix: str = "B", use_bits: bool = True, show_bits: bool = False) -> str:
    if isinstance(filepath, str):
        filepath = pathlib.Path(filepath)

    file_size = filepath.stat().st_size
    return sizeof_fmt(file_size, suffix, use_bits, show_bits)


def sizeof_fmt(num: int, suffix: str = "B", use_bits: bool = False, show_bits: bool = False) -> str:
    kilobyte_size = 1024
    if num == 0:
        return '0 {}'.format(suffix)

    def get_magnitude(n):
        order = 0
        n = math.fabs(n)
        if use_bits:
            n /= 8
        while n >= kilobyte_size:
            n /= kilobyte_size
            order += 1
        return n, order

    val, magnitude = get_magnitude(num)
    if magnitude > 7:
        notation = 'Y'
    else:
        notation = ["", "K", "M", "G", "T", "P", "E", "Z"][magnitude]
    output = f'{f"{val:3.1f}".rstrip(".0")}{notation}{suffix}'
    if show_bits:
        output = f"{output} ({num} bits)"

    return output
