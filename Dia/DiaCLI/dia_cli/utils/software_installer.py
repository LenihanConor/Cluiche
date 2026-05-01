from dia_cli.utils.dia_cli_paths import DiaCLIPaths
from loguru import logger
from pathlib import Path
import requests
from requests_toolbelt import exceptions
from requests_toolbelt.downloadutils import stream
import shutil
from utils.dia_cli_compression import unzip_file_all


## @addtogroup utils_grp
#  @{

## @package software_installer Utilities for installing software.


class Installer:
    """!Downloads and installs (unzips) software into the configured `paths.external` folder.
    This class is pretty dumb right now and will be improved as we require different types of installations.
    """

    def __init__(self, config, **kwargs):
        """!
        @param [in] config a dia_cli_config.Config object
        """
        self._mdk_paths = DiaCLIPaths(config)

    def install(self, software_id, url_provider):
        """!Downloads and installs (unzips) software to its own folder in the `paths.external` location.
        @param [in] software_id used as name for installation subfolder
        @param [in] url_provider a callable function that returns the URL to download from
        """
        logger.info("Installing " + software_id)

        generated = Path('generated')
        if not generated.exists():
            generated.mkdir()

        resp = requests.get(url_provider(), stream=True)

        download_to = generated

        if not resp.headers.get('content-disposition'):
            download_to = generated.joinpath(software_id + '.zip')

        try:
            filename = stream.stream_response_to_file(resp, download_to)
        except exceptions.StreamingError as e:
            logger.error(e)
            raise e

        dest_folder = self._mdk_paths.external(software_id)
        unzip_file_all(filename, dest_folder)

        logger.info("Cleaning up")
        shutil.rmtree(generated)

        logger.info("Successfully installed " + software_id + " to " + str(dest_folder.resolve()))
        return dest_folder

## @}
