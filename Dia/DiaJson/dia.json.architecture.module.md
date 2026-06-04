---
schema: dia.module.v1
module_id: dia.json
name: DiaJson
owner_team: TBD
layer: foundation/core
status: active
maturity: dev

path: Dia/DiaJson
language: cpp

summary: >
  JSON parsing and writing via jsoncpp. Extracted from DiaCore to allow
  pure-computation modules to avoid JSON parsing dependencies.

intent: >
  Provide JSON infrastructure without pulling it into every module that
  depends on DiaCore.

responsibilities:
  - JSON value model (Json::Value)
  - JSON parsing (Json::Reader)
  - JSON writing (Json::Writer, Json::StyledWriter, Json::FastWriter)

non_responsibilities:
  - File I/O (DiaFileIO)
  - Serialization/reflection (DiaCore/Reflect)

dependencies:
  required:
    - dia.core
---
