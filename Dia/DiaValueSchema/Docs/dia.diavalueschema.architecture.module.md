---
schema: dia.module.v1
module_id: dia.valueschema
name: DiaValueSchema
path: Dia/DiaValueSchema
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: stub
version: "1.0"
dependencies:
  required:
    - dia.core
    - dia.applicationflow
    - dia.streams
    - dia.observation
  forbidden: []
public_api:
  headers:
    - DiaValueSchema/ValueSchemaVersion.h
  namespaces:
    - Dia::ValueSchema::
  entry_points:
    - kFormatVersion
responsibilities:
  - Sidecar *.schema.json format definition and parsing
  - ValueInfo registry for storing and querying schema metadata
  - ValueSchemaRegistry for managing schema registrations
  - ValueSchemaModule and ValueSchemaService lifecycle integration
  - ClampAndWarn utility for value validation and warning logging
non_responsibilities:
  - Validation CLI tooling
  - Tooltip UI and visualization
  - Ramp/curve animation systems
  - Data export and serialization
---
# DiaValueSchema

Sidecar schema definition and registry system for the Dia engine.

Provides machine-readable value schema definitions via *.schema.json files that pair with game/editor data files. Includes schema parsing, metadata registry, and validation utilities.
