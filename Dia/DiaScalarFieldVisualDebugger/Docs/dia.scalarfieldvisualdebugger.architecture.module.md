---
schema: dia.module.v1
module_id: dia.scalarfieldvisualdebugger
name: DiaScalarFieldVisualDebugger
layer: domain/gameplay/tools
path: Dia/DiaScalarFieldVisualDebugger
status: active
maturity: dev
parent_module_id: dia.scalarfield

summary: >
  IDebugDomain implementation for DiaScalarField — heatmap and gradient
  world-space overlays.

dependencies:
  required:
    - dia.core
    - dia.scalarfield
  forbidden: []

public_api:
  headers:
    - DiaScalarFieldVisualDebugger/ScalarFieldVisualDebugger.h
  namespaces:
    - Dia::ScalarField
  entry_points:
    - ScalarFieldVisualDebugger

non_responsibilities:
  - Scalar field computation (DiaScalarField)
  - Any runtime behaviour in Release builds
---
