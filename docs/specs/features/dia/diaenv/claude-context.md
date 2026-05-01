# Feature Spec: claude-context

## Parent System
@docs/specs/systems/dia/diaenv.md

## Status
`Done`

## Summary

Implement `dia env claude-setup` — a command that provisions the Claude Code AI context for a fresh developer machine. It generates `.claude/settings.local.json` from a committed template (`.claude/settings.local.template.json`), and creates a symlink from the Claude Code user-profile memory directory to the in-repo memory directory (`C:\Users\<user>\.claude\projects\<repo-slug>\memory\` → `.claude\projects\C--GitHub-Cluiche\memory\`). This ensures a developer who clones the repo immediately has the same AI permissions and shared memory context without manual configuration.

## Problem

`.claude/settings.local.json` is machine-specific and gitignored — a new developer machine starts with no permissions configured and no AI memory context. The current `settings.local.json` is a large, organically-accumulated permission list that cannot be committed and would not be appropriate to share verbatim. The in-repo memory directory is authoritative, but Claude Code looks for memory in the user-profile path, not the repo — so without a symlink, a new machine gets no memory context.

## Current State

- `settings.local.json` exists at `.claude/settings.local.json` — gitignored, accumulated over time, machine-specific
- Memory lives at `C:\Users\clenihan\.claude\projects\C--GitHub-Cluiche\memory\` and is currently **not** symlinked from the repo
- No template exists yet — `.claude/settings.local.template.json` needs to be created and committed

## Goals

- Commit `.claude/settings.local.template.json` with a clean, minimal set of well-reasoned permissions
- `dia env claude-setup` generates `.claude/settings.local.json` from the template (does not overwrite if already present, unless `--force`)
- `dia env claude-setup` creates a symlink: `C:\Users\<user>\.claude\projects\C--GitHub-Cluiche\memory\` → `.claude\projects\C--GitHub-Cluiche\memory\` (in-repo)
- User profile path computed dynamically from the repo's absolute path — no hardcoded usernames
- `dia env verify` checks both: template-vs-generated drift, and symlink validity

## Non-Goals

- Managing individual permission entries — template is committed manually; this feature only generates from it
- Syncing `settings.local.json` across machines — template is the shared source; local file is machine-specific
- Backing up or migrating existing `settings.local.json` — developer's responsibility
- Cross-platform paths — Windows only (PD-005); `%USERPROFILE%` and `mklink` are Windows-specific

## CLI Interface

```bash
# Generate settings.local.json from template + create memory symlink
dia env claude-setup

# Force overwrite even if settings.local.json already exists
dia env claude-setup --force

# Verify AI context health (read-only)
dia env verify --claude
```

## Template Design

`.claude/settings.local.template.json` is committed to the repo. It contains a clean, minimal permissions baseline — not the accumulated list from the current `settings.local.json`.

```json
{
  "permissions": {
    "allow": [
      "Bash(git:*)",
      "Bash(msbuild:*)",
      "Bash(python:*)",
      "Bash(poetry:*)",
      "Bash(npm:*)",
      "Bash(node:*)",
      "Bash(ls:*)",
      "Bash(find:*)",
      "Bash(grep:*)",
      "Bash(cat)",
      "Bash(powershell:*)",
      "Bash(pwsh:*)",
      "Read(//c/GitHub/Cluiche/**)",
      "Read(//c/Program Files/Microsoft Visual Studio/**)",
      "Read(//c/Users/**)"
    ],
    "defaultMode": "auto"
  }
}
```

The template is intentionally minimal and principled — individual `Bash(specific-command:*)` entries from the current `settings.local.json` are not carried over. Developers accumulate additional permissions over time in their local file.

## Memory Symlink

Claude Code stores project memory at:
```
C:\Users\<user>\.claude\projects\<repo-slug>\memory\
```

Where `<repo-slug>` is derived from the repo's absolute path by replacing path separators with `--` and colons with `-`. For `C:\GitHub\Cluiche`:
```
C--GitHub-Cluiche
```

The in-repo memory directory is at:
```
.claude\projects\C--GitHub-Cluiche\memory\
```

`dia env claude-setup` computes the user-profile memory path dynamically:
```python
import os
from pathlib import Path

repo_root = Path(__file__).resolve().parents[4]  # walk up to repo root
slug = str(repo_root).replace("\\", "-").replace(":", "-").replace("/", "-")
profile_memory = Path(os.environ["USERPROFILE"]) / ".claude" / "projects" / slug / "memory"
repo_memory = repo_root / ".claude" / "projects" / slug / "memory"
```

Then creates the symlink using `os.symlink` (requires Developer Mode or admin on Windows).

## Implementation

### Files introduced / modified

```
.claude/
└── settings.local.template.json     # New — committed minimal permissions template

Dia/DiaCLI/
└── dia_cli/
    ├── cli/
    │   └── env/
    │       └── claude_cmd.py         # New — Click command: dia env claude-setup
    └── commands/
        └── env/
            └── claude_context_cmd.py # New — template generation + symlink creation
```

### `claude_context_cmd.py` responsibilities

1. Check if `.claude/settings.local.json` exists; skip generation if present (unless `--force`)
2. Read `.claude/settings.local.template.json`; copy to `.claude/settings.local.json`
3. Compute user-profile memory path dynamically from repo root + `%USERPROFILE%`
4. Check if profile memory path already exists and is a symlink; skip if correct (unless `--force`)
5. If profile memory path is a directory (not a symlink), warn and do not overwrite
6. Create symlink: profile memory path → in-repo memory directory
7. Requires Developer Mode or elevation for symlink creation on Windows; exit 1 with clear message if neither

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| `.claude/settings.local.template.json` | Hard | Must be created and committed as part of this feature |
| `env-verify-cmd` feature | Integration | `dia env verify --claude` checks template drift and symlink validity |
| Windows Developer Mode or admin | Runtime | Required for `os.symlink` on Windows without elevation |

## Acceptance Criteria

1. `.claude/settings.local.template.json` is committed to the repo with a clean minimal permissions set
2. `dia env claude-setup` generates `.claude/settings.local.json` from the template; exits 0 on success
3. Re-running `dia env claude-setup` without `--force` skips generation if `settings.local.json` already exists; prints a message
4. `dia env claude-setup --force` overwrites `settings.local.json` from the template
5. `dia env claude-setup` creates a symlink at the correct user-profile memory path pointing to the in-repo memory directory
6. The user-profile memory path is computed dynamically — no hardcoded usernames in code or template
7. If the profile memory path is already a valid symlink to the correct target, `dia env claude-setup` reports it as already configured and exits 0
8. If the profile memory path is a plain directory (not a symlink), `dia env claude-setup` warns and does not overwrite; exits 2
9. `dia env verify --claude` reports PASS if `settings.local.json` exists and symlink is valid; FAIL if either is missing
10. `dia env verify --claude` reports WARN if `settings.local.json` exists but differs structurally from the template (keys missing or extra)

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaEnv | @docs/specs/systems/dia/diaenv.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | Use StringCRC for all identifiers | Compliant — Python tooling only; no C++ identifiers introduced |
| PD-002 | Platform | ProcessingUnit/Phase/Module architecture | Compliant — development tooling; no runtime components |
| PD-003 | Platform | Component-based entities | Compliant — development tooling; no entity system used |
| PD-004 | Platform | No STL containers in public APIs | Compliant — Python only; no C++ public API surface |
| PD-005 | Platform | x64 Windows only | Compliant — `%USERPROFILE%`, `mklink`, and `os.symlink` are Windows-specific; no cross-platform support |
| PD-006 | Platform | VS project files are source of truth | Compliant — no `.vcxproj` files touched |
| PD-007 | Platform | C++20 required | Compliant — tooling feature; no compiler configuration touched |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir | Compliant — no build output paths modified |
| AD-001 | Dia App | Module system with YAML frontmatter | Compliant — Python tooling; no new C++ module introduced |
| SD-CLI-001 | DiaCLI | MDK CLI architecture | Compliant — `claude_cmd.py` follows the two-file plugin pattern |
| SD-CLI-002 | DiaCLI | Python-based implementation | Compliant — all implementation is Python |
| SD-CLI-003 | DiaCLI | Separate from C++ DiaAPI | Compliant — no C++ DiaAPI commands introduced |
| SD-CLI-006 | DiaCLI | Click framework for argument parsing | Compliant — `claude_cmd.py` uses Click decorators |
| SD-CLI-008 | DiaCLI | Exit codes follow Unix conventions | Compliant — exits 0 on success, 1 on hard failure, 2 on warnings |
| SD-ENV-006 | DiaEnv | `settings.local.json` never committed; template committed instead | Compliant — this feature implements SD-ENV-006; template committed, generated file gitignored |
| SD-ENV-010 | DiaEnv | Python 3.11 is the single Python version | Compliant — tooling feature; no Python version constraint introduced |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Symlink | Does `os.symlink` on Windows require Developer Mode or admin elevation? | Yes — on Windows, creating symlinks requires either Developer Mode enabled (no elevation needed) or running as Administrator. `claude_context_cmd.py` detects which is available; exits 1 with instructions if neither. |
| 2 | Template | Should the template be applied as a full overwrite or a merge with existing `settings.local.json`? | Full overwrite when `--force` is used. Without `--force`, skip entirely if file exists. Merge logic would be complex and error-prone; developers manage their local permissions manually. |
| 3 | Repo slug | The repo slug derivation assumes a specific path separator replacement scheme — is this the same scheme Claude Code uses internally? | The scheme (`replace \\ with --, replace : with -`) matches the observed path `C--GitHub-Cluiche` for `C:\GitHub\Cluiche`. Must be verified against Claude Code's actual slug generation at implementation time. |
| 4 | Template permissions | Should the template include the accumulated permissions from the current `settings.local.json`, or start fresh? | Start fresh with a minimal, principled set. The accumulated list contains many one-off commands that are not appropriate for a shared baseline. Developers will re-accumulate project-specific permissions naturally. |
| 5 | verify drift detection | How should `dia env verify --claude` detect "structural drift" between template and generated file? | Compare top-level keys only — check that every key present in the template also exists in `settings.local.json`. Do not compare values (local file may have more permissions than template). Missing keys = WARN. |
