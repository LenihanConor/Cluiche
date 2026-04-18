"""Unit tests for DiaAPI bridge feature."""
import pytest
from unittest.mock import Mock, patch
from dia_cli.utils.diaapi_bridge import DiaAPIBridge


def test_bridge_graceful_fallback_when_unavailable():
    """AC5: If DiaAPI not available, bridge logs warning and skips"""
    bridge = DiaAPIBridge()
    assert bridge.is_available() == False


def test_bridge_detects_availability():
    """AC4: Bridge loads DiaAPI.dll/.so dynamically at runtime"""
    with patch('dia_cli.utils.diaapi_bridge.logger') as mock_logger:
        bridge = DiaAPIBridge()
        # Without dia_api module, should warn
        assert mock_logger.warning.called
        assert not bridge.is_available()


@patch('dia_cli.utils.diaapi_bridge.sys.modules')
def test_bridge_with_mock_diaapi(mock_modules):
    """AC1-AC3: Bridge can invoke C++ DiaAPI when available"""
    # Create mock dia_api module
    mock_dia_api = Mock()
    mock_dia_api.list_commands.return_value = ['validate-assets', 'compile-shaders']
    mock_dia_api.execute_command.return_value = 0
    mock_dia_api.get_command_help.return_value = "Help text for command"

    with patch('builtins.__import__', return_value=mock_dia_api):
        bridge = DiaAPIBridge()

        # Should be available
        assert bridge.is_available()

        # AC2: List commands
        commands = bridge.list_commands()
        assert 'validate-assets' in commands
        assert 'compile-shaders' in commands

        # AC3: Execute command
        exit_code = bridge.execute('validate-assets', ['--path', 'Assets/'])
        assert exit_code == 0
        mock_dia_api.execute_command.assert_called_once_with('validate-assets', ['--path', 'Assets/'])

        # Test get help
        help_text = bridge.get_command_help('validate-assets')
        assert help_text == "Help text for command"


def test_bridge_handles_missing_functions():
    """Bridge handles case where dia_api module exists but lacks expected functions"""
    mock_dia_api = Mock()
    # Module exists but doesn't have the functions
    del mock_dia_api.list_commands
    del mock_dia_api.execute_command

    with patch('builtins.__import__', return_value=mock_dia_api):
        bridge = DiaAPIBridge()

        # Should be marked as available (module loaded)
        assert bridge.is_available()

        # But operations should fail gracefully
        commands = bridge.list_commands()
        assert commands == []

        exit_code = bridge.execute('test', [])
        assert exit_code == 1


def test_bridge_singleton():
    """Verify bridge uses singleton pattern"""
    from dia_cli.utils.diaapi_bridge import get_bridge

    bridge1 = get_bridge()
    bridge2 = get_bridge()

    assert bridge1 is bridge2
