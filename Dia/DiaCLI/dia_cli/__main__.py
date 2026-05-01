import sys
from pathlib import Path

# MDK adds the dia_cli/ directory to sys.path so unqualified imports like
# `from utils.X import Y` resolve correctly. When invoked via `python -m dia_cli`
# from an arbitrary cwd, neither the package dir nor its parent may be on sys.path.
# Insert both to mirror MDK's behaviour regardless of cwd.
_pkg_dir = Path(__file__).parent          # .../Dia/DiaCLI/dia_cli/
_cli_root = _pkg_dir.parent               # .../Dia/DiaCLI/
for _p in (_pkg_dir, _cli_root):
    _s = str(_p)
    if _s not in sys.path:
        sys.path.insert(0, _s)

from dia_cli.cli_main import main

if __name__ == "__main__":
    main()
