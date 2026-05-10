# Module Registry

**Last Updated:** 2026-04-01

Complete catalog of all 56 module architecture files in the Cluiche codebase.

---

## Overview

This registry catalogs all `.architecture.module.md` files that describe modules using the `dia.module.v1` YAML schema.

**Schema Documentation:** [Module Metadata Schema](module-metadata-schema.md)

**Total Modules:** 56

---

## Module Hierarchy

### Cluiche Application Layer

#### dia.cluiche (Root)
- **Path:** `Cluiche/CluicheTest/`
- **File:** `dia.cluiche.architecture.module.md`
- **Children:** dia.cluiche.levels, dia.cluiche.applicationflow, dia.cluiche.kernel

---

#### dia.cluiche.levels
- **Path:** `Cluiche/Levels/`
- **File:** `dia.cluiche.levels.architecture.module.md`
- **Children:**
  - `dia.cluiche.levels.dummylevel`
  - `dia.cluiche.levels.unittestlevel`

##### dia.cluiche.levels.dummylevel
- **Path:** `Cluiche/Stages/DummyStage/`
- **File:** `dia.cluiche.levels.dummylevel.architecture.module.md`
- **Purpose:** Simple example level

##### dia.cluiche.levels.unittestlevel
- **Path:** `Cluiche/Levels/UnitTestLevel/`
- **File:** `dia.cluiche.levels.unittestlevel.architecture.module.md`
- **Purpose:** In-engine test harness

---

#### dia.cluiche.applicationflow
- **Path:** `Cluiche/ApplicationFlow/`
- **File:** `dia.cluiche.applicationflow.architecture.module.md`
- **Children:**
  - `dia.cluiche.applicationflow.processingunits`
  - `dia.cluiche.applicationflow.phases`

##### dia.cluiche.applicationflow.processingunits
- **Path:** `Cluiche/ApplicationFlow/ProcessingUnits/`
- **File:** `dia.cluiche.applicationflow.processingunits.architecture.module.md`
- **Children:**
  - `dia.cluiche.applicationflow.processingunits.main`
  - `dia.cluiche.applicationflow.processingunits.render`
  - `dia.cluiche.applicationflow.processingunits.sim`

##### dia.cluiche.applicationflow.phases
- **Path:** `Cluiche/ApplicationFlow/Phases/`
- **File:** `dia.cluiche.applicationflow.phases.architecture.module.md`
- **Children:** (Various phase implementations)

---

#### dia.cluiche.kernel
- **Path:** `Cluiche/CluicheKernel/`
- **File:** `dia.cluiche.kernel.architecture.module.md`
- **Children:**
  - `dia.cluiche.kernel.applicationflow.modules`

##### dia.cluiche.kernel.applicationflow.modules
- **Path:** `Cluiche/CluicheKernel/ApplicationFlow/Modules/`
- **Purpose:** Core application modules (Main, Render, Sim)

---

## Dia Engine Layer

### dia.application
- **Path:** `Dia/DiaApplicationFlow/`
- **File:** `dia.application.architecture.module.md`
- **Purpose:** Application framework (ProcessingUnit/Phase/Module)
- **No Children** (single subsystem)

---

### dia.core
- **Path:** `Dia/DiaCore/`
- **File:** `dia.core.architecture.module.md`
- **Children:**
  - `dia.core.containers`
  - `dia.core.architecture`
  - `dia.core.type`
  - `dia.core.crc`
  - `dia.core.time`
  - `dia.core.timer`
  - `dia.core.core`
  - `dia.core.memory`
  - `dia.core.filepath`
  - `dia.core.json`

#### dia.core.containers
- **Path:** `Dia/DiaCore/Containers/`
- **File:** `dia.core.containers.architecture.module.md`
- **Children:**
  - `dia.core.containers.arrays`
  - `dia.core.containers.hashtables`
  - `dia.core.containers.linklists`
  - `dia.core.containers.bitflag`
  - `dia.core.containers.graphs`
  - `dia.core.containers.strings`

##### dia.core.containers.arrays
- **Path:** `Dia/DiaCore/Containers/Arrays/`
- **File:** `dia.core.containers.arrays.architecture.module.md`
- **Key Classes:** `Array<T, N>`, `DynamicArray<T>`

##### dia.core.containers.hashtables
- **Path:** `Dia/DiaCore/Containers/HashTables/`
- **File:** `dia.core.containers.hashtables.architecture.module.md`
- **Key Classes:** `HashTable<K, V>`

##### dia.core.containers.linklists
- **Path:** `Dia/DiaCore/Containers/LinkLists/`
- **File:** `dia.core.containers.linklists.architecture.module.md`
- **Key Classes:** `LinkList<T>`

##### dia.core.containers.bitflag
- **Path:** `Dia/DiaCore/Containers/BitFlag/`
- **File:** `dia.core.containers.bitflag.architecture.module.md`
- **Key Classes:** `BitFlag<N>`

##### dia.core.containers.graphs
- **Path:** `Dia/DiaCore/Containers/Graphs/`
- **File:** `dia.core.containers.graphs.architecture.module.md`
- **Key Classes:** `Graph<T>`

##### dia.core.containers.strings
- **Path:** `Dia/DiaCore/Containers/Strings/`
- **File:** `dia.core.containers.strings.architecture.module.md`
- **Key Classes:** `String`, `StringWriter`

---

#### dia.core.architecture
- **Path:** `Dia/DiaCore/Architecture/`
- **File:** `dia.core.architecture.architecture.module.md`
- **Children:**
  - `dia.core.architecture.singleton`
  - `dia.core.architecture.factory`
  - `dia.core.architecture.observer`
  - `dia.core.architecture.functor`
  - `dia.core.architecture.components`

##### dia.core.architecture.singleton
- **Path:** `Dia/DiaCore/Architecture/Singleton/`
- **File:** `dia.core.architecture.singleton.architecture.module.md`
- **Key Classes:** `Singleton<T>`

##### dia.core.architecture.factory
- **Path:** `Dia/DiaCore/Architecture/Factory/`
- **File:** `dia.core.architecture.factory.architecture.module.md`
- **Key Classes:** `Factory<T>`

##### dia.core.architecture.observer
- **Path:** `Dia/DiaCore/Architecture/Observer/`
- **File:** `dia.core.architecture.observer.architecture.module.md`
- **Key Classes:** `Observer`, `ObserverSubject`

##### dia.core.architecture.functor
- **Path:** `Dia/DiaCore/Architecture/Functor/`
- **File:** `dia.core.architecture.functor.architecture.module.md`
- **Key Classes:** `Functor<T>`

##### dia.core.architecture.components
- **Path:** `Dia/DiaCore/Architecture/Components/`
- **File:** `dia.core.architecture.components.architecture.module.md`
- **Key Classes:** `IComponent`, `ComponentFactoryRegistry`, `StaticPooledComponentFactory`

---

#### dia.core.type
- **Path:** `Dia/DiaCore/Type/`
- **File:** `dia.core.type.architecture.module.md`
- **Key Classes:** `TypeDefinition`, `TypeRegistry`

#### dia.core.crc
- **Path:** `Dia/DiaCore/CRC/`
- **File:** `dia.core.crc.architecture.module.md`
- **Key Classes:** `StringCRC`

#### dia.core.time
- **Path:** `Dia/DiaCore/Time/`
- **File:** `dia.core.time.architecture.module.md`
- **Key Classes:** `TimeServer`, `TimeAbsolute`

#### dia.core.timer
- **Path:** `Dia/DiaCore/Timer/`
- **File:** `dia.core.timer.architecture.module.md`
- **Key Classes:** `TimeRelative`

#### dia.core.core
- **Path:** `Dia/DiaCore/Core/`
- **File:** `dia.core.core.architecture.module.md`
- **Key Files:** `Assert.h`, `Log.h`, `CallStack.h`

#### dia.core.memory
- **Path:** `Dia/DiaCore/Memory/`
- **File:** `dia.core.memory.architecture.module.md`
- **Purpose:** Memory utilities

#### dia.core.filepath
- **Path:** `Dia/DiaCore/FilePath/`
- **File:** `dia.core.filepath.architecture.module.md`
- **Key Classes:** `FilePath`

#### dia.core.json
- **Path:** `Dia/DiaCore/Json/`
- **File:** `dia.core.json.architecture.module.md`
- **Purpose:** JsonCpp wrapper

---

### dia.maths
- **Path:** `Dia/DiaMaths/`
- **File:** `dia.maths.architecture.module.md`
- **Children:**
  - `dia.maths.vector`
  - `dia.maths.matrix`
  - `dia.maths.transform`
  - `dia.maths.shape`
  - `dia.maths.core`

#### dia.maths.vector
- **Path:** `Dia/DiaMaths/Vector/`
- **File:** `dia.maths.vector.architecture.module.md`
- **Key Classes:** `Vector2D`, `Vector3D`, `Vector4D`

#### dia.maths.matrix
- **Path:** `Dia/DiaMaths/Matrix/`
- **File:** `dia.maths.matrix.architecture.module.md`
- **Key Classes:** `Matrix22`, `Matrix33`, `Matrix44`

#### dia.maths.transform
- **Path:** `Dia/DiaMaths/Transform/`
- **File:** `dia.maths.transform.architecture.module.md`
- **Key Classes:** `Transform2D`, `Transform3D`

#### dia.maths.shape
- **Path:** `Dia/DiaMaths/Shape/`
- **File:** `dia.maths.shape.architecture.module.md`
- **Key Classes:** `Circle`, `AABB`, `Line`, `Polygon`

#### dia.maths.core
- **Path:** `Dia/DiaMaths/Core/`
- **File:** `dia.maths.core.architecture.module.md`
- **Key Classes:** `Random`, `FloatMaths`, `Interpolation`

---

### dia.graphics
- **Path:** `Dia/DiaGraphics/`
- **File:** `dia.graphics.architecture.module.md`
- **Children:**
  - `dia.graphics.interface`

#### dia.graphics.interface
- **Path:** `Dia/DiaGraphics/Interface/`
- **File:** `dia.graphics.interface.architecture.module.md`
- **Key Classes:** `ICanvas`, `Frame`

---

### dia.window
- **Path:** `Dia/DiaWindow/`
- **File:** `dia.window.architecture.module.md`
- **Children:**
  - `dia.window.interface`

#### dia.window.interface
- **Path:** `Dia/DiaWindow/Interface/`
- **File:** `dia.window.interface.architecture.module.md`
- **Key Classes:** `IWindow`

---

### dia.input
- **Path:** `Dia/DiaInput/`
- **File:** `dia.input.architecture.module.md`
- **Key Classes:** `InputEvent`, `InputSourceManager`

---

### dia.ui
- **Path:** `Dia/DiaUI/`
- **File:** `dia.ui.architecture.module.md`
- **Key Classes:** `IUISystem`

---

### dia.sfml
- **Path:** `Dia/DiaSFML/`
- **File:** `dia.sfml.architecture.module.md`
- **Key Classes:** `DiaSFMLRenderWindow`, `DiaSFMLInputSource`, `DiaSFMLSoundManager`

---

### dia.io
- **Path:** `Dia/DiaIO/`
- **File:** `dia.io.architecture.module.md`
- **Purpose:** File I/O

---

### dia.physics
- **Path:** `Dia/DiaPhysics/`
- **File:** `dia.physics.architecture.module.md`
- **Status:** **STUB**

---

### dia.ai
- **Path:** `Dia/DiaAI/`
- **File:** `dia.ai.architecture.module.md`
- **Status:** **STUB**

---

## Module Count by Subsystem

| Subsystem | Module Count | Notes |
|-----------|--------------|-------|
| **Cluiche** | 10+ | Application, levels, phases, modules |
| **DiaCore** | 20+ | Containers, architecture, type system |
| **DiaMaths** | 5 | Vector, matrix, transform, shape, core |
| **DiaApplicationFlow** | 1 | Framework |
| **DiaGraphics** | 2 | Interface, frame |
| **DiaWindow** | 2 | Interface |
| **DiaInput** | 1 | Input events |
| **DiaUI** | 1 | UI interface |
| **DiaSFML** | 1 | SFML backend |
| **DiaIO** | 1 | File I/O |
| **DiaPhysics** | 1 | Physics (stub) |
| **DiaAI** | 1 | AI (stub) |

**Total:** 56 modules

---

## Finding Modules

### By File Pattern

```bash
# Find all module architecture files
find . -name "*.architecture.module.md"
```

### By Tool

```bash
# List all modules
python Tools/dia_modules.py --list-all

# Show module details
python Tools/dia_modules.py --info dia.core.containers.arrays

# Validate all modules
python Tools/dia_modules.py --validate
```

---

## Module Status Summary

### By Status

| Status | Count | Modules |
|--------|-------|---------|
| **active** | ~50 | Most modules |
| **deprecated** | 0 | |
| **stub** | 2 | dia.physics, dia.ai |

### By Maturity

| Maturity | Count | Description |
|----------|-------|-------------|
| **dev** | ~45 | Development (API may change) |
| **stable** | ~5 | Stable API (DiaCore containers) |
| **legacy** | ~5 | Old code (maintenance mode) |

---

## Module Updates

### When to Update Module Files

**Add Module:**
1. Create new directory
2. Create `.architecture.module.md` file
3. Update parent module's `dependent_modules`
4. Update `.vcxproj` and `.vcxproj.filters`
5. Validate with `dia_modules.py --validate`

**Deprecate Module:**
1. Set `status: deprecated` in YAML
2. Document replacement in file body
3. Update dependent modules
4. Plan migration

**Change Dependencies:**
1. Update `dependencies.required` in YAML
2. Update code imports
3. Validate with `dia_modules.py --check-cycles`

---

## Module Relationships

### Most Depended-On Modules

1. **dia.core.containers.arrays** - Used by nearly everything
2. **dia.core.crc** - StringCRC used everywhere
3. **dia.core.type** - Type system widely used
4. **dia.maths.vector** - Used by graphics, physics, gameplay

### Leaf Modules (No Children)

- All container implementations (arrays, hashtables, etc.)
- All math types (vector, matrix, transform, shape)
- Application modules (MainKernelModule, etc.)
- Backend implementations (DiaSFML)

---

## Quick Reference

**Find a module:**
```bash
# By name
ls -R | grep "dia.core.containers.architecture.module.md"

# By subsystem
ls Dia/DiaCore/**/*.architecture.module.md
```

**Validate modules:**
```bash
python Tools/dia_modules.py --validate
```

**Generate dependency graph:**
```bash
python Tools/dia_modules.py --graph output.dot
dot -Tpng output.dot -o graph.png
```

---

**[→ Module Metadata Schema](module-metadata-schema.md)**  
**[→ File Locations](file-locations.md)**  
**[→ Back to Documentation Index](../README.md)**
