# ------------------------------------------------------------
import sys
import enum
from typing import Optional


# ------------------------------------------------------------
class Platform(enum.IntFlag):
    Unknown = 0
    Windows = enum.auto()
    Darwin = enum.auto()
    Linux = enum.auto()
    Unix = Darwin | Linux


# ------------------------------------------------------------
def _get_platform(platform: Optional[Platform]) -> Platform:
    if platform is None:
        return get_running_platform()
    return platform


# ------------------------------------------------------------
def is_windows(platform: Optional[Platform] = None) -> bool:
    """
    :param platform: platform value to test against. If None, use the running platform
    :return: if the current platform is Windows
    :rtype: bool
    """
    return _get_platform(platform) == Platform.Windows


# ------------------------------------------------------------
def is_darwin(platform: Optional[Platform] = None) -> bool:
    """
    :param platform: platform value to test against. If None, use the running platform
    :return: if the current platform is Darwin (OSX)
    :rtype: bool
    """
    return _get_platform(platform) == Platform.Darwin


# ------------------------------------------------------------
def is_linux(platform: Optional[Platform] = None) -> bool:
    """
    :param platform: platform value to test against. If None, use the running platform
    :return: if the current platform is Linux
    :rtype: bool
    """
    return _get_platform(platform) == Platform.Linux


# ------------------------------------------------------------
def is_unix(platform: Optional[Platform] = None) -> bool:
    """
    :param platform: platform value to test against. If None, use the running platform
    :return: if the current platform is based on Unix
    :rtype: bool
    """
    current_platform = _get_platform(platform)
    return (current_platform & Platform.Unix) != Platform.Unknown


# ------------------------------------------------------------
def is_unknown(platform: Optional[Platform] = None) -> bool:
    """
    :param platform: platform value to test against. If None, use the running platform
    :return: if the current platform is not recognized
    :rtype: bool
    """
    return _get_platform(platform) == Platform.Unknown


# ------------------------------------------------------------
def get_running_platform() -> Platform:
    if sys.platform.startswith("win"):
        return Platform.Windows
    elif "linux" in sys.platform:
        return Platform.Linux
    elif "darwin" in sys.platform:
        return Platform.Darwin
    else:
        return Platform.Unknown


# ------------------------------------------------------------
def get_running_platform_name() -> str:
    running_platform = get_running_platform()
    return platform_to_str(running_platform)


# ------------------------------------------------------------
def platform_to_str(platform: Platform) -> str:
    if platform == Platform.Windows:
        return "Windows"
    elif platform == Platform.Darwin:
        return "Darwin"
    elif platform == Platform.Linux:
        return "Linux"
    else:
        return "Unknown"


# ------------------------------------------------------------
def platform_to_str_lower(platform: Platform) -> str:
    return platform_to_str(platform).lower()


# ------------------------------------------------------------
def str_to_platform(platform_str: str) -> Platform:
    platform_map = {
        "windows": Platform.Windows,
        "win": Platform.Windows,
        "win32": Platform.Windows,
        "win64": Platform.Windows,
        "darwin": Platform.Darwin,
        "osx": Platform.Darwin,
        "linux": Platform.Linux,
        "linux2": Platform.Linux
    }
    platform_str = platform_str.lower()
    if platform_str in platform_map:
        return platform_map[platform_str]
    return Platform.Unknown


# ------------------------------------------------------------
def get_running_platform_name_lower() -> str:
    running_platform = get_running_platform()
    return platform_to_str_lower(running_platform)


# ------------------------------------------------------------
def get_executable_name(executable):
    if is_windows():
        executable += '.exe'
    return executable


def get_script_name(script):
    if is_windows():
        script += '.bat'
    return script
