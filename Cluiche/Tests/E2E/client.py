"""DiaClient — sync WebSocket client for DiaAutomation commands."""
import json
import queue
import threading
import time

try:
    from websockets.sync.client import connect as ws_connect
except ImportError:
    ws_connect = None  # allow import without websockets installed


class AutomationError(Exception):
    pass


class DiaClient:
    """Sync WebSocket client for DiaDebugServer protocol.

    A background reader thread owns all recv() calls and demultiplexes:
      - MESSAGE_TYPE_COMMAND_RESPONSE  → _cmd_queue
      - MESSAGE_TYPE_EVENT stage_transition → _event_queue
      - Everything else (observation data, logs) is discarded

    navigate_to() blocks on _event_queue waiting for a stage_transition
    event instead of polling report(). This eliminates the race where C++
    has already returned to Boot before Python calls navigate_to("Boot").
    """

    def __init__(self, host="localhost", port=9002):
        self.host = host
        self.port = port
        self._ws = None
        self._cmd_queue = queue.Queue()
        self._event_queue = queue.Queue()
        self._topic_queues: dict = {}
        self._reader_thread = None
        self._reader_stop = threading.Event()

    def connect(self, timeout=60.0):
        """Retry connection every 500ms until success or timeout."""
        if ws_connect is None:
            raise ImportError("websockets package is required: pip install websockets>=12.0")
        deadline = time.time() + timeout
        last_error = None
        while time.time() < deadline:
            try:
                self._ws = ws_connect(f"ws://{self.host}:{self.port}")
                self._drain_welcome()
                self._subscribe_stage_transitions()
                self._start_reader()
                return
            except Exception as e:
                last_error = e
                if self._ws:
                    try:
                        self._ws.close()
                    except Exception:
                        pass
                    self._ws = None
                time.sleep(0.5)
        raise TimeoutError(
            f"Could not connect to ws://{self.host}:{self.port} within {timeout}s: {last_error}"
        )

    def _drain_welcome(self):
        """Read handshake + game_info synchronously before handing off to reader."""
        for _ in range(5):
            try:
                raw = self._ws.recv(timeout=2.0)
                msg = json.loads(raw)
                if msg.get("type") in ("MESSAGE_TYPE_HANDSHAKE_RESPONSE", "MESSAGE_TYPE_GAME_INFO"):
                    continue
                self._route(msg)
                break
            except Exception:
                break

    def _subscribe_stage_transitions(self):
        """Send SUBSCRIBE for stage_transition and wait for the ACK."""
        sub = {"type": "MESSAGE_TYPE_SUBSCRIBE", "subscribe": {"data_type": "stage_transition"}}
        self._ws.send(json.dumps(sub))
        deadline = time.time() + 5.0
        while time.time() < deadline:
            try:
                raw = self._ws.recv(timeout=1.0)
                msg = json.loads(raw)
                if msg.get("type") == "MESSAGE_TYPE_SUBSCRIBE_ACK":
                    return
                self._route(msg)
            except Exception:
                return

    def _start_reader(self):
        self._reader_stop.clear()
        # Drain stale queue items from any prior connection
        for q in (self._cmd_queue, self._event_queue):
            while not q.empty():
                try:
                    q.get_nowait()
                except queue.Empty:
                    break
        for q in self._topic_queues.values():
            while not q.empty():
                try:
                    q.get_nowait()
                except queue.Empty:
                    break
        self._reader_thread = threading.Thread(
            target=self._read_loop, daemon=True, name="DiaClient-reader"
        )
        self._reader_thread.start()

    def _read_loop(self):
        while not self._reader_stop.is_set():
            try:
                raw = self._ws.recv(timeout=0.5)
            except TimeoutError:
                continue
            except Exception:
                break
            try:
                msg = json.loads(raw)
            except json.JSONDecodeError:
                continue
            self._route(msg)

    def _route(self, msg: dict):
        msg_type = msg.get("type", "")
        if msg_type == "MESSAGE_TYPE_COMMAND_RESPONSE":
            self._cmd_queue.put(msg)
        elif msg_type == "MESSAGE_TYPE_EVENT":
            ev = msg.get("event", {})
            if ev.get("event_type") == "stage_transition":
                self._event_queue.put(ev.get("payload", {}))
        elif msg_type == "MESSAGE_TYPE_DATA_UPDATE":
            update = msg.get("data_update", {})
            topic = update.get("data_type", "")
            if topic and topic in self._topic_queues:
                payload = update.get("payload", {})
                self._topic_queues[topic].put(payload)

    def subscribe_topic(self, topic: str):
        """Subscribe to a data topic. Call before wait_for_topic()."""
        if topic not in self._topic_queues:
            self._topic_queues[topic] = queue.Queue()
        sub = {"type": "MESSAGE_TYPE_SUBSCRIBE", "subscribe": {"data_type": topic}}
        self._ws.send(json.dumps(sub))
        time.sleep(0.2)  # give server time to register the subscription

    def wait_for_topic(self, topic: str, timeout_s: float = 5.0) -> dict:
        """Block until a DATA_UPDATE message arrives for the given topic."""
        if topic not in self._topic_queues:
            self._topic_queues[topic] = queue.Queue()
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            remaining = deadline - time.time()
            try:
                return self._topic_queues[topic].get(timeout=min(0.5, remaining))
            except queue.Empty:
                continue
        raise TimeoutError(f"No data received for topic '{topic}' within {timeout_s}s")

    def disconnect(self):
        self._reader_stop.set()
        if self._ws:
            try:
                self._ws.close()
            except Exception:
                pass
            self._ws = None
        if self._reader_thread:
            self._reader_thread.join(timeout=2.0)
            self._reader_thread = None

    def send_command(self, command: str, params: dict = None, timeout: float = 30.0) -> dict:
        """Send a command and return its response payload. Raises AutomationError on failure."""
        if self._ws is None:
            raise RuntimeError("DiaClient is not connected")
        request = {
            "type": "MESSAGE_TYPE_COMMAND_REQUEST",
            "command_request": {"command": command, "payload": params or {}},
        }
        self._ws.send(json.dumps(request))
        deadline = time.time() + timeout
        while time.time() < deadline:
            remaining = deadline - time.time()
            try:
                msg = self._cmd_queue.get(timeout=min(0.5, remaining))
            except queue.Empty:
                continue
            resp = msg.get("command_response", {})
            if resp.get("command") != command:
                continue  # stale response for a different command
            if not resp.get("success"):
                raise AutomationError(resp.get("message", "unknown error"))
            return resp.get("payload", {})
        raise TimeoutError(f"No command response received for '{command}'")

    # --- Navigation ---

    def navigate_to(self, target: str, timeout_s: float = 10.0) -> dict:
        """Navigate to a stage; blocks until a stage_transition event confirms arrival.

        Fast-paths:
        1. Already at target according to report() — returns immediately.
        2. A buffered stage_transition event already shows to_stage==target — returns immediately.
        """
        # Fast path 1: already there (e.g. post-reconnect, or C++ returned before we got here)
        try:
            if self.send_command("dia.app.report", timeout=3.0).get("stage") == target:
                return {}
        except Exception:
            pass

        # Fast path 2: event already buffered — drain queue, keep any that match target
        already_there = False
        while not self._event_queue.empty():
            try:
                ev = self._event_queue.get_nowait()
                if ev.get("to_stage") == target:
                    already_there = True
                # discard events for other transitions
            except queue.Empty:
                break
        if already_there:
            return {}

        self.send_command("dia.automation.navigate_to", {"target": target})

        # After sending the command, check immediately — the transition may have
        # already committed before we sent (C++ called ReleaseNavigationHold first),
        # meaning the event is already gone. A quick report() catches this.
        try:
            if self.send_command("dia.app.report", timeout=3.0).get("stage") == target:
                return {}
        except Exception:
            pass

        deadline = time.time() + timeout_s
        while time.time() < deadline:
            remaining = deadline - time.time()
            try:
                ev = self._event_queue.get(timeout=min(0.5, remaining))
            except queue.Empty:
                # Periodically re-check via report() in case we missed the event
                try:
                    if self.send_command("dia.app.report", timeout=2.0).get("stage") == target:
                        return {}
                except Exception:
                    pass
                continue
            if ev.get("to_stage") == target:
                return {}
            # Unexpected transition — put back so caller can detect it, keep waiting
            self._event_queue.put(ev)
            time.sleep(0.1)

        raise TimeoutError(
            f"Stage transition to '{target}' did not complete within {timeout_s}s"
        )

    # --- Helpers (thin wrappers) ---

    def abort_stage(self) -> dict:
        try:
            return self.send_command("dia.automation.abort_stage", timeout=5.0)
        except Exception:
            return {}

    def quit(self) -> dict:
        try:
            return self.send_command("dia.app.quit")
        except Exception:
            return {}

    def validate(self, checkpoint: str) -> dict:
        return self.send_command("dia.automation.validate", {"checkpoint": checkpoint})

    def poll_checkpoint(self, checkpoint: str, timeout_s: float = 10.0,
                        interval_s: float = 0.5, expected_stage: str = None) -> dict:
        """Poll checkpoint until it passes or the deadline is reached.

        If expected_stage is given, exits immediately when a stage_transition
        event shows the stage has exited (C++ auto-returned to Boot). The event
        is put back into the queue so navigate_to("Boot") can pick it up.
        """
        deadline = time.time() + timeout_s
        last_result = None
        while time.time() < deadline:
            # Check for stage exit via push event (non-blocking peek)
            if expected_stage is not None and not self._event_queue.empty():
                try:
                    ev = self._event_queue.get_nowait()
                    if ev.get("from_stage") == expected_stage:
                        # Put it back so _try_return_boot's navigate_to can consume it
                        self._event_queue.put(ev)
                        return {"passed": False,
                                "message": f"stage exited (went to '{ev.get('to_stage')}')"}
                    # Different transition — put back and continue
                    self._event_queue.put(ev)
                except queue.Empty:
                    pass

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
        result = self.send_command("dia.automation.get_metric", {"name": name})
        value = result.get("value")
        if isinstance(value, dict):
            return value.get("mean", 0)
        return value

    def list_stages(self) -> list:
        result = self.send_command("dia.manifest.stages")
        return result.get("stages", [])

    def list_checkpoints(self) -> list:
        result = self.send_command("dia.automation.list_checkpoints")
        return result.get("checkpoints", [])

    def pause(self) -> dict:
        return self.send_command("dia.automation.pause")

    def resume(self) -> dict:
        return self.send_command("dia.automation.resume")
