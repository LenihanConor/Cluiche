"""Tests for dia_console/shell.py (console-native-shell, Goals 1 and 8).

The startup-handshake test spins up a real uvicorn thread against a small
FastAPI app on an ephemeral port -- no `dia` subprocess, no pywebview, no
real window, so it is not marked ``integration``.

The window-close tests simulate the close handler directly -- calling
``shell.shutdown()`` (the exact function ``launch()``'s
``window.events.closing`` handler invokes) and, in one test, driving
``launch()`` itself with a faked-out ``webview`` module -- instead of
driving a real GUI close event. Real native-window rendering is
CANNOT VERIFY PROGRAMMATICALLY in this environment (see the feature spec's
Acceptance Criteria note); a human must confirm the window actually opens,
is chromeless, and closes cleanly.
"""
from __future__ import annotations

import ast
import inspect

import pytest
from fastapi import FastAPI

from dia_console import shell


def _code_body_source(func) -> str:
    """``inspect.getsource(func)``'s statements, with its docstring dropped.

    Used to check the *executable* code doesn't hardcode something the
    docstring is allowed to mention in prose (e.g. explaining what a
    function never does).
    """
    tree = ast.parse(inspect.getsource(func))
    body = tree.body[0].body
    if (
        body
        and isinstance(body[0], ast.Expr)
        and isinstance(body[0].value, ast.Constant)
        and isinstance(body[0].value.value, str)
    ):
        body = body[1:]
    return "\n".join(ast.unparse(stmt) for stmt in body)


# ===========================================================================
# start_server -- the port-discovery startup handshake (Task 10 smoke test)
# ===========================================================================

def test_start_server_completes_handshake_and_binds_ephemeral_port():
    app = FastAPI()
    server, thread, port = shell.start_server(app, startup_timeout=5.0)
    try:
        assert thread.is_alive()
        assert isinstance(port, int)
        assert port > 0
    finally:
        server.should_exit = True
        thread.join(timeout=5.0)
    assert not thread.is_alive()


def test_start_server_never_binds_a_fixed_or_wildcard_host():
    assert inspect.signature(shell.start_server).parameters["host"].default == "127.0.0.1"
    code_only = _code_body_source(shell.start_server)
    assert "0.0.0.0" not in code_only
    assert "host=host" in code_only  # forwards the loopback-only default through to uvicorn.Config


def test_start_server_default_host_is_loopback_only():
    app = FastAPI()
    server, thread, port = shell.start_server(app, startup_timeout=5.0)
    try:
        sock = server.servers[0].sockets[0]
        assert sock.getsockname()[0] == "127.0.0.1"
    finally:
        server.should_exit = True
        thread.join(timeout=5.0)


# ===========================================================================
# shutdown() -- Goal 8: cancel every in-flight execution BEFORE stopping
# the uvicorn server. No orphaned `dia` subprocess may survive a close.
# ===========================================================================

class _FakeHandle:
    def __init__(self):
        self.cancel_called = False

    async def cancel(self):
        self.cancel_called = True


class _FakeServer:
    def __init__(self):
        self.should_exit = False


class _FakeThread:
    def __init__(self):
        self.joined_with_timeout = None

    def join(self, timeout=None):
        self.joined_with_timeout = timeout


def _app_with_executions(handles: dict) -> FastAPI:
    app = FastAPI()
    app.state.executions = handles
    return app


def test_shutdown_cancels_every_tracked_execution():
    handles = {"a": _FakeHandle(), "b": _FakeHandle()}
    app = _app_with_executions(handles)
    server = _FakeServer()
    thread = _FakeThread()

    shell.shutdown(app, server, thread, join_timeout=1.0)

    assert all(h.cancel_called for h in handles.values())
    assert server.should_exit is True
    assert thread.joined_with_timeout == 1.0


def test_shutdown_cancels_before_setting_should_exit():
    """Order matters: cancelling after should_exit risks a `dia` subprocess
    surviving past the server's own shutdown."""
    call_order = []

    class _OrderedHandle:
        async def cancel(self):
            call_order.append("cancel")

    app = _app_with_executions({"a": _OrderedHandle()})

    class _OrderedServer:
        def __init__(self):
            self._should_exit = False

        @property
        def should_exit(self):
            return self._should_exit

        @should_exit.setter
        def should_exit(self, value):
            call_order.append("should_exit")
            self._should_exit = value

    server = _OrderedServer()
    thread = _FakeThread()

    shell.shutdown(app, server, thread)

    assert call_order == ["cancel", "should_exit"]


def test_shutdown_joins_the_uvicorn_thread():
    app = _app_with_executions({})
    server = _FakeServer()
    thread = _FakeThread()

    shell.shutdown(app, server, thread, join_timeout=2.5)

    assert thread.joined_with_timeout == 2.5


def test_shutdown_with_no_in_flight_executions_does_not_raise():
    app = _app_with_executions({})
    server = _FakeServer()
    thread = _FakeThread()

    shell.shutdown(app, server, thread)  # must not raise

    assert server.should_exit is True


def test_cancel_all_executions_is_a_noop_for_empty_dict():
    shell.cancel_all_executions({})  # must not raise, no event loop required


def test_cancel_all_executions_cancels_every_handle():
    handles = {"a": _FakeHandle(), "b": _FakeHandle(), "c": _FakeHandle()}
    shell.cancel_all_executions(handles)
    assert all(h.cancel_called for h in handles.values())


# ===========================================================================
# launch() -- full wiring, with webview faked out (no real window/GUI)
# ===========================================================================

class _FakeEvent:
    """Stand-in for webview.event.Event -- supports `+=` and manual firing."""

    def __init__(self):
        self._callbacks = []

    def __iadd__(self, callback):
        self._callbacks.append(callback)
        return self

    def fire(self):
        for callback in list(self._callbacks):
            callback()


class _FakeWindowEvents:
    def __init__(self):
        self.closing = _FakeEvent()


class _FakeWindow:
    def __init__(self):
        self.events = _FakeWindowEvents()
        self.destroyed = False
        self.minimized = False
        self.fullscreen_toggled = False

    def destroy(self):
        self.destroyed = True

    def minimize(self):
        self.minimized = True

    def toggle_fullscreen(self):
        self.fullscreen_toggled = True


def test_launch_wires_window_close_to_cancel_then_shutdown(monkeypatch):
    """Task 12: simulate the close handler directly (no real GUI event)."""
    handle = _FakeHandle()
    app = _app_with_executions({"a": handle})

    fake_window = _FakeWindow()
    create_window_kwargs = {}

    def fake_create_window(title, **kwargs):
        create_window_kwargs["title"] = title
        create_window_kwargs.update(kwargs)
        return fake_window

    start_called = {"value": False}

    def fake_start():
        start_called["value"] = True
        # Simulate the user closing the window while webview.start() is
        # blocked in its event loop.
        fake_window.events.closing.fire()

    monkeypatch.setattr(shell.webview, "create_window", fake_create_window)
    monkeypatch.setattr(shell.webview, "start", fake_start)

    shell.launch(app=app, startup_timeout=5.0)

    assert start_called["value"] is True
    assert handle.cancel_called is True, "in-flight execution must be cancelled on window close"
    assert create_window_kwargs["frameless"] is True
    assert create_window_kwargs["url"].startswith("http://127.0.0.1:")
    assert create_window_kwargs["title"] == shell.WINDOW_TITLE


# ===========================================================================
# Custom titlebar controls -- regression guard for a real bug found live:
# the user reported the frameless window's own "X" button did nothing.
# window.pywebview.window does not exist anywhere in pywebview's actual
# bundled JS (webview/js/api.js) -- confirmed by reading it -- so
# app.js's minimize/maximize/close buttons, all gated on
# `window.pywebview && window.pywebview.window`, never even attached their
# click handlers. Every prior test in this file drove shutdown()/the
# closing *event* directly, never the button's own JS, which is exactly
# why this was never caught until a real click.
# ===========================================================================

def test_launch_passes_a_js_api_with_working_window_controls(monkeypatch):
    """launch() must expose a real js_api whose close()/minimize()/maximize()
    methods actually drive the real webview.Window -- window.pywebview.api
    is the only thing pywebview's own JS bridge really exposes to the page."""
    app = _app_with_executions({})
    fake_window = _FakeWindow()
    create_window_kwargs = {}

    def fake_create_window(title, **kwargs):
        create_window_kwargs.update(kwargs)
        return fake_window

    monkeypatch.setattr(shell.webview, "create_window", fake_create_window)
    monkeypatch.setattr(shell.webview, "start", lambda: None)

    shell.launch(app=app, startup_timeout=5.0)

    js_api = create_window_kwargs.get("js_api")
    assert js_api is not None, "create_window must be given a js_api -- window.pywebview.api is the only real bridge"

    js_api.close()
    assert fake_window.destroyed is True

    js_api.minimize()
    assert fake_window.minimized is True

    js_api.maximize()
    assert fake_window.fullscreen_toggled is True


def test_window_js_api_close_before_bind_does_not_raise():
    js_api = shell._WindowJsApi()
    js_api.close()  # must be a safe no-op, not an AttributeError on None


def test_app_js_titlebar_controls_use_the_real_pywebview_api_not_window():
    """Static guard: window.pywebview.window does not exist anywhere in
    pywebview's own bundled JS -- catches a future accidental revert to the
    broken call without needing a real GUI click to notice."""
    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    app_js = (repo_root / "Dia" / "DiaCLI" / "dia_console" / "web" / "static" / "app.js").read_text(encoding="utf-8")

    assert "pywebview.window.close(" not in app_js, "window.pywebview.window does not exist in pywebview's real JS bridge"
    assert "pywebview.window.minimize(" not in app_js
    assert "pywebview.window.toggleFullscreen(" not in app_js


def test_app_js_waits_for_pywebviewready_before_wiring_titlebar_controls():
    """Second real bug on top of the first: window.pywebview.api is injected
    asynchronously, with no guarantee it exists by the time this script's own
    DOMContentLoaded handler runs -- confirmed by an actual real DOM click
    that evaluated fine but did nothing, because the wiring code's guard was
    checked too early and never re-checked. pywebview dispatches its own
    'pywebviewready' event once the bridge is genuinely ready
    (webview/js/finish.js); this must be listened for as a fallback."""
    from dia_cli.utils.repo_root import find_repo_root

    repo_root = find_repo_root(__file__)
    app_js = (repo_root / "Dia" / "DiaCLI" / "dia_console" / "web" / "static" / "app.js").read_text(encoding="utf-8")

    assert "pywebviewready" in app_js
    assert "window.pywebview.api.close()" in app_js
    assert "window.pywebview.api.minimize()" in app_js
    assert "window.pywebview.api.maximize()" in app_js
