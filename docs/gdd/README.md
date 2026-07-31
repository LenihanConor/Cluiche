# GDD Cross-Reference Documents

Each file in this directory is a TDD (Technical Design Document) cross-reference derived
from a GDD (Game Design Document). It maps GDD requirements to engine capabilities.

Run `dia check gdd-sync` to validate all cross-reference files in this directory.

## File Naming

One file per game: `<game-slug>.md` (e.g. `dungeon-crawler.md`, `cow.md`).

## Table Format

Each section groups requirements by gameplay domain. Every row must follow this format:

```markdown
| # | GDD Requirement | Engine Capability | Spec | Status | Gap / Notes |
|---|---|---|---|---|---|
| M-01 | Plain-language description of what the game needs | What engine feature covers it | [SystemName](path/to/spec.md) | `built` | Leave blank if fully covered |
```

### Status values

| Value | Meaning |
|---|---|
| `built` | Shipped — spec status is Done |
| `partial` | Capability exists but a specific behaviour is unconfirmed — **Gap / Notes required** |
| `not-started` | No implementation yet |
| `out-of-scope` | Intentionally not an engine concern (game code only) |

### Rules `dia check gdd-sync` enforces

1. Every spec link resolves to a real file in the repo.
2. Every `Status` cell is one of the four valid values above.
3. Every `partial` row has a non-empty Gap / Notes cell.
4. No orphan spec links — the linked spec must exist on disk.

## Gap Summary Section

Every file must end with a `## Gap Summary` section containing three subsections:

- **Feature specs needed** — `partial` rows that need a feature spec written
- **Engine work needed** — `not-started` rows with no spec path
- **Backlog candidates** — `not-started` rows that have a spec and need scheduling

See `../reference/gdd-tdd-example.md` for a worked example.
