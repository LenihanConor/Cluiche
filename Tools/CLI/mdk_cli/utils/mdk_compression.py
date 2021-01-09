import os
import zipfile
import pathlib

from typing import Union, Optional
from loguru import logger


ZIP_UNIX_SYSTEM = 3


class mdkZipFile(zipfile.ZipFile):
    """!ZipFile instance that fixes the file permissions of files after it extracts them."""

    # noinspection SpellCheckingInspection,PyPep8Naming
    def __init__(self, file, mode="r", compression=zipfile.ZIP_STORED, allow_zip_64=True,
                 compresslevel=None, *, strict_timestamps=True):
        super().__init__(file, mode, compression, allow_zip_64, compresslevel, strict_timestamps=strict_timestamps)

    # override _extract_member to preserve file permissions:
    # http://bugs.python.org/issue15795
    def extract(self, member: Union[str, zipfile.ZipInfo],
                path: Union[str, pathlib.Path] = None,
                pwd: Union[str, pathlib.Path] = None):
        if path is None:
            path = pathlib.Path.cwd()
        if not isinstance(member, zipfile.ZipInfo):
            member = self.getinfo(member)

        ret_val = super()._extract_member(member, path, pwd)
        attr = member.external_attr >> 16 & 0x1FF
        os.chmod(ret_val, attr)
        return ret_val


def unzip_file_all(path: Union[str, pathlib.Path], output_path: Optional[Union[str, pathlib.Path]] = None,
                   delete_after: bool = False):
    if isinstance(path, str):
        path = pathlib.Path(path)
    if output_path is None:
        output_path = pathlib.Path.cwd()
    with mdkZipFile(path, "r") as zf:
        logger.info('Extracting files...')
        zf.printdir()
        # Pulled from zipfile.py
        for info in zf.infolist():
            extracted_path = zf.extract(info, output_path)

            if info.create_system == ZIP_UNIX_SYSTEM:
                unix_attributes = info.external_attr >> 16
                if unix_attributes:
                    os.chmod(extracted_path, unix_attributes)

    if delete_after:
        path.unlink(missing_ok=True)


def unzip_file(path: Union[str, pathlib.Path], zip_member: str, output_path: Optional[Union[str, pathlib.Path]] = None,
               delete_after: bool = False):
    if isinstance(path, str):
        path = pathlib.Path(path)
    if output_path is None:
        output_path = pathlib.Path.cwd()

    with mdkZipFile(path, "r") as zip_ref:
        zip_ref.extract(zip_member, path=output_path)

    if delete_after:
        path.unlink(missing_ok=True)
