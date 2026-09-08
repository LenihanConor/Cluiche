---
name: pre-commit
description: Run mechanical validation checks before committing — deps, manifests, module docs, vcxproj sync.
tags: [validation, pre-commit, check, quality]
user_invocable: true
agent_invocable: true
---

# Pre-Commit

Runs a suite of mechanical validation checks to catch common errors before they're committed. Quality gate, not a build — checks structural consistency without compiling.

## When This Activates

- User invokes `/pre-commit`
- Agent-invocable: before creating a commit in the `/implement` loop
- Before creating a PR

## Usage

```
/pre-commit
/pre-commit --scope <path>
```

Without `--scope`, checks all modified files (from `git status`). With `--scope`, limits to files under that path.

## Instructions for Claude

### Step 1 — Determine scope

Run `git status` to find modified/added files. If `--scope` provided, filter to only files under that path.

### Step 2 — Run checks

#### Check 1: Module dependency consistency

For any modified `dia.*.architecture.module.md` OR modified `.h`/`.cpp` under `Dia/`:
- Parse the module.md for `dependent_modules`
- Scan includes in the modified source files
- Flag if a new include requires a dependency not declared in module.md

Run `dia check deps` scoped to changed modules, or replicate its logic inline.

#### Check 2: Manifest validity

For any modified `.diaapp`, `.diagame`, or `.diastage`:
- Parse as JSON
- Validate required fields exist and have correct types
- Check that referenced files (imports, manifest paths) exist on disk

Run `dia validate manifest --path <file>` for each, or replicate inline.

#### Check 3: Module doc exists

For any new `.h` file added under `Dia/`:
- Check that a corresponding `dia.*.architecture.module.md` exists for its parent module
- Catches code added to undocumented modules

#### Check 4: vcxproj sync

For any new `.h` or `.cpp` added under `Dia/` or `Cluiche/`:
- Check it appears in the corresponding `.vcxproj`
- Check it appears in the corresponding `.vcxproj.filters`

#### Check 5: No forbidden patterns

Scan modified `.h` and `.cpp` files for:
- `GetStatic()` or `sInstance` — warn (singleton without approval)
- `#include <iostream>` in engine code — fail (use DiaLog)
- `using namespace std;` — fail
- `TODO` without a task reference — warn (informational)

### Step 3 — Report

```
Pre-commit checks (N files in scope):

 Module dependencies    — consistent
 Manifest validity      — M manifests valid
 vcxproj sync           — all synced
 Module docs            — all modules documented
 Forbidden patterns     — clean

Result: PASS
```

Or on failure:
```
Pre-commit checks (N files in scope):

 Module dependencies    — consistent
 Manifest validity      — 2 manifests valid
 vcxproj sync           — NewFile.cpp missing from DiaCore.vcxproj
 Module docs            — all modules documented
 Forbidden patterns     — 1 warning (GetStatic in Foo.cpp:42)

Result: 1 FAIL, 1 WARN — fix vcxproj before committing.
```

### Step 4 — Offer fix

If any check failed with an auto-fixable issue (vcxproj sync, missing dep), offer: "I can fix this automatically. Want me to?"

## Check Severity

| Check | Severity | Blocks commit? |
|-------|----------|---------------|
| Module deps | Error | Yes |
| Manifest validity | Error | Yes |
| Module docs | Warning | No |
| vcxproj sync | Error | Yes |
| Forbidden patterns | Warning (except iostream/using namespace std) | Hard fails only |
