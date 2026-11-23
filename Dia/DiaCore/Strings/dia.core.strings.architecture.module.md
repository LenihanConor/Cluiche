---
schema: dia.module.v1
module_id: dia.core.strings
name: Strings
owner_team: TBD
layer: foundation/core
status: active
maturity: dev

path: Dia/DiaCore/Strings
language: cpp
parent_module_id: dia.core

summary: >
  Defines Strings APIs (String, String1024, String128, String256, String32, String512, String64, String8, stringutils) including: String, String1024, String128, String256, String32, String512, String64, String8.

intent: >
  Provide reusable Strings building blocks with consistent semantics for higher-level systems.

responsibilities:
  - Expose primary types: String, String1024, String128, String256, String32, String512, String64, String8
  - Define and maintain the public header surface for this module
  - Provide lightweight operations with predictable behavior

non_responsibilities:
  - Domain-specific gameplay behavior
  - Rendering or platform integration concerns (unless this module is explicitly an adapter)
  - High-level orchestration (owned by higher-layer modules)

dependent_modules:
  - dia.core.core
  - dia.core.containers.arrays

public_api:
  headers:
    - Dia/DiaCore/Strings/String.h
    - Dia/DiaCore/Strings/String1024.h
    - Dia/DiaCore/Strings/String128.h
    - Dia/DiaCore/Strings/String256.h
    - Dia/DiaCore/Strings/String32.h
    - Dia/DiaCore/Strings/String512.h
    - Dia/DiaCore/Strings/String64.h
    - Dia/DiaCore/Strings/String8.h
    - Dia/DiaCore/Strings/stringutils.h
    - Dia/DiaCore/Strings/Base64.h
  namespaces:
    - Dia::Dia
  entry_points:
    - String
    - String1024
    - String128
    - String256
    - String32
    - String512
    - String64
    - String8

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.core
  forbidden: []
---

## Text construction and assignment (decision, AD-001)

`String<N>` and the sized types (`String8`..`String1024`) treat `const char*` as plain text: `operator=(const char*)`, `SetText` and the single-argument `String64(cstr)` constructor copy and truncate to N-1 and never interpret `%`, so paths, keys and user input are safe. The printf-style constructor is kept but only exists with at least one extra argument (`String64("%s-%d", a, b)`, constrained template); use `Format`/`FormatArgs` to format into an existing string. `FormatArgs(fmt, ...)` exists because `Format(fmt, x)` with a single `char*` argument binds to the `va_list` overload.
