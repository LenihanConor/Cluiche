---
schema: dia.module.v1
module_id: draoi.gamespeed
name: DraoiGameSpeed
path: Draoi/DraoiGameSpeed
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: alpha
version: "1.0"
dependencies:
  required: []
  forbidden: []
public_api:
  headers: []
  namespaces:
    - Draoi::
  entry_points: []
responsibilities:
  - Data-driven calendar (ladder of named periods) and time bands
  - Speed presets, pause/modal state machine and day-boundary accumulator (pure state in, state out)
non_responsibilities:
  - Owning the clock (DiaSimTime) or any scheduling
  - Modules, bus messages, debug domain and save participant (application wiring)
  - Preset, calendar and band values (application config)
---
# DraoiGameSpeed

Header-only Draoi package (Utility project, no library output). Standard library only. `GameSpeedController.h` moves in from CoW/Shared under @docs/specs/features/draoi/extract-draoigamespeed.md.
