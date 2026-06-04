---
schema: dia.module.v1
module_id: dia.fileio
name: DiaFileIO
owner_team: TBD
layer: foundation/core
status: active
maturity: dev

path: Dia/DiaFileIO
language: cpp

summary: >
  File path resolution, async file loading, file watching, and stream reader/writer.
  Extracted from DiaCore to allow pure-computation modules to avoid filesystem dependencies.

intent: >
  Provide file I/O infrastructure without pulling it into the base DiaCore dependency.

responsibilities:
  - File path abstraction and resolution (Path, FilePath)
  - Path store configuration and aliases (PathStore, PathStoreConfig)
  - Asynchronous file loading (AsyncFileLoader, FileLoad)
  - File system watching for hot-reload (FileWatcher)
  - Buffered stream reading and writing (StreamReader, StreamWriter)

non_responsibilities:
  - JSON parsing (DiaJson)
  - Serialization/reflection (DiaCore/Reflect)
  - Threading primitives (DiaCore/Threading)

dependencies:
  required:
    - dia.core
    - dia.core.threading
    - dia.core.containers.arrays
    - dia.core.containers.strings
    - dia.core.crc
    - dia.core.strings
---
