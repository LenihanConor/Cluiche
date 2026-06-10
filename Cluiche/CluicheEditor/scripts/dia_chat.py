"""
dia_chat.py -- ChatOrchestrator for DiaChatPlugin.

Loaded by C++ via DiaPython: dia_python.load_script("scripts/dia_chat.py")
Top-level functions are registered as DiaPython callable bindings.

No third-party imports at module level.  Backend-specific imports (anthropic,
ollama, httpx, google-generativeai) live in Task 3's backend classes.
"""

import json
import logging
import threading
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field

_LOG = logging.getLogger("dia_chat")

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
    """In-memory conversation history (persistence is Task 8)."""

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

    def to_messages(self):
        """
        Return a flat list of {role, content} dicts suitable for the messages
        array passed to ILLMBackend.stream_chat.
        """
        result = []
        for entry in self._exchanges:
            result.append({"role": entry["role"], "content": entry["content"]})
        return result

    def clear(self):
        """Discard all stored history."""
        self._exchanges = []


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
    max_tool_depth   : int
        Maximum tool calls allowed per send_message exchange (DCP-007, default 8).
    """

    def __init__(self, token_callback, manifest_getter, execute_action,
                 confirm_callback, max_tool_depth=8):
        self._token_callback = token_callback
        self._manifest_getter = manifest_getter
        self._execute_action = execute_action
        self._confirm_callback = confirm_callback
        self._max_tool_depth = max_tool_depth

        self._backend = None
        self._system_prompt = ""
        self._history = ConversationHistory()
        self._context_mode = "full_context"

        # Maps call_id -> (fn, params, threading.Event, result_container)
        # result_container is a one-element list so the event-handler can write
        # the outcome (dict | None) and send_message can read it after the event.
        self.pending_confirm = {}

    # ------------------------------------------------------------------
    # Configuration
    # ------------------------------------------------------------------

    def set_backend(self, backend):
        """Set the active ILLMBackend implementation."""
        self._backend = backend

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
                _LOG.error("dia_chat: exception during stream_chat: %s", exc)
                self._token_callback("[error: {0}]".format(str(exc)), True)
                return

            if pending_tool_call is not None:
                tool_depth += 1
                tool_result = self._handle_tool_call(
                    pending_tool_call, tool_depth, messages, tools
                )
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

        # --- 6. Append to history ---
        assistant_text = "".join(assistant_text_parts)
        self._history.add_exchange(text, assistant_text, all_tool_calls)

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
                "dia_chat: max tool call depth (%d) reached", self._max_tool_depth
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
                return {"error": "User cancelled action"}

        # --- Dispatch ---
        try:
            result = self._execute_action(fn, params)
        except Exception as exc:
            _LOG.error("dia_chat: execute_action('%s') raised: %s", fn, exc)
            result = {"error": str(exc)}

        return result

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
        """Clear the conversation history."""
        self._history.clear()


# ---------------------------------------------------------------------------
# Module-level singleton and DiaPython-callable bindings
# ---------------------------------------------------------------------------

_orchestrator = None  # type: ChatOrchestrator | None


def initialize(token_callback, manifest_getter, execute_action, confirm_callback):
    """
    Create the global ChatOrchestrator instance.

    Called by C++ (ChatPanelBridge) at plugin startup via DiaPython AddFunction.
    """
    global _orchestrator
    _orchestrator = ChatOrchestrator(
        token_callback=token_callback,
        manifest_getter=manifest_getter,
        execute_action=execute_action,
        confirm_callback=confirm_callback,
    )
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
    """
    _LOG.info("dia_chat: set_backend: backend=%s model=%s", backend_name, model)
    try:
        from dia_chat_backends import create_backend
        backend = create_backend(backend_name, model)
        if _orchestrator is not None:
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
