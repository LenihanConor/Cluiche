"""
dia_chat_backends.py -- LLM backend implementations for DiaChatPlugin.

Provides OllamaBackend, ClaudeBackend, and GeminiBackend, each implementing
ILLMBackend from dia_chat.py.

All third-party imports (anthropic, google.generativeai, httpx, requests) are
deferred inside try/except ImportError so this file imports cleanly with only
stdlib present.

Backend selection is a runtime config — see create_backend().
"""

import json
import logging
import os
import sys

from dia_chat import ILLMBackend, TokenChunk, ToolCall, Done, Error

_LOG = logging.getLogger("dia_chat_backends")

# ---------------------------------------------------------------------------
# HTTP helper (OllamaBackend uses httpx, falls back to requests, raises on both absent)
# ---------------------------------------------------------------------------

def _try_import_http():
    """
    Return (module_name, module) for the first available HTTP library.
    Tries httpx first, then requests. Returns (None, None) if neither is present.
    """
    try:
        import httpx as _httpx
        return ("httpx", _httpx)
    except ImportError:
        pass
    try:
        import requests as _requests
        return ("requests", _requests)
    except ImportError:
        pass
    return (None, None)


def _http_get(url, timeout=2):
    """
    Issue a GET request using whichever HTTP library is available.
    Returns a response object with .json() and .status_code.
    Raises ImportError if neither httpx nor requests is available.
    Raises on connection failure.
    """
    lib_name, lib = _try_import_http()
    if lib is None:
        raise ImportError("httpx or requests required for OllamaBackend")
    return lib.get(url, timeout=timeout)


def _http_post_stream(url, payload, timeout=60):
    """
    Issue a streaming POST request using whichever HTTP library is available.
    Returns a context manager / response that supports iter_lines() or equivalent.
    Raises ImportError if neither httpx nor requests is available.
    Raises on connection failure.
    """
    lib_name, lib = _try_import_http()
    if lib is None:
        raise ImportError("httpx or requests required for OllamaBackend")

    headers = {"Content-Type": "application/json"}
    if lib_name == "httpx":
        # httpx streaming: use a context manager
        return lib.stream(
            "POST", url,
            content=json.dumps(payload).encode(),
            headers=headers,
            timeout=timeout,
        )
    else:
        # requests streaming: stream=True
        return lib.post(url, json=payload, headers=headers, stream=True, timeout=timeout)


# ---------------------------------------------------------------------------
# OllamaBackend
# ---------------------------------------------------------------------------

class OllamaBackend(ILLMBackend):
    """
    Calls Ollama via its OpenAI-compatible /v1/chat/completions endpoint.

    Parameters
    ----------
    base_url : str
        Base URL for the Ollama API, e.g. "http://localhost:11434".
    model : str
        Ollama model name, e.g. "llama3.2".
    """

    def __init__(self, base_url="http://localhost:11434", model="llama3.2"):
        self.base_url = base_url.rstrip("/")
        self.model = model

    def is_available(self):
        """
        Return True if Ollama is reachable at base_url/api/tags.
        Returns False on any exception (connection error, library unavailable, etc.).
        """
        try:
            resp = _http_get("{0}/api/tags".format(self.base_url), timeout=2)
            return resp.status_code == 200
        except ImportError:
            _LOG.warning(
                "OllamaBackend.is_available: httpx or requests not installed — "
                "cannot check availability"
            )
            return False
        except Exception:
            return False

    def list_models(self):
        """
        Return a list of installed Ollama model names.
        Returns [] on any error.
        """
        try:
            resp = _http_get("{0}/api/tags".format(self.base_url), timeout=2)
            data = resp.json()
            models = data.get("models", [])
            return [m["name"] for m in models if isinstance(m, dict) and "name" in m]
        except Exception as exc:
            _LOG.warning("OllamaBackend.list_models error: %s", exc)
            return []

    def stream_chat(self, messages, tools):
        """
        Stream a chat completion from Ollama's OpenAI-compat endpoint.

        Yields TokenChunk, ToolCall, Done, or Error events.
        """
        lib_name, lib = _try_import_http()
        if lib is None:
            yield Error("httpx or requests required for OllamaBackend")
            return

        url = "{0}/v1/chat/completions".format(self.base_url)
        payload = {
            "model": self.model,
            "messages": messages,
            "stream": True,
        }
        if tools:
            payload["tools"] = tools

        try:
            yield from self._do_stream(lib_name, lib, url, payload)
        except Exception as exc:
            _LOG.error("OllamaBackend.stream_chat error: %s", exc)
            yield Error(str(exc))

    def _do_stream(self, lib_name, lib, url, payload):
        """Inner generator that handles the actual streaming, separated for cleaner error handling."""
        headers = {"Content-Type": "application/json"}

        if lib_name == "httpx":
            with lib.stream(
                "POST", url,
                content=json.dumps(payload).encode(),
                headers=headers,
                timeout=60,
            ) as resp:
                for line in resp.iter_lines():
                    event = self._parse_sse_line(line)
                    if event is not None:
                        yield event
                        if isinstance(event, Done):
                            return
        else:
            resp = lib.post(
                url, json=payload, headers=headers, stream=True, timeout=60
            )
            for line in resp.iter_lines(decode_unicode=True):
                event = self._parse_sse_line(line)
                if event is not None:
                    yield event
                    if isinstance(event, Done):
                        return

    def _parse_sse_line(self, line):
        """
        Parse a single SSE/NDJSON line from the completion stream.
        Returns a ChatEvent or None if the line should be skipped.
        """
        if not line:
            return None

        # Strip "data: " prefix if present (SSE format)
        if isinstance(line, bytes):
            line = line.decode("utf-8", errors="replace")
        line = line.strip()
        if not line:
            return None
        if line == "data: [DONE]":
            return Done()
        if line.startswith("data: "):
            line = line[6:]

        try:
            chunk = json.loads(line)
        except (ValueError, TypeError):
            return None

        choices = chunk.get("choices", [])
        if not choices:
            return None

        choice = choices[0]
        finish_reason = choice.get("finish_reason")
        delta = choice.get("delta", {})

        # Tool calls
        tool_calls = delta.get("tool_calls", [])
        if tool_calls:
            for tc in tool_calls:
                fn_info = tc.get("function", {})
                call_id = tc.get("id", "")
                fn_name = fn_info.get("name", "")
                fn_args_raw = fn_info.get("arguments", "{}")
                try:
                    fn_params = json.loads(fn_args_raw) if fn_args_raw else {}
                except (ValueError, TypeError):
                    fn_params = {}
                return ToolCall(call_id=call_id, fn=fn_name, params=fn_params)

        # Text content
        content = delta.get("content")
        if content:
            return TokenChunk(text=content, done=False)

        # Stop signal
        if finish_reason == "stop":
            return Done()

        return None


# ---------------------------------------------------------------------------
# ClaudeBackend
# ---------------------------------------------------------------------------

class ClaudeBackend(ILLMBackend):
    """
    Calls Anthropic Claude via the official anthropic Python SDK.

    Parameters
    ----------
    api_key : str or None
        Anthropic API key. Defaults to ANTHROPIC_API_KEY env var.
    model : str
        Claude model ID, e.g. "claude-haiku-4-5".
    """

    def __init__(self, api_key=None, model="claude-haiku-4-5"):
        self.api_key = api_key if api_key is not None else os.environ.get("ANTHROPIC_API_KEY")
        self.model = model

    def is_available(self):
        """Return True if an API key is configured."""
        return self.api_key is not None and len(self.api_key) > 0

    def list_models(self):
        """Return known Claude model identifiers."""
        return [
            "claude-haiku-4-5",
            "claude-sonnet-4-6",
            "claude-opus-4-8",
        ]

    def stream_chat(self, messages, tools):
        """
        Stream a chat completion from Anthropic Claude.

        Yields TokenChunk, ToolCall, Done, or Error events.
        Maps DiaEditorAPI tool dicts to Anthropic's tool format.
        """
        try:
            import anthropic
        except ImportError:
            yield Error("anthropic package not installed")
            return

        if not self.api_key:
            yield Error("ANTHROPIC_API_KEY not set")
            return

        client = anthropic.Anthropic(api_key=self.api_key)

        # Extract system prompt; remaining messages go to Anthropic as-is
        system_prompt = ""
        non_system_messages = []
        for msg in messages:
            if isinstance(msg, dict) and msg.get("role") == "system":
                content = msg.get("content", "")
                if content:
                    if system_prompt:
                        system_prompt = system_prompt + "\n" + content
                    else:
                        system_prompt = content
            else:
                non_system_messages.append(msg)

        # Map tool dicts to Anthropic format: {name, description, input_schema}
        anthropic_tools = []
        if tools:
            for tool in tools:
                if not isinstance(tool, dict):
                    continue
                anthropic_tool = {
                    "name": tool.get("name", ""),
                    "description": tool.get("description", ""),
                    "input_schema": tool.get("parameters", tool.get("input_schema", {"type": "object", "properties": {}})),
                }
                if anthropic_tool["name"]:
                    anthropic_tools.append(anthropic_tool)

        # Build kwargs
        create_kwargs = {
            "model": self.model,
            "max_tokens": 4096,
            "messages": non_system_messages,
        }
        if system_prompt:
            create_kwargs["system"] = system_prompt
        if anthropic_tools:
            create_kwargs["tools"] = anthropic_tools

        try:
            with client.messages.stream(**create_kwargs) as stream:
                for event in stream:
                    event_type = getattr(event, "type", None)

                    if event_type == "content_block_delta":
                        delta = getattr(event, "delta", None)
                        if delta is not None:
                            delta_type = getattr(delta, "type", None)
                            if delta_type == "text_delta":
                                text = getattr(delta, "text", "")
                                if text:
                                    yield TokenChunk(text=text, done=False)
                            elif delta_type == "input_json_delta":
                                # Tool input streaming — we collect at block stop
                                pass

                    elif event_type == "content_block_start":
                        block = getattr(event, "content_block", None)
                        if block is not None:
                            block_type = getattr(block, "type", None)
                            if block_type == "tool_use":
                                # Store partial tool use state on the stream object
                                # We'll yield it at content_block_stop
                                pass

                    elif event_type == "message_delta":
                        delta = getattr(event, "delta", None)
                        if delta is not None:
                            stop_reason = getattr(delta, "stop_reason", None)
                            if stop_reason == "end_turn":
                                yield Done()
                                return

                    elif event_type == "message_stop":
                        yield Done()
                        return

                # Collect the final message to extract any tool_use blocks
                final = stream.get_final_message()
                for block in getattr(final, "content", []):
                    block_type = getattr(block, "type", None)
                    if block_type == "tool_use":
                        call_id = getattr(block, "id", "")
                        fn_name = getattr(block, "name", "")
                        fn_params = getattr(block, "input", {})
                        if not isinstance(fn_params, dict):
                            fn_params = {}
                        yield ToolCall(call_id=call_id, fn=fn_name, params=fn_params)

                stop_reason = getattr(final, "stop_reason", None)
                if stop_reason != "tool_use":
                    yield Done()

        except Exception as exc:
            _LOG.error("ClaudeBackend.stream_chat error: %s", exc)
            yield Error(str(exc))


# ---------------------------------------------------------------------------
# GeminiBackend
# ---------------------------------------------------------------------------

class GeminiBackend(ILLMBackend):
    """
    Calls Google Gemini via the google-generativeai SDK.

    Parameters
    ----------
    api_key : str or None
        Google API key. Defaults to GOOGLE_API_KEY env var.
    model : str
        Gemini model name, e.g. "gemini-1.5-flash".
    """

    def __init__(self, api_key=None, model="gemini-1.5-flash"):
        self.api_key = api_key if api_key is not None else os.environ.get("GOOGLE_API_KEY")
        self.model = model

    def is_available(self):
        """Return True if an API key is configured."""
        return self.api_key is not None and len(self.api_key) > 0

    def list_models(self):
        """Return known Gemini model identifiers."""
        return [
            "gemini-1.5-flash",
            "gemini-1.5-pro",
            "gemini-2.0-flash",
        ]

    def stream_chat(self, messages, tools):
        """
        Stream a chat completion from Google Gemini.

        Yields TokenChunk, ToolCall, Done, or Error events.
        Converts messages and tools to Gemini format.
        """
        try:
            import google.generativeai as genai
        except ImportError:
            yield Error("google-generativeai package not installed")
            return

        if not self.api_key:
            yield Error("GOOGLE_API_KEY not set")
            return

        try:
            genai.configure(api_key=self.api_key)

            # Convert tools to Gemini function declarations format
            gemini_tools = None
            if tools:
                function_declarations = []
                for tool in tools:
                    if not isinstance(tool, dict):
                        continue
                    fn_name = tool.get("name", "")
                    if not fn_name:
                        continue
                    parameters = tool.get("parameters", tool.get("input_schema", {}))
                    fn_decl = {
                        "name": fn_name,
                        "description": tool.get("description", ""),
                    }
                    if parameters:
                        fn_decl["parameters"] = parameters
                    function_declarations.append(fn_decl)
                if function_declarations:
                    gemini_tools = [{"function_declarations": function_declarations}]

            # Convert messages to Gemini format
            # Gemini uses 'user' and 'model' roles; system prompt becomes first user turn
            gemini_messages = []
            system_parts = []
            for msg in messages:
                if not isinstance(msg, dict):
                    continue
                role = msg.get("role", "user")
                content = msg.get("content", "")
                if role == "system":
                    system_parts.append({"text": content})
                elif role == "assistant":
                    gemini_messages.append({"role": "model", "parts": [{"text": content}]})
                elif role == "tool":
                    # Tool result
                    gemini_messages.append({
                        "role": "user",
                        "parts": [{"text": content}],
                    })
                else:
                    gemini_messages.append({"role": "user", "parts": [{"text": content}]})

            # Prepend system prompt as a user message if present
            if system_parts:
                system_text = " ".join(p["text"] for p in system_parts)
                if gemini_messages and gemini_messages[0]["role"] == "user":
                    existing = gemini_messages[0]["parts"][0].get("text", "")
                    gemini_messages[0]["parts"][0]["text"] = system_text + "\n\n" + existing
                else:
                    gemini_messages.insert(0, {"role": "user", "parts": [{"text": system_text}]})

            model_obj = genai.GenerativeModel(self.model)

            generate_kwargs = {"stream": True}
            if gemini_tools:
                generate_kwargs["tools"] = gemini_tools

            response = model_obj.generate_content(gemini_messages, **generate_kwargs)

            for chunk in response:
                # Text parts
                for part in getattr(chunk, "parts", []) or []:
                    text = getattr(part, "text", None)
                    if text:
                        yield TokenChunk(text=text, done=False)

                    # Function call parts
                    fn_call = getattr(part, "function_call", None)
                    if fn_call is not None:
                        fn_name = getattr(fn_call, "name", "")
                        fn_args = getattr(fn_call, "args", {})
                        params = dict(fn_args) if fn_args else {}
                        call_id = fn_name  # Gemini doesn't provide a separate call ID
                        yield ToolCall(call_id=call_id, fn=fn_name, params=params)

            yield Done()

        except Exception as exc:
            _LOG.error("GeminiBackend.stream_chat error: %s", exc)
            yield Error(str(exc))


# ---------------------------------------------------------------------------
# Factory
# ---------------------------------------------------------------------------

def create_backend(name, model=None, **kwargs):
    """
    Instantiate an ILLMBackend by name.

    Parameters
    ----------
    name : str
        One of "ollama", "claude", "gemini".
    model : str or None
        Model name override. Falls back to backend-specific defaults.
    **kwargs
        Passed through to the backend constructor (e.g. base_url for OllamaBackend).

    Returns
    -------
    ILLMBackend

    Raises
    ------
    ValueError
        If name is not a recognised backend.
    """
    if name == "ollama":
        if model is not None:
            kwargs["model"] = model
        else:
            kwargs.setdefault("model", "llama3.2")
        return OllamaBackend(**kwargs)

    if name == "claude":
        if model is not None:
            kwargs["model"] = model
        else:
            kwargs.setdefault("model", "claude-haiku-4-5")
        return ClaudeBackend(**kwargs)

    if name == "gemini":
        if model is not None:
            kwargs["model"] = model
        else:
            kwargs.setdefault("model", "gemini-1.5-flash")
        return GeminiBackend(**kwargs)

    raise ValueError(
        "Unknown backend '{0}'. Expected one of: ollama, claude, gemini.".format(name)
    )
