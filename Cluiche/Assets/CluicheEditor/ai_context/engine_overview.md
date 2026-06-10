# Dia Engine Overview

## Platform and Application Structure

**Cluiche** is a game development platform built on the **Dia engine**. 

- **Platform:** Cluiche — the overall platform
- **Engine:** Dia — shared engine code used by all applications
- **Applications:** CluicheTest (demo/testbed), CluicheEditor (game editor), future games
- Applications are siblings under `Cluiche/` directory

## Engine Architecture Layers

Dia is organized into layers (lowest to highest):

1. **DiaCore** — Foundation: containers, memory, architecture patterns, reflection
   - Namespaces: `Dia::Core::`
   - Key exports: `Singleton<T>`, `DynamicArrayC<T>`, `HashTable<K,V>`, `StringCRC`, `ComponentFactoryRegistry`, `ProcessingUnit`

2. **DiaMaths** — Vector, matrix, quaternion, physics-related math
   - Namespaces: `Dia::Maths::`

3. **DiaGraphics** — Rendering systems, camera, geometry, animation
   - Namespaces: `Dia::Graphics::`

4. **DiaPhysics** — Physics simulation (rigid bodies, collision)

5. **DiaSFML** — Window management, input, graphics backend (SFML wrapper)

6. **DiaAPI** — High-level engine APIs: animation, rendering, physics queries

7. **DiaObservation** — Telemetry: logging, tracing, profiling, metrics, health checks

8. **DiaEditor** — Editor-specific plugins and framework

## Module System

Each layer consists of **modules**. A module is a logical unit of functionality with:

- **Public API** — exported headers and namespaces
- **Dependencies** — list of modules it depends on (enforced at build time)
- **Unique ID** — `StringCRC`-based constant `kUniqueId` (compile-time hashed string)
- **Documentation** — `dia.<parent>.<module>.architecture.module.md` file with YAML frontmatter

**Module ID rules:**
- IDs are `static const StringCRC` (MSVC constraint)
- Namespace: `Dia::<Layer>::<Module>::kUniqueId`
- Used internally for type identification and component registration

## ProcessingUnit / Module / Phase Hierarchy

Applications follow a three-level execution model:

```
ProcessingUnit (e.g., MainProcessingUnit)
  ├─ Module (e.g., RenderModule, PhysicsModule)
  │   └─ Phase (e.g., InitPhase, UpdatePhase, RenderPhase, ShutdownPhase)
  │        └─ Phase transitions define application flow
```

- **ProcessingUnit** — may run on its own thread; owns modules and orchestrates phase transitions
- **Module** — provides functionality; implements phases
- **Phase** — discrete work unit that executes during a ProcessingUnit update cycle

Phases transition based on state machines. `TransitionPhase()` is immediate; `QueuePhaseTransition()` is thread-safe.

## Component-Based Architecture

Entities contain **components**. Components are typed, composable behaviors:

- **IComponent** interface — base for all components
- **Component Factory** — `ComponentFactoryRegistry` holds factories by `StringCRC` type ID
- **Access patterns:**
  - `DIA_READONLY` — data-driven: module owns data, provides read-only access to entity
  - **Behaviour** — entity signals module via mailbox (immutable messages)
- No service locators or statics for component access (except explicitly approved ModuleRef patterns)

## Naming Conventions

- **Namespaces:** `Dia::Core::`, `Dia::Maths::`, `Dia::Graphics::`, `Dia::Application::`
- **Files:** PascalCase matching class names (e.g., `ProcessingUnit.h`, `DynamicArray.h`)
- **Classes:** PascalCase (e.g., `ProcessingUnit`, `ComponentFactory`)
- **Member variables:** camelCase with `m` prefix (e.g., `mCurrentPhase`, `mModuleTable`)
- **Constants:** `k` prefix (e.g., `kUniqueId`, `kMaxSize`)
- **Interfaces:** `I` prefix (e.g., `IComponent`, `IComponentFactory`)

## Build System

- **MSBuild** — Visual Studio C++ project (`Cluiche/Cluiche.sln`)
- **Configurations:** `Debug|x64`, `Release|x64`
- **Always use `dia run <target>`** — never call executables directly; the CLI handles paths and dependencies
- **Examples:**
  - `dia run googletest` — build + run unit tests
  - `dia run cluichetest` — build + run game
  - `dia run cluicheeditor` — build + run editor
  - `dia pipeline --target cluicheeditor` — full pipeline: compile + build assets + deploy UI

## String IDs and CRC

- **StringCRC** — compile-time CRC hashing for efficient string comparison
- Used for: module IDs, component type IDs, event names, asset references
- Example: `Dia::Core::ProcessingUnit::kUniqueId` is a `StringCRC` constant
- Performance: O(1) string comparison via integer CRC value

## External Dependencies

- **SFML** — graphics, windowing, multimedia library
- **jsoncpp** — JSON parsing (used for manifests, asset metadata)
- **Webix, VisJS** — web UI frameworks for editor plugins and debugging
