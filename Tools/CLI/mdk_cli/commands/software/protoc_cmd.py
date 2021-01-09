from mdk_cli.utils.software_installer import Installer
from mdk_cli.utils import mdk_chmod
from mdk_cli.utils import mdk_platform
from functools import partial
from loguru import logger
import platform
from string import Template

## @addtogroup software_grp
#  @{

## @package protoc_cmd Implementations for `software protoc` sub-command

SOFTWARE_ID = 'protoc'
SOFTWARE_VERSION = '3.12.0'
COMPILER_TEMPLATE = Template('https://github.com/protocolbuffers/protobuf/releases/download/v${version}/protoc-${version}-${arch}.zip')
IMPLEMENTATION_TEMPLATE = Template('https://github.com/protocolbuffers/protobuf/releases/download/v${version}/protobuf-${language}-${version}.zip')

_arch_by_os = {
    'Darwin': 'osx-x86_64',
    'Windows': 'win64',
    'Linux': 'linux-x86_64'}


def execute(config):
    """!Installs the `protoc` Protobuf compiler into configured `paths.external` folder.
    @param [in] config a mdk_config.Config object
    """
    logger.info('Installing Protobuf compiler')
    dest_folder = Installer(config).install(SOFTWARE_ID, _compiler_url_provider)
    logger.info(f"protoc installed at: {dest_folder}")

    # Leaving these here in case we decide to build from source instead.
    # Installer(config).install('protobuf_python', _impl_url_provider('python'))
    # Installer(config).install('protobuf_csharp', _impl_url_provider('csharp'))
    # Installer(config).install('protobuf_java', _impl_url_provider('java'))


def _compiler_url_provider():
    """!Creates an os-specific download URL for the `protoc` compiler"""
    arch = _arch_by_os[platform.system()]
    params = dict(version=SOFTWARE_VERSION, arch=arch)
    return COMPILER_TEMPLATE.substitute(params)


def _implementation_url_provider(language):
    params = dict(version=SOFTWARE_VERSION, language=language)
    return IMPLEMENTATION_TEMPLATE.substitute(params)


def _impl_url_provider(language):
    return partial(_implementation_url_provider, language)

## @}
