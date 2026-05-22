"""Auto-registers the DiaAutomation pytest plugin for all scenarios."""
import sys
from pathlib import Path

# Ensure the orchestrator root is on sys.path so plugin.py can import client.py
_HERE = Path(__file__).resolve().parent
_TOOLS = _HERE.parent
if str(_TOOLS) not in sys.path:
    sys.path.insert(0, str(_TOOLS))

pytest_plugins = ["orchestrator.plugin"]
