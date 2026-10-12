---
name: backlog
description: Show or update the project backlog (all projects, including CoW) — ready to build, ready to spec, blocked, what's next; done/designed/drop
tags: [backlog, planning, specs]
user_invocable: true
agent_invocable: false
---

# Backlog Skill

Show the current project backlog in a consistent four-section format.

## Usage

```
/backlog            all projects
/backlog cow        only rows whose Project column is CoW (also: dia, cluichetest, baseline)
/backlog done <item>          move a finished row to BACKLOG-HISTORY.md
/backlog designed <piece>     CoW: cross out a testbed piece the GDD has locked
/backlog drop <item> <why>    move a row to history's Dropped table
```

## Instructions for Claude

When this skill is invoked:

### 1. Read the Backlog

Read `docs/BACKLOG.md` in full. Every table has a **Project** column. If an argument is given, keep only rows whose Project contains it (case-insensitive); otherwise keep all rows. Show the Project as the first column of each table you render. For CoW, also include the `Dia` rows whose notes say "CoW needs", and read the "CoW: Features to Be Designed (GDD)" section and add a short "Still to be designed" table (Testbed, pieces not yet crossed out) after Blocked when the filter is cow.

### 2. Render Four Sections

Emit the following four sections in order, derived from the file. Strip any struck-through (~~) items — those are Done and should not appear. Do not show the "Deferred" section.

---

#### Section 1 — Ready to Build

A table of all systems/features with `Approved` specs that have not yet been implemented. Columns: **System**, **Features** (brief description), **Depends On**.

Only include items that are actionable now (dependencies already built). If an item's dependency isn't built yet, note that in the Depends On column but still include it.

#### Section 2 — Ready to Spec

A table of items that need spec work before they can be built. Columns: **Item**, **What's Needed** (one-line summary).

#### Section 3 — Blocked

A table of items that cannot proceed right now. Columns: **Item**, **Why** (one sentence).

#### Section 4 — What's Next / Ideas

3–5 bullet points with a brief recommendation on the most logical next build, any quick wins, and notable ideas or long-horizon items. Derive this from the dependency graph and what's actionable now — don't just list everything.

---

### 3. Format Rules

- Use GitHub-flavored markdown tables
- Keep each table row to one line — no multi-line cells
- Do not include Done items (struck-through in the source)
- Do not show raw file paths or spec links — keep it readable
- If a section has no items, write "_Nothing here._" under the heading

### 4. Updating the Backlog

**done `<item>`**: run `dia docs backlog move <item> --notes "<note>"` (use `--dry-run` first if several rows could match). Ask the user for the note if it is not obvious. If the item was the last open row of a spec, remind the user to run `dia docs spec-done` for the spec.

**designed `<piece>`** (CoW only):
1. Find the piece in "CoW: Features to Be Designed (GDD)" and strike it with `~~piece~~`.
2. If every piece in that testbed row is struck, remove the row, log the testbed in BACKLOG-HISTORY.md under "CoW: Designed in the GDD" with the date and a note, and (after confirming with the user) add its Testbed row to **Spec Work Needed → CoW** with Kind, Deliverable, GDD and Depends On.
3. Only do this when the user says the GDD design is locked. Never infer it.

**drop `<item>` `<why>`**: remove the row and add it to BACKLOG-HISTORY.md's "Dropped" table (Item, Project, Dropped date, Why).

**CoW row rule**: every CoW row is one System or one Testbed with one deliverable; sub-tasks belong in the spec and plan, not the backlog.

Commit only if the user asks; show a short summary of what moved.
