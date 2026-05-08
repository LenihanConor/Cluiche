"""Implementation for editor-ui-test command."""
import pathlib
from dia_cli.utils.shell_utils import shell_run

_UI_DIR = pathlib.Path(__file__).resolve().parents[3] / "Cluiche" / "CluicheEditor" / "UI"

# Node shipped with Visual Studio — the only guaranteed Node on this machine.
_VS_NODE_DIR = (
    pathlib.Path("C:/Program Files/Microsoft Visual Studio/2022/Professional"
                 "/MSBuild/Microsoft/VisualStudio/NodeJs")
)
_NODE_EXE = _VS_NODE_DIR / "node.exe"
_NPM_CLI = _VS_NODE_DIR / "node_modules/npm/bin/npm-cli.js"


def _node_available() -> bool:
    return _NODE_EXE.exists()


def execute(watch: bool = False, coverage: bool = False) -> int:
    """Run the CluicheEditor UI Vitest suite.

    Returns the exit code (0 = all tests passed).
    """
    if not _node_available():
        print(f"ERROR: Node.js not found at {_NODE_EXE}")
        print("Install Node.js or update _VS_NODE_DIR in editor_ui_test_cmd.py")
        return 1

    # Build the vitest invocation. Use npx so the locally-installed vitest is used.
    vitest_args = "run"
    if watch:
        vitest_args = ""        # no 'run' = watch mode
    if coverage:
        vitest_args += " --coverage"

    # npx on Windows resolves to a batch shim that requires cmd.exe; invoke Node directly instead.
    vitest_main = _UI_DIR / "node_modules/vitest/vitest.mjs"
    cmd = f'"{_NODE_EXE}" "{vitest_main}" {vitest_args}'.strip()

    result = shell_run(cmd, cwd=_UI_DIR)
    return 0 if result.ok else result.return_code
