Run mechanical validation checks before committing — deps, manifests, module docs, vcxproj sync.

Usage: /pre-commit [--scope <path>]

Follow the full protocol in `.claude/skills/pre-commit.md`. Summary:

Checks (on modified/added files from `git status`, or scoped to `--scope` path):

1. **Module dependency consistency** — `dia check deps` on changed modules; flag undeclared deps
2. **Manifest validity** — `dia validate manifest` on any changed `.diaapp`/`.diagame`/`.diastage`
3. **Module docs** — new `.h` under `Dia/` must have a parent `dia.*.architecture.module.md`
4. **vcxproj sync** — new `.h`/`.cpp` must appear in `.vcxproj` and `.vcxproj.filters`
5. **Forbidden patterns** — `GetStatic()`/`sInstance` (warn), `#include <iostream>` (fail), `using namespace std;` (fail)

Report: checklist with PASS/FAIL/WARN per check. Offer auto-fix for mechanical failures (vcxproj, deps).
