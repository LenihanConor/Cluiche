---
name: fixup
description: Auto-diagnose and fix common build/test errors — missing includes, wrong signatures, typos.
tags: [fix, build, error, quick]
user_invocable: true
agent_invocable: true
---

# Fixup

Fast-path fix for common mechanical build/test errors. Unlike the full `/debug` skill, this doesn't form hypotheses — it pattern-matches known error types and applies the obvious fix.

## When This Activates

- After a failed `dia run` or `dia pipeline` that produced compiler/linker errors
- When the user says "fix it", "fixup", or invokes `/fixup`
- Agent-invocable: after a subagent build failure in the dispatch loop

## Usage

```
/fixup
/fixup <paste error output>
```

If no error is provided, read the last build output from the terminal.

## Instructions for Claude

### Step 1 — Classify the error

Read the error output. Match against these categories:

| Category | Pattern | Fix |
|----------|---------|-----|
| Missing include | `cannot open source file`, `undeclared identifier`, `incomplete type` | Add the missing `#include` |
| Wrong signature | `no matching function`, `cannot convert`, `too few/many arguments` | Fix the call site or declaration |
| Typo | `undeclared identifier` for a name close to an existing symbol | Correct the spelling |
| Missing source in vcxproj | `LNK2019 unresolved external` for a symbol that exists in a .cpp | Add the .cpp to vcxproj |
| Missing dependency | `LNK2019` for a symbol in another library | Add library to project references |
| Redefinition | `already defined in .obj` | Remove duplicate include or add pragma once |
| Namespace | `is not a member of` | Fix namespace qualification |

If the error doesn't match any category, say: "This doesn't look mechanical — switching to /debug." Then follow `.claude/skills/debug.md`.

### Step 2 — Locate the fix

- Missing includes: grep for the undeclared symbol to find which header declares it
- Wrong signatures: read the declaration to see the correct signature
- Typos: find the closest matching symbol in the relevant files
- vcxproj: confirm the .cpp exists but isn't in the project file

### Step 3 — Apply the fix

Make the minimal edit. One fix at a time. If there are multiple errors of the same type (e.g. 5 missing includes), fix them all in one pass.

### Step 4 — Rebuild

```bash
dia run <target> --build-only
```

### Step 5 — Report

- Fixed: "Fixed: <one-line summary>"
- Still broken: "First fix applied but new errors appeared. Running another pass." (loop to Step 1, max 3)
- 3 passes failed: "3 fixup passes didn't resolve it. Switching to /debug."

## Constraints

- Maximum 3 fix passes before escalating to /debug
- Only fix mechanical errors — design problems escalate immediately
- Never change public API signatures without flagging to the user
- Never add new dependencies without confirming correctness
