# Module Metadata Schema

**Last Updated:** 2026-04-01

Documentation of the `dia.module.v1` YAML schema used in module architecture files.

---

## Overview

All `.architecture.module.md` files use YAML frontmatter following the `dia.module.v1` schema.

**Schema Version:** `dia.module.v1`

**Purpose:**
- Machine-readable module metadata
- Dependency tracking
- Tooling support (dia_modules.py)
- Documentation generation

---

## Schema Definition

### Complete Example

```yaml
---
schema: dia.module.v1
module_id: dia.core.containers.arrays
name: Arrays
owner_team: TBD
layer: platform
status: active
maturity: stable

path: Dia/DiaCore/Containers/Arrays
language: cpp
parent_module_id: dia.core.containers

summary: >
  Fixed and dynamic array containers.

intent: >
  Provides two array types: Array<T, N> for fixed-size arrays and
  DynamicArray<T> for dynamically-sized arrays similar to std::vector.

responsibilities:
  - Provide fixed-size array (Array<T, N>)
  - Provide dynamic array (DynamicArray<T>)
  - Maintain array invariants (bounds checking in debug)
  - Support iteration (iterators, range-for)

non_responsibilities:
  - Thread safety (user must synchronize)
  - Specialized allocators (uses default new/delete)
  - Sorted containers (use other containers)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaCore/Containers/Arrays/Array.h
    - Dia/DiaCore/Containers/Arrays/DynamicArray.h
  namespaces:
    - Dia::Core::Containers
  entry_points:
    - Array<T, N>
    - DynamicArray<T>

dependencies:
  required:
    - dia.core.core
  optional: []
  forbidden:
    - dia.application
---

# Module Body

Markdown documentation follows YAML frontmatter.
```

---

## Field Reference

### Required Fields

#### schema
- **Type:** String
- **Value:** `"dia.module.v1"`
- **Purpose:** Identifies schema version
- **Example:** `schema: dia.module.v1`

---

#### module_id
- **Type:** String
- **Format:** Dot-separated hierarchy (e.g., `dia.core.containers.arrays`)
- **Purpose:** Unique module identifier
- **Rules:**
  - Must be unique across all modules
  - Should match directory hierarchy
  - Use lowercase with dots
- **Example:** `module_id: dia.core.containers.arrays`

---

#### name
- **Type:** String
- **Purpose:** Human-readable module name
- **Example:** `name: Arrays`

---

#### layer
- **Type:** Enum
- **Values:**
  - `platform` - Foundation layer (DiaCore, DiaMaths)
  - `framework` - Application framework (DiaApplication)
  - `application` - Application code (Cluiche)
  - `backend` - Backend implementations (DiaSFML)
- **Purpose:** Architectural layer classification
- **Example:** `layer: platform`

---

#### status
- **Type:** Enum
- **Values:**
  - `active` - In active development/use
  - `deprecated` - Being phased out
  - `experimental` - Experimental, unstable
  - `stub` - Minimal implementation
- **Purpose:** Module lifecycle status
- **Example:** `status: active`

---

#### path
- **Type:** String (relative path)
- **Purpose:** File system path to module
- **Rules:**
  - Relative to repository root
  - Use forward slashes
  - No trailing slash
- **Example:** `path: Dia/DiaCore/Containers/Arrays`

---

#### language
- **Type:** String
- **Values:** `cpp`, `python`, `typescript`, etc.
- **Purpose:** Primary implementation language
- **Example:** `language: cpp`

---

#### summary
- **Type:** String (multi-line)
- **Purpose:** One-sentence description
- **Format:** Use `>` for folded scalar
- **Example:**
  ```yaml
  summary: >
    Fixed and dynamic array containers.
  ```

---

### Optional Fields

#### owner_team
- **Type:** String
- **Default:** `"TBD"`
- **Purpose:** Team responsible for module
- **Example:** `owner_team: Platform Team`

---

#### maturity
- **Type:** Enum
- **Values:**
  - `dev` - Development (unstable API)
  - `stable` - Stable (API unlikely to change)
  - `legacy` - Old code (maintenance mode)
- **Default:** `dev`
- **Example:** `maturity: stable`

---

#### parent_module_id
- **Type:** String (module_id reference)
- **Purpose:** Parent module in hierarchy
- **Rules:**
  - Must refer to existing module
  - Creates parent-child relationship
- **Example:** `parent_module_id: dia.core.containers`

---

#### intent
- **Type:** String (multi-line)
- **Purpose:** Detailed purpose and goals
- **Format:** Use `>` for folded scalar
- **Example:**
  ```yaml
  intent: >
    Provides two array types: Array<T, N> for fixed-size arrays and
    DynamicArray<T> for dynamically-sized arrays similar to std::vector.
  ```

---

#### responsibilities
- **Type:** List of strings
- **Purpose:** What this module owns/provides
- **Example:**
  ```yaml
  responsibilities:
    - Provide fixed-size array (Array<T, N>)
    - Provide dynamic array (DynamicArray<T>)
    - Maintain array invariants (bounds checking)
  ```

---

#### non_responsibilities
- **Type:** List of strings
- **Purpose:** What this module explicitly does NOT handle
- **Example:**
  ```yaml
  non_responsibilities:
    - Thread safety (user must synchronize)
    - Specialized allocators
  ```

---

#### dependent_modules
- **Type:** List of strings (module_id references)
- **Purpose:** Child modules in hierarchy
- **Rules:**
  - Each must refer to existing module
  - Child's `parent_module_id` should point back
- **Example:**
  ```yaml
  dependent_modules:
    - dia.core.containers.arrays
    - dia.core.containers.hashtables
  ```

---

#### public_api
- **Type:** Object
- **Purpose:** Public API surface
- **Fields:**
  - `headers` (list) - Public header files
  - `namespaces` (list) - C++ namespaces
  - `entry_points` (list) - Key classes/functions
- **Example:**
  ```yaml
  public_api:
    headers:
      - Dia/DiaCore/Containers/Arrays/Array.h
    namespaces:
      - Dia::Core::Containers
    entry_points:
      - Array<T, N>
      - DynamicArray<T>
  ```

---

#### dependencies
- **Type:** Object
- **Purpose:** Module dependencies
- **Fields:**
  - `required` (list) - Must depend on these
  - `optional` (list) - May depend on these
  - `forbidden` (list) - Must NOT depend on these
- **Example:**
  ```yaml
  dependencies:
    required:
      - dia.core.core
    optional: []
    forbidden:
      - dia.application
  ```

---

## Field Categories

### Identification
- `schema` - Schema version
- `module_id` - Unique ID
- `name` - Human name

### Classification
- `layer` - Architectural layer
- `status` - Lifecycle status
- `maturity` - API stability

### Location
- `path` - File system path
- `language` - Implementation language

### Hierarchy
- `parent_module_id` - Parent module
- `dependent_modules` - Child modules

### Documentation
- `summary` - One-line description
- `intent` - Detailed purpose
- `responsibilities` - What it owns
- `non_responsibilities` - What it doesn't own

### API Surface
- `public_api.headers` - Public headers
- `public_api.namespaces` - Namespaces
- `public_api.entry_points` - Key classes/functions

### Dependencies
- `dependencies.required` - Must have
- `dependencies.optional` - May have
- `dependencies.forbidden` - Must not have

### Ownership
- `owner_team` - Responsible team

---

## Validation Rules

### module_id
- ✅ Must be unique
- ✅ Must match hierarchy pattern
- ✅ Should correspond to path

### parent_module_id
- ✅ Must refer to existing module (if present)
- ✅ Parent must list this module in `dependent_modules`

### dependent_modules
- ✅ Each must refer to existing module
- ✅ Each child should have `parent_module_id` pointing back

### dependencies.required
- ✅ Each must refer to existing module
- ✅ No circular dependencies (checked by tool)

### dependencies.forbidden
- ✅ Must not appear in actual code imports
- ✅ Used to prevent layer violations

### path
- ✅ Must be valid directory
- ✅ Should contain module code

---

## Common Patterns

### Root Module

```yaml
---
schema: dia.module.v1
module_id: dia.core
name: DiaCore
layer: platform
status: active

path: Dia/DiaCore
language: cpp
parent_module_id: null  # Root has no parent

dependent_modules:
  - dia.core.containers
  - dia.core.architecture
  - dia.core.type

dependencies:
  required: []
  forbidden: []
---
```

---

### Leaf Module (No Children)

```yaml
---
schema: dia.module.v1
module_id: dia.core.containers.arrays
name: Arrays
layer: platform
status: active

path: Dia/DiaCore/Containers/Arrays
language: cpp
parent_module_id: dia.core.containers

dependent_modules: []  # No children

dependencies:
  required:
    - dia.core.core
---
```

---

### Backend Implementation

```yaml
---
schema: dia.module.v1
module_id: dia.sfml
name: DiaSFML
layer: backend
status: active

path: Dia/DiaSFML
language: cpp

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.graphics
    - dia.window
    - dia.input
  forbidden:
    - dia.application  # Backend shouldn't know app
---
```

---

### Deprecated Module

```yaml
---
schema: dia.module.v1
module_id: dia.example.deprecated
name: ExampleDeprecated
layer: backend
status: deprecated  # Being phased out
maturity: legacy

path: Dia/DiaExampleDeprecated
language: cpp

summary: >
  Example deprecated module (DEPRECATED - use replacement).

dependencies:
  required:
    - dia.core
---

# Migration

**Deprecated:** This module is no longer maintained.

**Replacement:** Migrate to the replacement module.
```

---

## Tooling Support

### dia_modules.py

**Validate Schema:**
```bash
python Tools/dia_modules.py --validate
```

**List Module:**
```bash
python Tools/dia_modules.py --info dia.core.containers.arrays
```

**Check Cycles:**
```bash
python Tools/dia_modules.py --check-cycles
```

**Generate Graph:**
```bash
python Tools/dia_modules.py --graph output.dot
```

---

## File Structure

```
ModuleDirectory/
├── dia.parent.module.architecture.module.md  # This file
├── Header1.h
├── Header2.h
├── Source1.cpp
└── Source2.cpp
```

**File Name Pattern:**
- Format: `dia.<parent>.<module>.architecture.module.md`
- Must match `module_id` with dots replaced by dots
- Located in module directory

---

## Best Practices

### 1. Keep module_id Consistent with Path

```yaml
# Good
module_id: dia.core.containers.arrays
path: Dia/DiaCore/Containers/Arrays

# Bad (mismatch)
module_id: dia.core.arrays
path: Dia/DiaCore/Containers/Arrays
```

---

### 2. Document Boundaries Clearly

```yaml
responsibilities:
  - What this module provides
  
non_responsibilities:
  - What it explicitly does NOT provide
  - Common misconceptions
```

---

### 3. Use Forbidden Dependencies

```yaml
dependencies:
  forbidden:
    - dia.application  # Prevent layer violation
    - dia.graphics     # Prevent circular dependency
```

---

### 4. Keep Summary Concise

```yaml
# Good
summary: >
  Fixed and dynamic array containers.

# Bad (too verbose)
summary: >
  This module provides two types of array containers including a fixed-size
  array and a dynamic array that can grow and shrink at runtime...
```

---

### 5. List All Public Headers

```yaml
public_api:
  headers:
    - Dia/DiaCore/Containers/Arrays/Array.h
    - Dia/DiaCore/Containers/Arrays/DynamicArray.h
    - Dia/DiaCore/Containers/Arrays/ArrayIterator.h
```

---

## Migration from Old Format

If updating old module files:

1. Add `schema: dia.module.v1`
2. Ensure all required fields present
3. Add `dependencies.forbidden` for layer enforcement
4. Add `non_responsibilities` for clarity
5. Validate with `dia_modules.py --validate`

---

## Summary

**Schema:** `dia.module.v1`

**Required Fields:**
- `schema`, `module_id`, `name`, `layer`, `status`, `path`, `language`, `summary`

**Important Optional Fields:**
- `parent_module_id`, `dependent_modules`, `public_api`, `dependencies`

**Purpose:**
- Machine-readable metadata
- Dependency tracking
- Documentation generation
- Tooling support

**Tools:**
- `dia_modules.py` - Validation, visualization, analysis

**[→ Module Registry](module-registry.md)**  
**[→ Back to Documentation Index](../README.md)**
