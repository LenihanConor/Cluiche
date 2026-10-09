---
name: cow-backlog
description: Show or update the CoW (Crucible of Wings) backlog — ready to build, features still to be designed in the GDD, spec work, blocked, and what's next
tags: [backlog, planning, specs, cow]
user_invocable: true
agent_invocable: false
---

# CoW Backlog Skill

Works on `docs/specs/applications/cow/cow-backlog.md` (active) and `docs/specs/applications/cow/cow-backlog-history.md` (completed). This is separate from the platform backlog (`/backlog`, `docs/BACKLOG.md`); `dia docs backlog move` does not handle the CoW files, so edit them directly.

## Usage

```
/cow-backlog                      show the backlog
/cow-backlog done <item>          move a finished item to history
/cow-backlog designed <piece>     cross out a piece the GDD has locked
/cow-backlog drop <item> <why>    move an item to history's Dropped section
```

## Instructions for Claude

### Show (no arguments)

Read `docs/specs/applications/cow/cow-backlog.md` in full and render five sections:

1. **Ready to Build** — columns: Item, Depends On. Items in the "Ready to Build" section.
2. **Still to Be Designed** — one row per testbed that still has un-struck pieces. Columns: Testbed, Pieces not yet designed. Omit struck-through pieces; omit a testbed whose pieces are all struck.
3. **Ready to Spec** — items in "Spec Work Needed" whose dependencies are met or already Approved. Columns: Item, What's Needed.
4. **Blocked** — items in "Spec Work Needed" whose "Depends On" is not yet done (including awaiting a GDD design). Columns: Item, Why (one sentence).
5. **What's Next** — 3 to 5 bullets: the most logical next build, quick wins, and anything needing a user decision (for example the undecided bus recorder placement). Derive from the dependency order; do not list everything.

Format rules: GitHub-flavored tables, one line per row, no raw file paths, omit struck-through (`~~`) items, write "_Nothing here._" for an empty section. Do not show the Deferred / Dropped section unless asked.

### done `<item>`

1. Find the row in cow-backlog.md whose first cell contains `<item>` (case-insensitive substring). If zero or several match, list them and ask which.
2. Remove the row. Add it to cow-backlog-history.md under "Completed Features and Systems" (or "Completed Tooling" for DiaCLI, skills and fixes), newest first, with the spec link, today's date, and a one-line note. Ask the user for the note if it is not obvious.
3. If the item was the last open row of a spec, remind the user to run `dia docs spec-done` for the spec.

### designed `<piece>`

1. Find the piece in the "Features to Be Designed (GDD)" table.
2. Strike it with `~~piece~~`. If every piece in that testbed row is struck, strike the testbed name too.
3. Add the piece to cow-backlog-history.md under "Designed in the GDD" with the date and a note.
4. Ask the user whether implementation work for it should be added to "Spec Work Needed" (usually a `/spec-system` row for the testbed); add it if so.
5. Only do this when the user says the GDD design is locked. Never infer it.

### drop `<item>` `<why>`

Remove the row from cow-backlog.md and add it to history's "Dropped" table with today's date and the reason.

### Always

- Keep cow-backlog.md and the history file consistent; show the user a short diff summary of what moved.
- Commit only if the user asks.
