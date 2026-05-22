"""Auto-registers the DiaAutomation pytest plugin for all scenarios."""
import sys
from pathlib import Path

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from plugin import *  # noqa: F401, F403, E402
from plugin import pytest_runtest_teardown, pytest_configure  # noqa: F401, E402
from plugin import app_launcher, dia_client  # noqa: F401, E402
