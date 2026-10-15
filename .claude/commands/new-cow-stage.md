Scaffold a new CoW testbed stage from the GDD.

Follow `.claude/skills/new-cow-stage.md`. Arguments: $ARGUMENTS

- The argument is a testbed id or name from the Systems table in `docs/specs/applications/cow/cow.md` (for example `S-2` or `Dragon`). Stop and ask if it is not listed.
- Read the GDD roster entry first and warn if the design is not locked.
- Run `dia scaffold stage <Name> --app CoW`, add panel stubs for the testbed's modes and presets (one stage, modes as panels), then `dia run cow` and confirm the stage launches and returns.
