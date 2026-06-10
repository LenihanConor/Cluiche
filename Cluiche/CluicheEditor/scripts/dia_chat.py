"""
dia_chat.py -- ChatOrchestrator for DiaChatPlugin.

Loaded by C++ via DiaPython: dia_python.load_script("scripts/dia_chat.py")
Top-level functions are registered as DiaPython callable bindings.

No third-party imports at module level.  Backend-specific imports (anthropic,
ollama, httpx, google-generativeai) live in Task 3's backend classes.
"""

import json
import logging
import os
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field

_LOG = logging.getLogger("dia_chat")


# ---------------------------------------------------------------------------
# Persistence helpers (DCP-008)
# ---------------------------------------------------------------------------

def _history_path(project_path):
    """Return the JSONL path for the given project_path."""
    slug = os.path.basename(project_path.rstrip('/\\')).lower().replace(' ', '_')
    return os.path.join('Cluiche', 'out', 'CluicheEditor', 'chat', slug, 'history.jsonl')


def _load_history(path):
    """
    Load messages from a JSONL file.  Returns [] if the file does not exist.
    Skips malformed lines silently.
    """
    if not os.path.exists(path):
        return []
    messages = []
    try:
        with open(path, 'r', encoding='utf-8') as fh:
            for line in fh:
                line = line.strip()
                if not line:
                    continue
                try:
                    messages.append(json.loads(line))
                except Exception:
                    pass
    except Exception as e:
        print("[DiaChat] history error: {0}".format(e))
    return messages


def _save_history(path, messages):
    """Overwrite the JSONL file with the given message list."""
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w', encoding='utf-8') as fh:
            for msg in messages:
                fh.write(json.dumps(msg) + '\n')
    except Exception as e:
        print("[DiaChat] history error: {0}".format(e))


def _trim_history(messages, max_pairs=50):
    """
    Cap the message list to max_pairs exchanges (max_pairs * 2 messages).
    Drops the oldest messages when over the limit.
    """
    limit = max_pairs * 2
    if len(messages) > limit:
        return messages[len(messages) - limit:]
    return messages


# ---------------------------------------------------------------------------
# ChatEvent tagged union
# ---------------------------------------------------------------------------

@dataclass
class TokenChunk:
    """A streaming text token from the LLM."""
    text: str
    done: bool


@dataclass
class ToolCall:
    """The LLM wants to call an editor action."""
    call_id: str
    fn: str
    params: dict


@dataclass
class Done:
    """Signals end of a clean exchange (no further events)."""
    pass


@dataclass
class Error:
    """Signals a fatal error in the exchange."""
    message: str


# ---------------------------------------------------------------------------
# ILLMBackend ABC
# ---------------------------------------------------------------------------

class ILLMBackend(ABC):
    """Abstract base for all LLM provider backends."""

    @abstractmethod
    def stream_chat(self, messages, tools):
        """
        Yield ChatEvent instances for the given conversation turn.

        Parameters
        ----------
        messages : list[dict]  -- {role, content} message list
        tools    : list[dict]  -- tool definitions (action manifest format)

        Yields
        ------
        TokenChunk | ToolCall | Done | Error
        """

    @abstractmethod
    def list_models(self):
        """Return a list of available model name strings."""

    @abstractmethod
    def is_available(self):
        """Return True if this backend can be reached right now."""


# ---------------------------------------------------------------------------
# ConversationHistory
# ---------------------------------------------------------------------------

class ConversationHistory:
    """In-memory conversation history with JSONL persistence (DCP-008)."""

    def __init__(self):
        self._exchanges = []  # list of {role, content, tool_calls, timestamp}

    def add_exchange(self, user_text, assistant_text, tool_calls=None):
        """Append a user/assistant exchange pair to the history."""
        ts = time.time()
        self._exchanges.append({
            "role": "user",
            "content": user_text,
            "tool_calls": [],
            "timestamp": ts,
        })
        self._exchanges.append({
            "role": "assistant",
            "content": assistant_text,
            "tool_calls": tool_calls if tool_calls is not None else [],
            "timestamp": ts,
        })

    def load_from(self, messages):
        """
        Bulk-load a list of {"role", "content"} dicts (e.g. from JSONL file).
        Each dict is stored directly; missing keys default to empty values.
        """
        for msg in messages:
            if not isinstance(msg, dict):
                continue
            self._exchanges.append({
                "role": msg.get("role", ""),
                "content": msg.get("content", ""),
                "tool_calls": msg.get("tool_calls", []),
                "timestamp": msg.get("timestamp", 0.0),
            })

    def to_messages(self):
        """
        Return a flat list of {role, content} dicts suitable for the messages
        array passed to ILLMBackend.stream_chat.
        """
        result = []
        for entry in self._exchanges:
            result.append({"role": entry["role"], "content": entry["content"]})
        return result

    def to_list(self):
        """Return a copy of all exchanges as plain {role, content} dicts."""
        return [{"role": e["role"], "content": e["content"]} for e in self._exchanges]

    def clear(self):
        """Discard all stored history."""
        self._exchanges = []


# ---------------------------------------------------------------------------
# ToolDispatcher
# ---------------------------------------------------------------------------

class ToolError(Exception):
    pass


class ToolDispatcher:
    """
    Wraps the execute_action callable and raises ToolError on failure.

    Parameters
    ----------
    execute_action : callable(name: str, params: dict) -> dict
        The DiaPython binding to DiaEditorAPI::ExecuteAction.
    notify_bridge : callable(topic: str, payload: dict) | None
        Optional callback to push error events to the UI.
    """

    def __init__(self, execute_action, notify_bridge=None):
        self._execute_action = execute_action  # callable(name, params) -> dict
        self._notify_bridge = notify_bridge

    def _push(self, topic, payload):
        """Push an event to the UI bridge; silently swallows failures."""
        if self._notify_bridge is not None:
            try:
                self._notify_bridge(topic, payload)
            except Exception:
                pass

    def dispatch(self, name, params):
        try:
            result = self._execute_action(name, params)
            if result is None:
                result = {}
            return result
        except Exception as exc:
            # Detect DiaEditorAPI not loaded (ImportError or missing dia_editor module)
            if isinstance(exc, ImportError) or 'dia_editor' in str(exc):
                self._push('chat.error', {
                    'error_type': 'editor_api_unavailable',
                    'message': 'DiaEditorAPI not loaded. Editor actions are disabled.',
                })
            raise ToolError(str(exc)) from exc


# ---------------------------------------------------------------------------
# KnowledgeLoader
# ---------------------------------------------------------------------------

class KnowledgeLoader:
    """
    Reads ai_context/ files and assembles a token-budgeted system prompt.

    Priority order (highest first):
      1. editor_actions.md  — always generated from manifest (DCP-005; never read from disk)
      2. data_types.md      — always included if present
      3. engine_overview.md
      4. editor_workflows.md
      5. asset_style_guide.md

    Lower-priority files are trimmed first if the total would exceed token_budget.

    Parameters
    ----------
    ai_context_dir : str
        Path to the directory containing the ai_context .md files.
    token_budget : int
        Maximum estimated tokens for the assembled prompt (default 4096).
    """

    # Files read from disk, in priority order (editor_actions.md handled separately).
    _DISK_FILES = [
        ("data_types.md",       "Data Types"),
        ("engine_overview.md",  "Engine Overview"),
        ("editor_workflows.md", "Editor Workflows"),
        ("asset_style_guide.md","Asset Style Guide"),
    ]

    def __init__(self, ai_context_dir, token_budget=4096):
        self._ai_context_dir = ai_context_dir
        self._token_budget = token_budget

    def load(self, manifest=None):
        """
        Assemble and return the system prompt string.

        If manifest is provided, regenerate editor_actions.md content inline
        (DCP-005 — never read editor_actions.md from disk; always generate from manifest).

        Trims lower-priority files first if total exceeds token_budget.
        Returns the assembled prompt string.
        """
        import os

        sections = []  # list of (title, content) in priority order

        # --- Priority 1: editor_actions.md — generated from manifest, never from disk ---
        if manifest is not None:
            actions_content = self._generate_actions_content(manifest)
            if actions_content:
                sections.append(("Editor Actions", actions_content))

        # --- Priorities 2-5: disk files ---
        for filename, title in self._DISK_FILES:
            filepath = os.path.join(self._ai_context_dir, filename)
            try:
                with open(filepath, "r", encoding="utf-8") as fh:
                    content = fh.read().strip()
                if content:
                    sections.append((title, content))
            except OSError:
                pass  # Missing file — skip silently

        # --- Assemble with budget enforcement ---
        # Build formatted blocks; trim from the lowest priority (end of list) first.
        blocks = ["## {0}\n{1}\n".format(title, content) for title, content in sections]
        while blocks:
            candidate = "\n".join(blocks)
            if self.estimate_tokens(candidate) <= self._token_budget:
                return candidate
            # Over budget — drop the lowest-priority block and retry.
            blocks.pop()

        return ""

    @staticmethod
    def _generate_actions_content(manifest):
        """Format the manifest actions list into Markdown."""
        actions = manifest if isinstance(manifest, list) else manifest.get("actions", [])
        if not actions:
            return ""
        lines = []
        for action in actions:
            if not isinstance(action, dict):
                continue
            name = action.get("name", "")
            description = action.get("description", "")
            params = action.get("params", [])
            lines.append("### {0}".format(name))
            if description:
                lines.append(description)
            if params:
                param_parts = []
                for p in params:
                    if not isinstance(p, dict):
                        continue
                    pname = p.get("name", "")
                    ptype = p.get("type", "")
                    preq = "required" if p.get("required") else "optional"
                    pdesc = p.get("description", "")
                    param_parts.append("{0} ({1}, {2}): {3}".format(pname, ptype, preq, pdesc))
                lines.append("Params: " + "; ".join(param_parts))
            lines.append("")
        return "\n".join(lines).strip()

    @staticmethod
    def estimate_tokens(text):
        """word count / 0.75"""
        return int(len(text.split()) / 0.75)


# ---------------------------------------------------------------------------
# ChatOrchestrator
# ---------------------------------------------------------------------------

class ChatOrchestrator:
    """
    Manages the full LLM conversation loop for DiaChatPlugin.

    Parameters
    ----------
    token_callback   : callable(text: str, done: bool)
        Pushes streamed tokens to C++ (registered by ChatPanelBridge).
    manifest_getter  : callable() -> dict | None
        Returns the DiaEditorAPI action manifest.  Returns None when
        DiaEditorAPI is not loaded; chat continues in knowledge-only mode.
    execute_action   : callable(name: str, params: dict) -> dict
        Calls C++ DiaEditorAPI::ExecuteAction.
    confirm_callback : callable(call_id: str, fn: str, params: dict, description: str)
        Triggers the C++ confirmation gate for destructive actions.
    notify_bridge    : callable(topic: str, payload: dict) | None
        Pushes error/status events to the React UI.  None disables UI
        error notifications (used in headless tests).
    max_tool_depth   : int
        Maximum tool calls allowed per send_message exchange (DCP-007, default 8).
    """

    def __init__(self, token_callback, manifest_getter, execute_action,
                 confirm_callback, notify_bridge=None, max_tool_depth=8,
                 project_path=""):
        self._token_callback = token_callback
        self._manifest_getter = manifest_getter
        self._execute_action = execute_action
        self._confirm_callback = confirm_callback
        self._notify_bridge = notify_bridge
        self._max_tool_depth = max_tool_depth

        self._dispatcher = ToolDispatcher(execute_action, notify_bridge=notify_bridge)

        self._backend = None
        self._system_prompt = ""
        self._history = ConversationHistory()
        self._context_mode = "full_context"

        # Persistence (DCP-008)
        self._history_path = _history_path(project_path) if project_path else ""
        if self._history_path:
            saved = _load_history(self._history_path)
            if saved:
                self._history.load_from(saved)

        # Maps call_id -> (fn, params, threading.Event, result_container)
        # result_container is a one-element list so the event-handler can write
        # the outcome (dict | None) and send_message can read it after the event.
        self.pending_confirm = {}

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _push(self, topic, payload):
        """Push an event to the UI bridge; silently swallows failures."""
        if self._notify_bridge is not None:
            try:
                self._notify_bridge(topic, payload)
            except Exception:
                pass

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def set_backend(self, backend):
        """
        Set the active ILLMBackend implementation.

        After switching, checks availability and pushes a
        chat.backend_status error event to the UI if the backend is not
        reachable or has no models installed.
        """
        self._backend = backend
        self._check_backend_availability(backend)

    def _check_backend_availability(self, backend):
        """
        Inspect *backend* and push chat.backend_status error events for
        the three backend error states: ollama_down, no_models,
        api_key_missing.
        """
        if backend is None:
            return

        # --- Ollama: check reachability first, then model list ---
        backend_name = type(backend).__name__.lower()
        is_ollama = 'ollama' in backend_name

        try:
            available = backend.is_available()
        except Exception:
            available = False

        if is_ollama and not available:
            self._push('chat.backend_status', {
                'status': 'error',
                'error_type': 'ollama_down',
                'message': 'Ollama is not running. Start Ollama or switch to Claude/Gemini.',
            })
            return

        if available:
            try:
                models = backend.list_models()
            except Exception:
                models = []
            if is_ollama and not models:
                self._push('chat.backend_status', {
                    'status': 'error',
                    'error_type': 'no_models',
                    'message': 'No models installed. Run: ollama pull llama3.2',
                })
                return

        # --- Cloud backends (non-ollama): check for API key ---
        if not is_ollama:
            api_key = getattr(backend, 'api_key', None)
            # Also accept a truthy _api_key attribute for flexibility.
            if api_key is None:
                api_key = getattr(backend, '_api_key', None)
            if not api_key:
                self._push('chat.backend_status', {
                    'status': 'error',
                    'error_type': 'api_key_missing',
                    'message': 'API key not set. Set ANTHROPIC_API_KEY / GOOGLE_API_KEY env var.',
                })

    def set_system_prompt(self, text):
        """Set the system prompt (called by KnowledgeLoader in Task 4)."""
        self._system_prompt = text

    # ------------------------------------------------------------------
    # Public entry point
    # ------------------------------------------------------------------

    def send_message(self, text, context_mode="full_context", extra_files=None):
        """
        Run the full LLM conversation loop for a single user message.

        1. Assemble messages (system + history + new user message).
        2. Fetch tool definitions from manifest_getter (mode-dependent).
        3. Call backend.stream_chat(messages, tools).
        4. For each ToolCall event: dispatch, feed result back, continue.
        5. Stream tokens to C++ via token_callback.
        6. Append completed exchange to ConversationHistory.

        Parameters
        ----------
        text         : str  -- the user's message text
        context_mode : str  -- "full_context" | "tools_only" | "custom"
        extra_files  : list -- additional file paths for context (Task 4)
        """
        if self._backend is None:
            _LOG.warning("dia_chat: send_message called with no backend set")
            self._token_callback("[error: no LLM backend configured]", True)
            return

        if extra_files is None:
            extra_files = []

        # --- 1. Assemble messages ---
        messages = []
        if self._system_prompt and context_mode != "tools_only":
            messages.append({"role": "system", "content": self._system_prompt})
        messages.extend(self._history.to_messages())
        messages.append({"role": "user", "content": text})

        # --- 2. Fetch tools ---
        tools = self._get_tools(context_mode)

        # --- 3-4. Streaming loop with tool call handling ---
        tool_depth = 0
        assistant_text_parts = []
        all_tool_calls = []

        _LOG.info("dia_chat: stream started (context_mode=%s)", context_mode)

        while True:
            stream_ended = False
            pending_tool_call = None

            try:
                for event in self._backend.stream_chat(messages, tools):
                    if isinstance(event, TokenChunk):
                        if event.text:
                            assistant_text_parts.append(event.text)
                            self._token_callback(event.text, False)
                        if event.done:
                            stream_ended = True
                            break

                    elif isinstance(event, ToolCall):
                        pending_tool_call = event
                        break

                    elif isinstance(event, Done):
                        stream_ended = True
                        break

                    elif isinstance(event, Error):
                        _LOG.error("dia_chat: backend error: %s", event.message)
                        self._token_callback(
                            "[error: {0}]".format(event.message), True
                        )
                        return

            except Exception as exc:
                exc_type = type(exc).__name__
                if 'ConnectionError' in exc_type or 'ConnectError' in exc_type:
                    _LOG.error("dia_chat: stream connection dropped: %s", exc)
                    self._push('chat.error', {
                        'error_type': 'stream_dropped',
                        'message': 'Connection lost.',
                        'retry': True,
                    })
                else:
                    _LOG.error("dia_chat: exception during stream_chat: %s", exc)
                self._token_callback("[error: {0}]".format(str(exc)), True)
                return

            if pending_tool_call is not None:
                tool_depth += 1
                try:
                    tool_result = self._handle_tool_call(
                        pending_tool_call, tool_depth, messages, tools
                    )
                except ToolError as exc:
                    _LOG.error("dia_chat: _handle_tool_call raised: %s", exc)
                    self._token_callback("[error: {0}]".format(str(exc)), True)
                    return
                if tool_result is None:
                    # Depth limit reached — already injected the limit message;
                    # break out and let the loop send final tokens.
                    break
                all_tool_calls.append({
                    "call_id": pending_tool_call.call_id,
                    "fn": pending_tool_call.fn,
                    "params": pending_tool_call.params,
                    "result": tool_result,
                })
                # Feed tool result back into messages and re-enter the stream.
                messages.append({
                    "role": "tool",
                    "tool_call_id": pending_tool_call.call_id,
                    "content": json.dumps(tool_result),
                })
                continue  # re-enter the while loop

            # No more tool calls; the stream ended naturally.
            break

        # --- 5. Signal end of tokens ---
        self._token_callback("", True)
        _LOG.info("dia_chat: stream ended (tool_calls=%d)", len(all_tool_calls))

        # --- 6. Append to history ---
        assistant_text = "".join(assistant_text_parts)
        self._history.add_exchange(text, assistant_text, all_tool_calls)

        # --- 7. Persist history (DCP-008) ---
        if self._history_path:
            trimmed = _trim_history(self._history.to_list())
            _save_history(self._history_path, trimmed)

    # ------------------------------------------------------------------
    # Tool handling helpers
    # ------------------------------------------------------------------

    def _get_tools(self, context_mode):
        """Return the tools list appropriate for the given context_mode."""
        if context_mode == "tools_only" or context_mode == "full_context" or context_mode == "custom":
            manifest = self._manifest_getter()
            if manifest is None:
                _LOG.warning(
                    "dia_chat: manifest_getter returned None — "
                    "DiaEditorAPI not loaded; continuing in knowledge-only mode"
                )
                return []
            # The manifest dict is expected to carry an 'actions' or 'tools' key.
            # Return whatever the backend expects; pass through as-is.
            if isinstance(manifest, list):
                return manifest
            if isinstance(manifest, dict):
                return manifest.get("actions", manifest.get("tools", []))
        return []

    def _handle_tool_call(self, tool_call, depth, messages, tools):
        """
        Dispatch a single tool call event.

        Returns the result dict, or None if the depth limit was hit
        (in that case the limit message is injected into `messages`).
        """
        if depth > self._max_tool_depth:
            _LOG.warning(
                "dia_chat: depth guard triggered (depth=%d, max=%d)", depth, self._max_tool_depth
            )
            messages.append({
                "role": "user",
                "content": (
                    "Max tool call depth reached. "
                    "Please summarize what you've done."
                ),
            })
            return None

        fn = tool_call.fn
        params = tool_call.params
        call_id = tool_call.call_id

        # --- Destructive action gate ---
        manifest = self._manifest_getter()
        if self._is_destructive(fn, manifest):
            description = self._get_action_description(fn, manifest)
            confirmed = self._await_confirmation(call_id, fn, params, description)
            if not confirmed:
                self._push('chat.error', {
                    'error_type': 'destructive_cancel',
                    'message': '{0} was cancelled.'.format(fn),
                    'inline': True,
                })
                return {"error": "User cancelled action"}

        # --- Dispatch with timeout guard (5 s) ---
        _TOOL_TIMEOUT_S = 5.0
        result_holder = [None]
        error_holder = [None]
        done_event = threading.Event()

        def _run_tool():
            try:
                result_holder[0] = self._dispatcher.dispatch(fn, params)
            except Exception as exc:
                error_holder[0] = exc
            finally:
                done_event.set()

        t = threading.Thread(target=_run_tool, daemon=True)
        t.start()

        if not done_event.wait(timeout=_TOOL_TIMEOUT_S):
            _LOG.error("dia_chat: tool '%s' timed out after %.1fs", fn, _TOOL_TIMEOUT_S)
            self._push('chat.error', {
                'error_type': 'tool_timeout',
                'message': 'Action timed out. Retrying...',
            })
            # Single retry — wait another full timeout window for the thread to finish.
            if not done_event.wait(timeout=_TOOL_TIMEOUT_S):
                self._push('chat.error', {
                    'error_type': 'tool_timeout',
                    'message': 'Action timed out.',
                })
                raise ToolError('Tool call timed out after {0}s'.format(_TOOL_TIMEOUT_S))

        if error_holder[0] is not None:
            exc = error_holder[0]
            if isinstance(exc, ToolError):
                _LOG.error("dia_chat: execute_action('%s') raised: %s", fn, exc)
                return {"error": str(exc)}
            raise exc

        return result_holder[0]

    def _is_destructive(self, fn, manifest):
        """Return True if the named action is marked destructive in the manifest."""
        if manifest is None:
            return False
        actions = manifest if isinstance(manifest, list) else manifest.get("actions", [])
        for action in actions:
            if isinstance(action, dict) and action.get("name") == fn:
                return bool(action.get("destructive", False))
        return False

    def _get_action_description(self, fn, manifest):
        """Return the description string for an action, or empty string."""
        if manifest is None:
            return ""
        actions = manifest if isinstance(manifest, list) else manifest.get("actions", [])
        for action in actions:
            if isinstance(action, dict) and action.get("name") == fn:
                return action.get("description", "")
        return ""

    def _await_confirmation(self, call_id, fn, params, description):
        """
        Call confirm_callback to push a chat.confirm_required event to C++,
        then block on a threading.Event until on_confirm_response is called.

        Returns True if the user confirmed, False if cancelled.
        """
        event = threading.Event()
        result_container = [None]  # [True | False]
        self.pending_confirm[call_id] = (fn, params, event, result_container)

        try:
            self._confirm_callback(call_id, fn, params, description)
        except Exception as exc:
            _LOG.error("dia_chat: confirm_callback raised: %s", exc)
            del self.pending_confirm[call_id]
            return False

        # Block until C++ calls on_confirm_response (or timeout safety net).
        signalled = event.wait(timeout=300)  # 5-minute safety timeout
        del self.pending_confirm[call_id]

        if not signalled:
            _LOG.warning(
                "dia_chat: confirmation for '%s' timed out after 300 s", fn
            )
            return False

        return bool(result_container[0])

    # ------------------------------------------------------------------
    # Confirmation response (called from C++ via module-level binding)
    # ------------------------------------------------------------------

    def on_confirm_response(self, call_id, confirmed):
        """
        Resolve a pending destructive-action confirmation.

        Called by C++ (via the on_confirm_response module-level function)
        when the user clicks Confirm or Cancel in the chat panel.
        """
        entry = self.pending_confirm.get(call_id)
        if entry is None:
            _LOG.warning(
                "dia_chat: on_confirm_response for unknown call_id '%s'", call_id
            )
            return
        _fn, _params, event, result_container = entry
        result_container[0] = confirmed
        event.set()

    # ------------------------------------------------------------------
    # History
    # ------------------------------------------------------------------

    def clear_history(self):
        """Clear the in-memory conversation history and delete the JSONL file."""
        self._history.clear()
        if self._history_path and os.path.exists(self._history_path):
            try:
                os.remove(self._history_path)
            except Exception as e:
                print("[DiaChat] history error: {0}".format(e))


# ---------------------------------------------------------------------------
# Module-level singleton and DiaPython-callable bindings
# ---------------------------------------------------------------------------

_orchestrator = None  # type: ChatOrchestrator | None


def initialize(token_callback, manifest_getter, execute_action, confirm_callback,
               notify_bridge=None, ai_context_dir=None, token_budget=4096,
               project_path=""):
    """
    Create the global ChatOrchestrator instance.

    Called by C++ (ChatPanelBridge) at plugin startup via DiaPython AddFunction.

    Parameters
    ----------
    token_callback   : callable(text: str, done: bool)
    manifest_getter  : callable() -> dict | None
    execute_action   : callable(name: str, params: dict) -> dict
    confirm_callback : callable(call_id, fn, params, description)
    notify_bridge    : callable(topic: str, payload: dict) | None
        Callback for pushing error/status events to the React UI.
    ai_context_dir   : str | None
        Path to the ai_context/ directory.  When provided, a KnowledgeLoader
        is created, the system prompt is assembled, and set on the orchestrator.
    token_budget     : int
        Token budget forwarded to KnowledgeLoader (default 4096).
    project_path     : str
        Path to the open project directory.  Used to derive the JSONL history
        path for conversation persistence (DCP-008).
    """
    global _orchestrator
    _orchestrator = ChatOrchestrator(
        token_callback=token_callback,
        manifest_getter=manifest_getter,
        execute_action=execute_action,
        confirm_callback=confirm_callback,
        notify_bridge=notify_bridge,
        project_path=project_path,
    )
    if ai_context_dir:
        loader = KnowledgeLoader(ai_context_dir, token_budget=token_budget)
        manifest = manifest_getter() if manifest_getter else None
        prompt = loader.load(manifest=manifest)
        _orchestrator.set_system_prompt(prompt)
    _LOG.info("dia_chat: initialized")


def send_message(text, context_mode="full_context", extra_files=None):
    """
    Deliver a user message to the orchestrator.

    Runs on a DiaPython background thread; token_callback is thread-safe
    (ChatPanelBridge handles marshalling to the UI thread).
    """
    if _orchestrator is None:
        _LOG.error("dia_chat: send_message called before initialize()")
        return
    _orchestrator.send_message(text, context_mode=context_mode, extra_files=extra_files)


def set_backend(backend_name, model):
    """
    Select an LLM backend by name and model.

    Instantiates the named backend via create_backend() and registers it on
    the orchestrator.  Backend classes live in dia_chat_backends (Task 3).
    After switching, the orchestrator checks availability and pushes error
    events (ollama_down / no_models / api_key_missing) to the UI if needed.
    """
    _LOG.info("dia_chat: set_backend: backend=%s model=%s", backend_name, model)
    try:
        from dia_chat_backends import create_backend
        backend = create_backend(backend_name, model)
        if _orchestrator is not None:
            # set_backend also calls _check_backend_availability internally.
            _orchestrator.set_backend(backend)
        else:
            _LOG.warning(
                "dia_chat: set_backend called before initialize(); "
                "backend will not be applied until orchestrator is created"
            )
    except Exception as exc:
        _LOG.error("dia_chat: set_backend failed: %s", exc)


def set_context_mode(mode):
    """Store the active context mode on the orchestrator."""
    if _orchestrator is None:
        _LOG.warning("dia_chat: set_context_mode called before initialize()")
        return
    _orchestrator._context_mode = mode


def add_context_file(path):
    """Log receipt of an additional context file (KnowledgeLoader wires this in Task 4)."""
    _LOG.info("dia_chat: add_context_file: %s", path)


def clear_history():
    """Clear the conversation history."""
    if _orchestrator is None:
        _LOG.warning("dia_chat: clear_history called before initialize()")
        return
    _orchestrator.clear_history()


def on_confirm_response(call_id, confirmed):
    """
    Forward a user confirmation/cancellation to the orchestrator.

    Called by C++ after the user responds to a destructive-action prompt.
    """
    if _orchestrator is None:
        _LOG.warning("dia_chat: on_confirm_response called before initialize()")
        return
    _orchestrator.on_confirm_response(call_id, confirmed)
