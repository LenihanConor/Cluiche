Show or update the CoW (Crucible of Wings) backlog.

Follow `.claude/skills/cow-backlog.md`. Arguments: $ARGUMENTS

- No arguments: read `docs/specs/applications/cow/cow-backlog.md` and render Ready to Build, Still to Be Designed, Ready to Spec, Blocked, and What's Next.
- `done <item>`: move the row to `cow-backlog-history.md`.
- `designed <piece>`: strike the piece in "Features to Be Designed (GDD)" and record it in history (only when the user confirms the GDD design is locked).
- `drop <item> <why>`: move the row to history's Dropped table.
