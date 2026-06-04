Show the current project backlog in a consistent four-section format.

Step 1 — Read `docs/BACKLOG.md` in full.

Step 2 — Render four sections derived from the file:

**Section 1 — Ready to Build**
Table of all systems/features with `Approved` specs not yet implemented. Columns: System, Features, Depends On. Include items even if dependencies aren't built yet (note in Depends On column).

**Section 2 — Ready to Spec**
Table of items needing spec work before they can be built. Columns: Item, What's Needed.

**Section 3 — Blocked**
Table of items that cannot proceed right now. Columns: Item, Why.

**Section 4 — What's Next / Ideas**
3-5 bullet points recommending the most logical next build, quick wins, and notable ideas. Derive from the dependency graph and what's actionable now.

Format rules:
- Use GitHub-flavored markdown tables
- Keep each table row to one line
- Do not include Done items (struck-through in the source)
- Do not show the "Deferred" section
- Do not show raw file paths or spec links
- If a section has no items, write "_Nothing here._"
