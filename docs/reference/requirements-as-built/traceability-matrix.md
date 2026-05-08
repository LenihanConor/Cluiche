# Requirements Traceability Matrix

**Last Updated:** 2026-04-01

Mapping of requirements to implementation files for traceability.

---

## Overview

This matrix maps each requirement ID to its implementation location(s) in the codebase.

**Purpose:**
- Verify all requirements implemented
- Track implementation coverage
- Impact analysis (what code implements requirement X?)
- Audit and compliance

**Related Documents:**
- **[→ Main Requirements](requirements.md)** - Complete requirements list
- **[→ Functional Requirements](functional-requirements.md)** - Functional details
- **[→ Cluiche Requirements](cluiche-requirements.md)** - Application requirements
- **[→ Dia Requirements](dia-requirements.md)** - Engine requirements

---

## Functional Requirements - Cluiche (CF-*)

| Req ID | Status | Priority | Implementation Files |
|--------|--------|----------|---------------------|
| **CF-001** | ✅ | P0 | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.h`<br>`Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.h`<br>`Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.h` |
| **CF-002** | ✅ | P0 | `Cluiche/CluicheKernel/LevelFactory.h`<br>`Cluiche/Stages/DummyStage/DummyStage.h`<br>`Cluiche/Levels/UnitTestLevel/UnitTestLevel.h`<br>`Dia/DiaApplication/Level/ILevel.h` |
| **CF-003** | ✅ | P1 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h`<br>`Cluiche/CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h`<br>`Dia/DiaApplication/ApplicationModule.h` |
| **CF-004** | ✅ | P1 | `Cluiche/ApplicationFlow/Phases/MainBootPhase.h`<br>`Cluiche/ApplicationFlow/Phases/MainBootStrapPhase.h`<br>`Cluiche/ApplicationFlow/Phases/RenderRunningPhase.h`<br>`Dia/DiaApplication/ApplicationPhase.h` |
| **CF-005** | 🚧 | P2 | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.cpp:GenerateModuleDependecyGraph()` (partial) |

---

## Functional Requirements - Dia Engine (DE-*)

| Req ID | Status | Priority | Implementation Files |
|--------|--------|----------|---------------------|
| **DE-001** | ✅ | P0 | `Dia/DiaApplication/ApplicationProcessingUnit.h`<br>`Dia/DiaApplication/ApplicationPhase.h`<br>`Dia/DiaApplication/ApplicationModule.h`<br>No platform headers in Dia core modules |
| **DE-002** | ✅ | P0 | `Dia/DiaCore/Containers/Arrays/DiaCoreArray.h`<br>`Dia/DiaCore/Containers/Arrays/DiaCoreDynamicArray.h`<br>`Dia/DiaCore/Containers/HashTables/DiaCoreHashTable.h`<br>`Dia/DiaCore/Containers/Graphs/DiaCoreGraph.h`<br>`Dia/DiaCore/Containers/LinkLists/DiaCoreLinkList.h`<br>`Dia/DiaCore/Containers/BitFlags/DiaCoreBitFlag.h` |
| **DE-003** | ✅ | P0 | `Dia/DiaCore/CRC/DiaCoreCRC.h` (StringCRC)<br>`Dia/DiaCore/Type/DiaCoreTypeRegistry.h`<br>`Dia/DiaCore/Type/DiaCoreTypeDefinition.h` |
| **DE-004** | ✅ | P0 | `Dia/DiaGraphics/Interface/ICanvas.h`<br>`Dia/DiaSFML/DiaSFMLRenderWindow.h` (SFML backend) |
| **DE-005** | ✅ | P1 | `Dia/DiaMaths/Vector/DiaMathsVector2D.h`<br>`Dia/DiaMaths/Vector/DiaMathsVector3D.h`<br>`Dia/DiaMaths/Matrix/DiaMathsMatrix33.h`<br>`Dia/DiaMaths/Matrix/DiaMathsMatrix44.h`<br>`Dia/DiaMaths/Transform/DiaMathsTransform2D.h`<br>`Dia/DiaMaths/Shape/Circle.h`<br>`Dia/DiaMaths/Shape/AABB.h` |
| **DE-006** | ⚠️ | P1 | `Dia/DiaMaths/Core/DiaMathsRandom.h` (fixed, thread-safe)<br>`Dia/DiaMaths/Transform/DiaMathsTransform2D.h` (⚠️ not thread-safe) |
| **DE-007** | ✅ | P1 | `Dia/DiaInput/DiaInputEvent.h`<br>`Dia/DiaInput/DiaInputInputSourceManager.h`<br>`Dia/DiaInput/IInputSource.h`<br>`Dia/DiaSFML/DiaSFMLInputSource.h` (SFML backend) |
| **DE-008** | ✅ | P1 | `Dia/DiaUI/Interface/IUISystem.h` |
| **DE-009** | ✅ | P1 | `Dia/DiaWindow/Interface/IWindow.h`<br>`Dia/DiaSFML/DiaSFMLRenderWindow.h` (SFML backend) |
| **DE-010** | ❌ | P2 | Not implemented (stub in `/docs/05-api/dia/physics-api.md`) |
| **DE-011** | ❌ | P2 | Not implemented (stub in `/docs/05-api/dia/ai-api.md`) |

---

## Non-Functional Requirements (NF-*)

| Req ID | Status | Priority | Implementation Files / Verification |
|--------|--------|----------|-------------------------------------|
| **NF-001** | ✅ | P0 | `Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.cpp`<br>`Dia/DiaSFML/DiaSFMLRenderWindow.cpp:SetVSync(true)`<br>Verified: 60 FPS in Debug and Release |
| **NF-002** | 🚧 | P1 | `Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.cpp`<br>Needs profiling and optimization |
| **NF-003** | ✅ | P1 | All modules use RAII for resource management<br>Verified: No memory leaks detected |
| **NF-004** | ✅ | P2 | Lazy initialization throughout<br>Verified: < 2s Debug, < 1s Release |
| **NF-005** | ✅ | P1 | `Dia/DiaCore/Containers/`<br>Verified: Performance comparable to STL |
| **NF-006** | ❌ | P1 | No code coverage measurement yet<br>Tests exist in `Tests/UnitTests/` |
| **NF-007** | ✅ | P0 | `Dia/DiaCore/Core/Assert.h` (DIA_ASSERT)<br>Assertions throughout codebase |
| **NF-008** | 🚧 | P0 | `Dia/DiaApplication/FrameStream.h` (thread-safe)<br>`Dia/DiaMaths/Core/DiaMathsRandom.h` (thread-safe, fixed)<br>Transform2D hierarchy not thread-safe (known issue) |
| **NF-009** | ✅ | P1 | DIA_ASSERT usage throughout codebase<br>Verified: Assertions fire for invalid input |
| **NF-010** | ✅ | P1 | `/docs/05-api/` - Comprehensive API documentation |
| **NF-011** | ✅ | P0 | `/docs/reference/architecture/` - Architecture documentation<br>`/docs/reference/design-rationale/` - Design rationale |
| **NF-012** | ✅ | P1 | `/docs/reference/development/coding-standards.md`<br>Code follows documented standards |
| **NF-013** | ✅ | P0 | `Cluiche/Cluiche.sln` - Visual Studio solution<br>External dependencies included in repository |
| **NF-014** | ✅ | P1 | Windows support verified<br>No platform headers in Dia core<br>Backend abstraction in place |
| **NF-015** | ✅ | P1 | `/docs/reference/architecture/external-dependencies.md`<br>SFML, JsonCpp documented |

---

## Cluiche Application Requirements (CR-*)

| Req ID | Status | Priority | Implementation Files |
|--------|--------|----------|---------------------|
| **CR-001** | ✅ | P0 | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.cpp`<br>`Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.cpp`<br>`Cluiche/ApplicationFlow/ProcessingUnits/SimProcessingUnit.cpp` |
| **CR-002** | ✅ | P0 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/` (all modules)<br>Module composition in ProcessingUnit constructors |
| **CR-003** | ✅ | P1 | `Cluiche/ApplicationFlow/Phases/` (all phases)<br>Phase transitions in ProcessingUnits |
| **CR-004** | ✅ | P0 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.h`<br>`Dia/DiaApplication/FrameStream.h` |
| **CR-005** | 🚧 | P1 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/SimUIProxyModule.h`<br>Basic implementation, needs expansion |
| **CR-006** | ✅ | P0 | `Cluiche/CluicheKernel/LevelFactory.h`<br>`Dia/DiaApplication/Level/ILevel.h` |
| **CR-007** | ✅ | P0 | All levels implement ILevel::Load() and Unload()<br>`Cluiche/Stages/DummyStage/DummyStage.cpp`<br>`Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp` |
| **CR-008** | ✅ | P1 | `Cluiche/Stages/DummyStage/` - Minimal level<br>`Cluiche/Levels/UnitTestLevel/` - Test harness |
| **CR-009** | ✅ | P0 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainInputModule.cpp`<br>`Cluiche/CluicheKernel/ApplicationFlow/Modules/SimInputFrameStreamModule.cpp` |
| **CR-010** | 🚧 | P2 | Pattern documented in `/docs/05-api/dia/input-api.md`<br>Not yet implemented in codebase |
| **CR-011** | ✅ | P0 | `Cluiche/ApplicationFlow/ProcessingUnits/RenderProcessingUnit.cpp`<br>`Cluiche/CluicheKernel/ApplicationFlow/Modules/RenderCanvasModule.cpp` |
| **CR-012** | 🚧 | P2 | FPS counter exists, needs expansion for bounding boxes, etc. |
| **CR-013** | ⚠️ | P1 | `Cluiche/CluicheKernel/ApplicationFlow/Modules/MainUIModule.h` |
| **CR-014** | ✅ | P1 | `Cluiche/Levels/UnitTestLevel/UnitTestLevel.cpp` |
| **CR-015** | ❌ | P2 | `Cluiche/ApplicationFlow/ProcessingUnits/MainProcessingUnit.cpp:GenerateModuleDependecyGraph()` (partial) |
| **CR-016** | ✅ | P0 | Verified: 60 FPS maintained in typical scenarios |

---

## Dia Engine Requirements (DR-*)

| Req ID | Status | Priority | Implementation Files |
|--------|--------|----------|---------------------|
| **DR-001** | ✅ | P0 | `Dia/DiaCore/Containers/Arrays/DynamicArray.h`<br>`Dia/DiaCore/Containers/HashTables/HashTable.h`<br>`Dia/DiaCore/Containers/LinkLists/LinkList.h`<br>`Dia/DiaCore/Containers/Graphs/Graph.h`<br>`Dia/DiaCore/Containers/BitFlags/BitFlag.h`<br>`Dia/DiaCore/Containers/Arrays/Array.h` |
| **DR-002** | ✅ | P0 | `Dia/DiaCore/CRC/StringCRC.h`<br>Compile-time CRC32 implementation |
| **DR-003** | ✅ | P0 | `Dia/DiaCore/Type/TypeRegistry.h`<br>`Dia/DiaCore/Type/TypeDefinition.h` |
| **DR-004** | ✅ | P1 | `Dia/DiaCore/Architecture/Components/IComponent.h`<br>`Dia/DiaCore/Architecture/Components/IComponentObject.h`<br>`Dia/DiaCore/Architecture/Components/IComponentFactory.h`<br>`Dia/DiaCore/Architecture/Components/ComponentFactoryRegistry.h`<br>`Dia/DiaCore/Architecture/Components/StaticPooledComponentFactory.h` |
| **DR-005** | ✅ | P1 | `Dia/DiaCore/Architecture/Singleton/Singleton.h` |
| **DR-006** | ✅ | P1 | `Dia/DiaCore/Architecture/Observer.h` |
| **DR-007** | ✅ | P0 | `Dia/DiaApplication/ApplicationProcessingUnit.h` |
| **DR-008** | ✅ | P0 | `Dia/DiaApplication/ApplicationModule.h` |
| **DR-009** | ✅ | P0 | `Dia/DiaApplication/ApplicationPhase.h` |
| **DR-010** | ✅ | P0 | `Dia/DiaApplication/FrameStream.h` |
| **DR-011** | ✅ | P0 | `Dia/DiaApplication/Level/ILevel.h`<br>`Dia/DiaApplication/Level/LevelFactory.h` |
| **DR-012** | ✅ | P0 | `Dia/DiaMaths/Vector/Vector2D.h`<br>`Dia/DiaMaths/Vector/Vector3D.h`<br>`Dia/DiaMaths/Vector/Vector4D.h` |
| **DR-013** | ✅ | P0 | `Dia/DiaMaths/Matrix/Matrix22.h`<br>`Dia/DiaMaths/Matrix/Matrix33.h`<br>`Dia/DiaMaths/Matrix/Matrix44.h` |
| **DR-014** | ⚠️ | P1 | `Dia/DiaMaths/Transform/Transform2D.h`<br>`Dia/DiaMaths/Transform/Transform3D.h`<br>⚠️ Performance issue: GetWorldMatrix() slow (no caching) |
| **DR-015** | ✅ | P1 | `Dia/DiaMaths/Shape/Circle.h`<br>`Dia/DiaMaths/Shape/AABB.h`<br>`Dia/DiaMaths/Shape/Line.h`<br>`Dia/DiaMaths/Shape/Polygon.h` |
| **DR-016** | ✅ | P1 | `Dia/DiaMaths/Core/Random.h`<br>Thread-safe (uses std::mutex, fixed 2026-03) |
| **DR-017** | ✅ | P0 | `Dia/DiaGraphics/Interface/ICanvas.h` |
| **DR-018** | ✅ | P1 | `Dia/DiaGraphics/Interface/Frame.h` |
| **DR-019** | ✅ | P0 | `Dia/DiaInput/DiaInputEvent.h`<br>`Dia/DiaInput/DiaInputKeyCode.h`<br>`Dia/DiaInput/DiaInputMouseButton.h` |
| **DR-020** | ✅ | P1 | `Dia/DiaInput/DiaInputInputSourceManager.h`<br>`Dia/DiaInput/IInputSource.h` |
| **DR-021** | ⚠️ | P1 | `Dia/DiaUI/Interface/IUISystem.h` |
| **DR-022** | ✅ | P0 | `Dia/DiaWindow/Interface/IWindow.h` |
| **DR-023** | ✅ | P0 | `Dia/DiaSFML/DiaSFMLRenderWindow.h`<br>`Dia/DiaSFML/DiaSFMLInputSource.h`<br>`Dia/DiaSFML/DiaSFMLSoundManager.h` |
| **DR-024** | ✅ | P1 | `Dia/DiaCore/FilePath/Path.h`<br>`Dia/DiaCore/FilePath/FilePath.h` |
| **DR-025** | ✅ | P1 | `Dia/DiaCore/FilePath/IFileLoad.h`<br>`Dia/DiaCore/FilePath/FileLoad.h` |
| **DR-026** | ❌ | P2 | Not implemented (stub in docs) |
| **DR-027** | ❌ | P2 | Not implemented (stub in docs) |

---

## Coverage Summary

### By Category

| Category | Total | Complete | In Progress | Blocked | Not Started |
|----------|-------|----------|-------------|---------|-------------|
| **CF (Cluiche Functional)** | 5 | 4 (80%) | 1 (20%) | 0 | 0 |
| **DE (Dia Engine)** | 11 | 9 (82%) | 0 | 1 (9%) | 2 (18%) |
| **NF (Non-Functional)** | 15 | 12 (80%) | 2 (13%) | 0 | 1 (7%) |
| **CR (Cluiche Requirements)** | 16 | 11 (69%) | 3 (19%) | 1 (6%) | 1 (6%) |
| **DR (Dia Requirements)** | 27 | 22 (81%) | 0 | 2 (7%) | 2 (7%) |
| **TOTAL** | 74 | 58 (78%) | 6 (8%) | 4 (5%) | 6 (8%) |

### By Priority

| Priority | Total | Complete | In Progress | Blocked | Not Started |
|----------|-------|----------|-------------|---------|-------------|
| **P0 (Critical)** | 34 | 31 (91%) | 1 (3%) | 2 (6%) | 0 |
| **P1 (High)** | 33 | 24 (73%) | 4 (12%) | 2 (6%) | 3 (9%) |
| **P2 (Medium)** | 7 | 3 (43%) | 1 (14%) | 0 | 3 (43%) |

---

## Gaps and Risks

### Critical Gaps (P0)
- **NF-008**: Thread safety (Transform2D not thread-safe, ⚠️ blocked)
- **NF-002**: Sim thread performance (needs profiling, 🚧 in progress)

### High Priority Gaps (P1)
- **DE-006**: Math thread safety (Transform2D issue, ⚠️ blocked)
- **DR-014**: Transform hierarchies (performance issue, ⚠️ blocked)
- **NF-006**: Test coverage (no measurement, ❌ not started)
- **DE-008, DR-021, CR-013**: UI system (⚠️ needs replacement backend)

### Medium Priority Gaps (P2)
- **DE-010, DR-026**: Physics API (not started)
- **DE-011, DR-027**: AI/Pathfinding API (not started)
- **CF-005, CR-015**: Module dependency visualization (partial)

---

## Next Steps

**Immediate Actions:**
1. Fix Transform2D thread safety (DR-014, DE-006, NF-008)
2. Profile Sim thread performance (NF-002)
3. Replace UI backend (DE-008, DR-021, CR-013)
4. Set up code coverage measurement (NF-006)

**Planned Actions:**
1. Complete module dependency visualization (CF-005, CR-015)
2. Expand debug rendering (CR-012)
3. Implement input state tracking (CR-010)

**Future Actions:**
1. Plan physics integration (DE-010, DR-026)
2. Plan AI integration (DE-011, DR-027)

---

## Verification Methods

| Requirement Type | Verification Method |
|------------------|-------------------|
| **Functional** | Manual testing, automated tests, code review |
| **Performance** | Profiling, benchmarks, FPS measurement |
| **Reliability** | Stress testing, error injection, code coverage |
| **Maintainability** | Documentation review, code review |
| **Portability** | Multi-platform builds, dependency audit |

---

**[→ Main Requirements](requirements.md)**  
**[→ Functional Requirements](functional-requirements.md)**  
**[→ Test Coverage Targets](../testing/test-coverage-targets.md)**
