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
    """Sync WebSocket client for DiaDebugServer protocol.

    DiaDebugServer uses a protobuf-over-JSON wire format: each message has a
    'type' field (MESSAGE_TYPE_*) and a typed payload. Commands use
    MESSAGE_TYPE_COMMAND_REQUEST / MESSAGE_TYPE_COMMAND_RESPONSE.
    """

    def __init__(self, host="localhost", port=9876):
        self.host = host
        self.port = port
        self._ws = None

    def connect(self, timeout=60.0):
        """Retry WebSocket connection every 500ms until success or timeout.

        After connecting, drains the server's welcome messages (handshake
        response + game info) before returning.
        """
        if ws_connect is None:
            raise ImportError("websockets package is required: pip install websockets>=12.0")
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self._ws = ws_connect(f"ws://{self.host}:{self.port}")
                self._drain_welcome()
                return
            except Exception as e:
                last_error = e
                time.sleep(0.5)
        raise TimeoutError(
            f"Could not connect to ws://{self.host}:{self.port} within {timeout}s: {last_error}"
        )

    def _drain_welcome(self):
        """Read and discard the initial handshake_response and game_info."""
        for _ in range(5):
            try:
                raw = self._ws.recv(timeout=2)
                msg = json.loads(raw)
                msg_type = msg.get("type", "")
                if msg_type in ("MESSAGE_TYPE_HANDSHAKE_RESPONSE", "MESSAGE_TYPE_GAME_INFO"):
                    continue
                break
            except Exception:
                break

    def disconnect(self):
        """Close WebSocket connection."""
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass
            self._ws = None

    def send_command(self, command: str, params: dict = None) -> dict:
        """Send a DiaAPI command; return the parsed payload on success.

        Raises AutomationError if the response contains success=false.
        """
        if self._ws is None:
            raise RuntimeError("DiaClient is not connected")

        request = {
            "type": "MESSAGE_TYPE_COMMAND_REQUEST",
            "command_request": {
                "command": command,
                "payload": params or {},
            }
        }
        self._ws.send(json.dumps(request))

        # Read messages until we get the command response.
        # Server pushes observation metrics/logs continuously — skip them.
        deadline = time.time() + 15.0
        while time.time() < deadline:
            raw = self._ws.recv(timeout=10)
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if msg.get("type") == "MESSAGE_TYPE_COMMAND_RESPONSE":
                resp = msg.get("command_response", {})
                if not resp.get("success"):
                    raise AutomationError(resp.get("message", "unknown error"))
                return resp.get("payload", {})
        raise TimeoutError(f"No command response received for '{command}'")

    # --- Helpers (thin wrappers over send_command) ---

    def navigate_to(self, target: str) -> dict:
        return self.send_command("dia.automation.navigate_to", {"target": target})

    def quit(self) -> dict:
        try:
            return self.send_command("dia.app.quit")
        except Exception:
            return {}

    def validate(self, checkpoint: str) -> dict:
        return self.send_command("dia.automation.validate", {"checkpoint": checkpoint})

    def report(self) -> dict:
        return self.send_command("dia.app.report")

    def pause(self) -> dict:
        return self.send_command("dia.automation.pause")

    def resume(self) -> dict:
        return self.send_command("dia.automation.resume")
