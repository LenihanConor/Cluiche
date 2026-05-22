"""DiaClient — sync WebSocket client for DiaAutomation commands."""
import json
import time

try:
    from websockets.sync.client import connect as ws_connect
except ImportError:
    ws_connect = None  # allow import without websockets installed


class AutomationError(Exception):
    pass


class DiaClient:
    """Sync WebSocket client for DiaAutomation commands."""

    def __init__(self, host="localhost", port=9876):
        self.host = host
        self.port = port
        self._ws = None

    def connect(self, timeout=60.0):
        """Retry WebSocket connection every 500ms until success or timeout."""
        if ws_connect is None:
            raise ImportError("websockets package is required: pip install websockets>=12.0")
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self._ws = ws_connect(f"ws://{self.host}:{self.port}")
                return
            except (ConnectionRefusedError, OSError) as e:
                last_error = e
                time.sleep(0.5)
        raise TimeoutError(
            f"Could not connect to ws://{self.host}:{self.port} within {timeout}s: {last_error}"
        )

    def disconnect(self):
        """Close WebSocket connection."""
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass
            self._ws = None

    def send_command(self, command: str, params: dict = None) -> dict:
        """Send a DiaAPI command; return the parsed data field on success.

        Raises AutomationError if the response contains success=false.
        """
        if self._ws is None:
            raise RuntimeError("DiaClient is not connected")
        request = {"command": command, "params": params or {}}
        self._ws.send(json.dumps(request))
        response = json.loads(self._ws.recv())
        if not response.get("success"):
            raise AutomationError(response.get("error", "unknown error"))
        return response.get("data", {})

    # --- Helpers (thin wrappers over send_command) ---

    def navigate_to(self, target: str) -> dict:
        return self.send_command("dia.automation.navigate_to", {"target": target})

    def quit(self) -> dict:
        return self.send_command("dia.app.quit")

    def validate(self, checkpoint: str) -> dict:
        return self.send_command("dia.automation.validate", {"checkpoint": checkpoint})

    def report(self) -> dict:
        return self.send_command("dia.app.report")

    def pause(self) -> dict:
        return self.send_command("dia.automation.pause")

    def resume(self) -> dict:
        return self.send_command("dia.automation.resume")
