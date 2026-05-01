"""Bridge to invoke C++ DiaAPI commands from Python.

This module provides a Python interface to execute C++ DiaAPI commands
via DiaPython bindings when available.
"""
from loguru import logger
from typing import Optional, List, Dict
import sys


class DiaAPIBridge:
    """Bridge to C++ DiaAPI command system.

    Loads the C++ DiaAPI library via Python bindings and provides
    methods to discover and execute registered commands.
    """

    def __init__(self):
        """Initialize the bridge and attempt to load DiaAPI."""
        self._available = False
        self._dia_api = None
        self._load_diaapi()

    def _load_diaapi(self):
        """Attempt to load the C++ DiaAPI module via Python bindings."""
        try:
            # Try to import the DiaAPI Python module (created by DiaPython)
            import dia_api
            self._dia_api = dia_api
            self._available = True
            logger.debug("DiaAPI bridge loaded successfully")
        except ImportError as e:
            logger.warning(f"DiaAPI not available: {e}")
            logger.warning("C++ DiaAPI commands cannot be invoked from DiaCLI")
            self._available = False

    def is_available(self) -> bool:
        """Check if DiaAPI bridge is available.

        Returns:
            True if C++ DiaAPI can be invoked, False otherwise
        """
        return self._available

    def list_commands(self) -> List[str]:
        """List all registered DiaAPI commands.

        Returns:
            List of command names registered in C++ DiaAPI
        """
        if not self._available:
            logger.error("DiaAPI not available - cannot list commands")
            return []

        try:
            # Call C++ function to get list of commands
            commands = self._dia_api.list_commands()
            return commands
        except AttributeError:
            logger.error("DiaAPI module does not have 'list_commands' function")
            return []
        except Exception as e:
            logger.error(f"Error listing DiaAPI commands: {e}")
            return []

    def execute(self, command_name: str, args: Optional[List[str]] = None) -> int:
        """Execute a C++ DiaAPI command.

        Args:
            command_name: Name of the command to execute
            args: Optional list of command arguments

        Returns:
            Exit code from the command (0=success)
        """
        if not self._available:
            logger.error("DiaAPI not available - cannot execute commands")
            return 1

        if args is None:
            args = []

        try:
            # Call C++ function to execute command
            exit_code = self._dia_api.execute_command(command_name, args)
            return exit_code
        except AttributeError:
            logger.error("DiaAPI module does not have 'execute_command' function")
            return 1
        except Exception as e:
            logger.error(f"Error executing DiaAPI command '{command_name}': {e}")
            return 1

    def get_command_help(self, command_name: str) -> Optional[str]:
        """Get help text for a specific DiaAPI command.

        Args:
            command_name: Name of the command

        Returns:
            Help text string or None if not available
        """
        if not self._available:
            return None

        try:
            help_text = self._dia_api.get_command_help(command_name)
            return help_text
        except AttributeError:
            logger.debug("DiaAPI module does not have 'get_command_help' function")
            return None
        except Exception as e:
            logger.error(f"Error getting help for command '{command_name}': {e}")
            return None


# Singleton instance
_bridge_instance: Optional[DiaAPIBridge] = None


def get_bridge() -> DiaAPIBridge:
    """Get the singleton DiaAPI bridge instance.

    Returns:
        DiaAPIBridge instance
    """
    global _bridge_instance
    if _bridge_instance is None:
        _bridge_instance = DiaAPIBridge()
    return _bridge_instance
