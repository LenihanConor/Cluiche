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

    def __init__(self, host="localhost", port=9002):
        self.host = host
        self.port = port
        self._ws = None
        self._recv_buffer = []

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
        """Read the initial handshake/game_info messages; buffer anything else."""
        for _ in range(5):
            try:
                raw = self._ws.recv(timeout=2)
                msg = json.loads(raw)
                msg_type = msg.get("type", "")
                if msg_type in ("MESSAGE_TYPE_HANDSHAKE_RESPONSE", "MESSAGE_TYPE_GAME_INFO"):
                    continue
                # Not a welcome message — keep it so send_command can see it
                self._recv_buffer.append(raw)
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

    def send_command(self, command: str, params: dict = None, timeout: float = 30.0) -> dict:
        """Send a DiaAPI command; return the parsed payload on success.

        Raises AutomationError if the response contains success=false.
        The server broadcasts observation data continuously, so this uses a
        fast string-level pre-filter before JSON parsing to handle the flood.
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

        deadline = time.time() + timeout
        while time.time() < deadline:
            # Drain buffered messages before blocking on the socket
            if self._recv_buffer:
                raw = self._recv_buffer.pop(0)
            else:
                try:
                    raw = self._ws.recv(timeout=0.5)
                except (TimeoutError, OSError):
                    continue
            if "MESSAGE_TYPE_COMMAND_RESPONSE" not in raw:
                continue
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if msg.get("type") != "MESSAGE_TYPE_COMMAND_RESPONSE":
                continue
            resp = msg.get("command_response", {})
            if resp.get("command") != command:
                continue
            if not resp.get("success"):
                raise AutomationError(resp.get("message", "unknown error"))
            return resp.get("payload", {})
        raise TimeoutError(f"No command response received for '{command}'")

    # --- Helpers (thin wrappers over send_command) ---

    def navigate_to(self, target: str, wait: bool = True, timeout_s: float = 10.0) -> dict:
        """Navigate to a stage. If wait=True, polls report() until the transition completes."""
        result = self.send_command("dia.automation.navigate_to", {"target": target})
        if wait:
            deadline = time.time() + timeout_s
            current_stage = None
            while time.time() < deadline:
                r = self.report()
                current_stage = r.get("stage")
                if current_stage == target:
                    return result
                time.sleep(0.2)
            raise TimeoutError(
                f"Stage transition to '{target}' did not complete within {timeout_s}s"
                f" (still on '{current_stage}')"
            )
        return result

    def quit(self) -> dict:
        try:
            return self.send_command("dia.app.quit")
        except Exception:
            return {}

    def validate(self, checkpoint: str) -> dict:
        return self.send_command("dia.automation.validate", {"checkpoint": checkpoint})

    def poll_checkpoint(self, checkpoint: str, timeout_s: float = 10.0, interval_s: float = 0.5) -> dict:
        """Poll a checkpoint until it passes or timeout is reached.

        Tolerates 'checkpoint not found' errors (module still starting up).
        Any other AutomationError is a permanent failure and is raised immediately.
        """
        deadline = time.time() + timeout_s
        last_result = None
        while time.time() < deadline:
            try:
                result = self.validate(checkpoint)
            except AutomationError as e:
                if "checkpoint not found" in str(e).lower():
                    time.sleep(interval_s)
                    continue
                raise
            if result.get("passed"):
                return result
            last_result = result
            time.sleep(interval_s)
        return last_result or {"passed": False, "message": "timeout"}

    def report(self) -> dict:
        return self.send_command("dia.app.report")

    def get_metric(self, name: str):
        """Read a live metric value from MetricRegistry by fully-qualified name.

        Returns the scalar value (int or float) for gauges. For histogram metrics,
        returns the mean. Raises AutomationError if the metric is not found.
        """
        result = self.send_command("dia.automation.get_metric", {"name": name})
        value = result.get("value")
        if isinstance(value, dict):
            return value.get("mean", 0)
        return value

    def list_stages(self) -> list:
        """Return the list of stages navigable from Boot."""
        result = self.send_command("dia.manifest.stages")
        return result.get("stages", [])

    def list_checkpoints(self) -> list:
        """Return checkpoint names registered for the current stage."""
        result = self.send_command("dia.automation.list_checkpoints")
        return result.get("checkpoints", [])

    def pause(self) -> dict:
        return self.send_command("dia.automation.pause")

    def resume(self) -> dict:
        return self.send_command("dia.automation.resume")
