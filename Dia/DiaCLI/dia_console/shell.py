"""Native window shell for DiaConsole (Goal 1, Goal 8, SD-CONSOLE-003).

Wraps ``dia_console.web.app``'s FastAPI app in a daemon uvicorn thread bound
to ``127.0.0.1`` on an ephemeral port, then opens a chromeless ``pywebview``
window pointed at it. On window close, every tracked in-flight
:class:`ExecutionHandle` is cancelled *before* uvicorn is asked to shut down
-- closing the window must never leave an orphaned ``dia`` subprocess tree
running.
"""
from __future__ import annotations

import asyncio
import threading

import uvicorn
import webview
from fastapi import FastAPI

from dia_console.execution import ExecutionHandle
from dia_console.web.app import app as _default_app

#: Judgment call, not measured (Open Design Question 2 in the feature spec)
#: -- revisit if it ever actually fires in practice.
DEFAULT_STARTUP_TIMEOUT_SECONDS = 5.0
DEFAULT_SHUTDOWN_JOIN_TIMEOUT_SECONDS = 5.0

WINDOW_TITLE = "Dia Console"
WINDOW_WIDTH = 1180
WINDOW_HEIGHT = 720


class _NoSignalServer(uvicorn.Server):
    """``uvicorn.Server`` with signal-handler installation disabled, and a
    ``threading.Event`` signalled once the listen socket is actually bound.

    Uvicorn installs SIGINT/SIGTERM handlers by default, which only works
    from the interpreter's main thread -- this server always runs in a
    daemon thread instead, so installing them would raise on some uvicorn
    versions/platforms (this version's own ``capture_signals()`` already
    guards for that, but the override is kept for defensiveness across
    versions).

    The readiness signal deliberately does *not* come from a FastAPI/ASGI
    ``lifespan`` "startup" hook on ``app`` itself: uvicorn awaits
    ``self.lifespan.startup()`` (which is what fires that ASGI event)
    *before* it creates the listen socket -- ``self.servers`` does not exist
    yet at that point, so reading ``server.servers[0]`` from an app-level
    lifespan hook raises ``AttributeError``. Overriding this class's own
    ``startup()`` instead lets us signal *after* ``super().startup()``
    returns, once the socket really is bound -- still event-driven via a
    ``threading.Event``, never polled/guessed.
    """

    def __init__(self, config: uvicorn.Config, *, ready: threading.Event, port_box: dict) -> None:
        super().__init__(config)
        self._ready = ready
        self._port_box = port_box

    def install_signal_handlers(self) -> None:  # noqa: D102 - uvicorn API
        pass

    async def startup(self, sockets=None) -> None:  # noqa: D102 - uvicorn API
        await super().startup(sockets=sockets)
        if self.servers and self.servers[0].sockets:
            self._port_box["port"] = self.servers[0].sockets[0].getsockname()[1]
        self._ready.set()


def start_server(
    app: FastAPI,
    *,
    host: str = "127.0.0.1",
    startup_timeout: float = DEFAULT_STARTUP_TIMEOUT_SECONDS,
) -> tuple[uvicorn.Server, threading.Thread, int]:
    """Start ``app`` under uvicorn in a daemon thread on an ephemeral port.

    Blocks until the server signals its socket is bound (via a
    ``threading.Event``, never polled/guessed) or ``startup_timeout``
    elapses -- fails loudly (``RuntimeError``) rather than hanging or guessing
    a port. Never binds to a fixed port, and never to anything but ``host``
    (never ``0.0.0.0``).

    Returns ``(server, thread, port)``.
    """
    ready = threading.Event()
    port_box: dict[str, int] = {}

    # log_config=None skips uvicorn's own dictConfig-based logging setup
    # entirely -- DiaConsole never reads uvicorn's console log output (it
    # uses its own NDJSON/log-tail mechanism), and that setup unconditionally
    # builds a StreamHandler over sys.stderr, which is None in a windowed
    # (console=False) frozen build (see packaging/entrypoint.py's docstring).
    config = uvicorn.Config(app, host=host, port=0, log_level="warning", log_config=None)
    server = _NoSignalServer(config, ready=ready, port_box=port_box)
    thread = threading.Thread(target=server.run, daemon=True, name="dia-console-uvicorn")
    thread.start()

    if not ready.wait(timeout=startup_timeout) or "port" not in port_box:
        raise RuntimeError(
            f"DiaConsole server did not start within {startup_timeout}s "
            "(uvicorn never signalled readiness)."
        )
    return server, thread, port_box["port"]


def cancel_all_executions(executions: dict[str, ExecutionHandle]) -> None:
    """Cancel every tracked in-flight execution. Safe to call with none running."""
    handles = list(executions.values())
    if not handles:
        return

    async def _cancel_all() -> None:
        await asyncio.gather(*(handle.cancel() for handle in handles), return_exceptions=True)

    asyncio.run(_cancel_all())


def shutdown(
    app: FastAPI,
    server: uvicorn.Server,
    thread: threading.Thread,
    *,
    join_timeout: float = DEFAULT_SHUTDOWN_JOIN_TIMEOUT_SECONDS,
) -> None:
    """Cancel every in-flight execution, then stop the uvicorn server (Goal 8).

    Order matters: every tracked execution is cancelled *before*
    ``should_exit`` is set, so no ``dia`` subprocess tree is left running
    once this returns. This is the exact function the window's ``closing``
    handler calls in :func:`launch` -- tests call it directly to simulate a
    window close without driving a real GUI event.
    """
    cancel_all_executions(dict(app.state.executions))
    server.should_exit = True
    thread.join(timeout=join_timeout)


def launch(
    *,
    app: FastAPI | None = None,
    startup_timeout: float = DEFAULT_STARTUP_TIMEOUT_SECONDS,
    shutdown_join_timeout: float = DEFAULT_SHUTDOWN_JOIN_TIMEOUT_SECONDS,
) -> None:
    """Launch the chromeless DiaConsole window (Goal 1). Blocks until it closes.

    On close: cancels every in-flight execution, then shuts uvicorn down (see
    :func:`shutdown`) -- no orphaned ``dia`` subprocess survives the window
    closing (Goal 8).
    """
    fastapi_app = app if app is not None else _default_app
    server, thread, port = start_server(fastapi_app, startup_timeout=startup_timeout)

    window = webview.create_window(
        WINDOW_TITLE,
        url=f"http://127.0.0.1:{port}",
        frameless=True,
        width=WINDOW_WIDTH,
        height=WINDOW_HEIGHT,
    )
    window.events.closing += lambda: shutdown(
        fastapi_app, server, thread, join_timeout=shutdown_join_timeout
    )

    webview.start()
