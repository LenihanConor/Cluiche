---
name: backlog
description: Show the current project backlog — ready to build, ready to spec, blocked, and what's next
tags: [backlog, planning, specs]
user_invocable: true
agent_invocable: false
---

# Backlog Skill

Show the current project backlog in a consistent four-section format.

## Usage

```
/backlog
```

## Instructions for Claude

When this skill is invoked:

### 1. Read the Backlog

Read `docs/BACKLOG.md` in full.

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
