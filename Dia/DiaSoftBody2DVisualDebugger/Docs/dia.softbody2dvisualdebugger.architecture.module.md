---
schema: dia.module.v1
module_id: dia.softbody2dvisualdebugger
name: DiaSoftBody2DVisualDebugger
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaSoftBody2DVisualDebugger
language: cpp
parent_module_id: dia.diavisualdebugger

summary: >
  Stack of four focused IVisualDebugger draw classes for DiaSoftBody2D simulation state.

intent: >
  Provides toggleable visual debugging layers for soft-body simulation:
  particles by pinning state, constraints by type, anchor links, and Verlet velocity.

responsibilities:
  - Draw particles coloured by pinning state (pinned=magenta, dynamic=white)
  - Draw active distance constraints coloured by ConstraintType
  - Draw rope anchor links in yellow
  - Draw per-frame Verlet velocity as line segments in green

non_responsibilities:
  - Does not simulate or own SoftBodyWorld state
  - Does not render UI or manage layer enable/disable state
  - Does not handle cloth/rope editing or selection

dependent_modules:
  - dia.core
  - dia.maths
  - dia.geometry2d
  - dia.rigidbody2d
  - dia.graphics
  - dia.softbody2d
  - dia.diavisualdebugger

public_api:
  headers:
    - DiaSoftBody2DVisualDebugger/SoftParticlesDrawer.h
    - DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.h
    - DiaSoftBody2DVisualDebugger/SoftAnchorLinksDrawer.h
    - DiaSoftBody2DVisualDebugger/SoftVelocityDrawer.h
  namespaces:
    - Dia::SoftBody2D
  entry_points:
    - SoftParticlesDrawer
    - SoftConstraintsDrawer
    - SoftAnchorLinksDrawer
    - SoftVelocityDrawer

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.geometry2d
    - dia.rigidbody2d
    - dia.graphics
    - dia.softbody2d
    - dia.diavisualdebugger
  forbidden: []
---
