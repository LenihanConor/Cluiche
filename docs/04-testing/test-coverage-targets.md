# Test Coverage Targets

**Last Updated:** 2026-04-01

Test coverage goals and current status for Cluiche and Dia engine.

---

## Overview

This document defines test coverage goals by subsystem to ensure adequate validation.

**Coverage Types:**
- **Line Coverage** - % of lines executed by tests
- **Branch Coverage** - % of conditional branches taken
- **Function Coverage** - % of functions called by tests

**Target Baseline:**
- Critical subsystems (P0): 70%+ line coverage
- High-priority subsystems (P1): 60%+ line coverage
- Medium-priority subsystems (P2): 40%+ line coverage

**Related Documents:**
- **[→ Testing Strategy](test.md)** - Overall testing approach
- **[→ Unit Testing](unit-testing.md)** - Component-level testing
- **[→ Integration Testing](integration-testing.md)** - Cross-component testing
- **[→ Requirements Traceability](../03-requirements/traceability-matrix.md)** - Requirements coverage

---

## Current Status

**Overall Coverage:** ❌ Not Yet Measured

**Infrastructure:**
- In-engine tests exist (UnitTestLevel)
- No code coverage tooling configured
- Manual test tracking in source comments

**Next Steps:**
1. Integrate coverage tool (Visual Studio Code Coverage or gcov)
2. Establish baseline coverage measurement
3. Set up CI/CD coverage reporting
4. Track coverage trends over time

---

## Coverage Goals by Subsystem

### DiaCore (Priority: P0)

**Target:** 70%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **Containers/DynamicArray** | 80% | TBD | P0 | 🚧 Tests exist |
| **Containers/HashTable** | 80% | TBD | P0 | 🚧 Tests exist |
| **Containers/LinkList** | 70% | TBD | P1 | 🚧 Tests exist |
| **Containers/Graph** | 60% | TBD | P2 | ❌ No tests |
| **Containers/BitFlag** | 70% | TBD | P1 | ❌ No tests |
| **CRC/StringCRC** | 90% | TBD | P0 | 🚧 Tests exist |
| **Type/TypeRegistry** | 70% | TBD | P0 | 🚧 Tests exist |
| **Type/TypeDefinition** | 70% | TBD | P1 | ❌ No tests |
| **Architecture/Singleton** | 80% | TBD | P0 | 🚧 Tests exist |
| **Architecture/Observer** | 70% | TBD | P1 | ❌ No tests |
| **Architecture/Components** | 60% | TBD | P1 | 🚧 Tests exist |
| **Memory/Allocators** | 50% | TBD | P2 | ❌ No tests |
| **Time/TimeServer** | 70% | TBD | P0 | 🚧 Tests exist |
| **FilePath/Path** | 60% | TBD | P1 | ❌ No tests |
| **FilePath/FileLoad** | 60% | TBD | P1 | ❌ No tests |

**Priority Test Areas:**
1. DynamicArray (add, remove, iterate, resize, edge cases)
2. HashTable (insert, find, remove, collision handling)
3. StringCRC (compile-time evaluation, hash correctness)
4. TypeRegistry (registration, lookup, serialization)
5. Singleton (lifecycle, thread safety)

**Known Gaps:**
- Graph container untested
- BitFlag operations untested
- Observer pattern untested
- Memory allocators untested
- File I/O untested

---

### DiaMaths (Priority: P0)

**Target:** 70%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **Vector/Vector2D** | 80% | TBD | P0 | 🚧 Tests exist |
| **Vector/Vector3D** | 80% | TBD | P0 | 🚧 Tests exist |
| **Vector/Vector4D** | 70% | TBD | P1 | ❌ No tests |
| **Matrix/Matrix22** | 70% | TBD | P1 | ❌ No tests |
| **Matrix/Matrix33** | 80% | TBD | P0 | 🚧 Tests exist |
| **Matrix/Matrix44** | 80% | TBD | P0 | 🚧 Tests exist |
| **Transform/Transform2D** | 60% | TBD | P1 | ⚠️ Tests exist (thread unsafe) |
| **Transform/Transform3D** | 50% | TBD | P2 | ❌ No tests |
| **Shape/Circle** | 70% | TBD | P1 | 🚧 Tests exist |
| **Shape/AABB** | 70% | TBD | P1 | 🚧 Tests exist |
| **Shape/Line** | 60% | TBD | P2 | ❌ No tests |
| **Shape/Polygon** | 60% | TBD | P2 | ❌ No tests |
| **Core/Random** | 80% | TBD | P0 | 🚧 Tests exist |
| **Core/FloatMath** | 70% | TBD | P1 | ❌ No tests |

**Priority Test Areas:**
1. Vector2D/3D (arithmetic, dot/cross product, magnitude, normalize)
2. Matrix33/44 (multiply, inverse, transpose, transformations)
3. Transform2D (parent/child hierarchy, local/world matrices)
4. Circle/AABB (intersection tests, contains tests)
5. Random (thread safety, distribution correctness)

**Known Gaps:**
- Vector4D untested
- Matrix22 untested
- Transform3D untested
- Line and Polygon shapes untested
- Float comparison utilities untested

---

### DiaApplication (Priority: P0)

**Target:** 60%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **ProcessingUnit** | 70% | TBD | P0 | 🚧 Tests exist |
| **Module** | 70% | TBD | P0 | 🚧 Tests exist |
| **Phase** | 70% | TBD | P0 | 🚧 Tests exist |
| **FrameStream** | 90% | TBD | P0 | 🚧 Tests exist |
| **Level/ILevel** | 60% | TBD | P1 | 🚧 Tests exist |
| **Level/LevelFactory** | 70% | TBD | P0 | 🚧 Tests exist |
| **StateObject** | 50% | TBD | P2 | ❌ No tests |

**Priority Test Areas:**
1. FrameStream (producer-consumer, thread safety, FIFO ordering)
2. Module (lifecycle, dependency resolution, update order)
3. Phase (state machine, transitions, module management)
4. ProcessingUnit (multi-threading, phase coordination)
5. LevelFactory (registration, creation, lifecycle)

**Known Gaps:**
- StateObject base class untested
- ProcessingUnit thread coordination edge cases

---

### DiaGraphics (Priority: P1)

**Target:** 50%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **Interface/ICanvas** | 60% | TBD | P1 | 🚧 Mock tests exist |
| **Interface/Frame** | 70% | TBD | P1 | ❌ No tests |
| **Rendering Pipeline** | 40% | TBD | P2 | ❌ No tests |

**Priority Test Areas:**
1. ICanvas interface (draw operations via mocks)
2. Frame data structure (view transforms, background color)

**Known Gaps:**
- Backend-specific implementations (DiaSFML) not unit tested
- Rendering pipeline integration untested

---

### DiaInput (Priority: P1)

**Target:** 60%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **InputEvent** | 80% | TBD | P0 | 🚧 Tests exist |
| **InputSourceManager** | 70% | TBD | P1 | ❌ No tests |
| **IInputSource** | 60% | TBD | P1 | 🚧 Mock tests exist |
| **KeyCode/MouseButton** | 90% | TBD | P0 | ❌ No tests |

**Priority Test Areas:**
1. InputEvent structure (event types, data integrity)
2. InputSourceManager (polling, multi-source)
3. KeyCode/MouseButton enums (completeness)

**Known Gaps:**
- InputSourceManager untested
- Backend-specific implementations (DiaSFML) not unit tested

---

### DiaWindow (Priority: P1)

**Target:** 50%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **Interface/IWindow** | 60% | TBD | P1 | 🚧 Mock tests exist |
| **Window Lifecycle** | 70% | TBD | P0 | ❌ No tests |

**Priority Test Areas:**
1. IWindow interface (create, close, display, clear)
2. Fullscreen toggle
3. VSync control

**Known Gaps:**
- Backend implementations (DiaSFML) not unit tested
- Window lifecycle integration untested

---

### DiaUI (Priority: P2)

**Target:** 30%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **Interface/IUISystem** | 40% | TBD | P2 | ⚠️ Blocked (Awesomium) |
| **UI Lifecycle** | 30% | TBD | P2 | ⚠️ Blocked |

**Status:** ⚠️ Blocked - Awesomium deprecated, no replacement yet

**Priority Test Areas:**
1. IUISystem interface (when replacement chosen)
2. JavaScript interop
3. Callback binding

---

### DiaSFML (Priority: P2)

**Target:** 40%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **DiaSFMLRenderWindow** | 50% | TBD | P1 | ❌ No tests |
| **DiaSFMLInputSource** | 50% | TBD | P1 | ❌ No tests |
| **DiaSFMLSoundManager** | 30% | TBD | P2 | ❌ No tests |

**Priority Test Areas:**
1. SFML → Dia type conversions
2. Event polling
3. Rendering integration

**Known Gaps:**
- Backend implementations require SFML initialization (integration tests)
- No unit tests for backend-specific code

---

### Cluiche Application (Priority: P1)

**Target:** 40%+ line coverage

| Component | Target | Current | Priority | Status |
|-----------|--------|---------|----------|--------|
| **MainProcessingUnit** | 50% | TBD | P0 | 🚧 Tests exist |
| **RenderProcessingUnit** | 50% | TBD | P0 | 🚧 Tests exist |
| **SimProcessingUnit** | 50% | TBD | P0 | 🚧 Tests exist |
| **MainKernelModule** | 60% | TBD | P1 | ❌ No tests |
| **LevelFactoryModule** | 60% | TBD | P1 | ❌ No tests |
| **RenderCanvasModule** | 40% | TBD | P2 | ❌ No tests |
| **SimInputFrameStreamModule** | 70% | TBD | P0 | ❌ No tests |
| **Levels (Dummy, UnitTest)** | 50% | TBD | P2 | 🚧 Tests exist |

**Priority Test Areas:**
1. ProcessingUnit thread coordination
2. Module composition and lifecycle
3. Phase transitions (Boot → Running)
4. FrameStream communication (Main → Sim)
5. Level loading/unloading

**Known Gaps:**
- Individual module tests missing
- Multi-threaded integration tests limited

---

## Coverage Measurement Tools

### Visual Studio Code Coverage

**Setup:**
1. Build in Debug configuration
2. Test → Analyze Code Coverage → All Tests
3. Review results in Code Coverage Results window

**Advantages:**
- Integrated with Visual Studio
- No additional setup required
- Visual highlighting in editor

**Limitations:**
- Windows only
- Requires Visual Studio Enterprise

---

### OpenCppCoverage (Windows)

**Setup:**
```bash
# Install OpenCppCoverage
choco install opencppcoverage

# Run with coverage
OpenCppCoverage.exe --sources Dia\* -- Cluiche.exe

# Generate HTML report
OpenCppCoverage.exe --sources Dia\* --export_type html:coverage_report -- Cluiche.exe
```

**Advantages:**
- Free and open-source
- HTML reports
- Works with any Visual Studio edition

---

### gcov/lcov (Linux)

**Setup:**
```bash
# Compile with coverage flags
g++ -fprofile-arcs -ftest-coverage -o Cluiche Main.cpp

# Run tests
./Cluiche

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
```

**Advantages:**
- Industry standard
- Detailed reports
- CI/CD integration

---

### Google Test Integration

**Future: Integrate Google Test with coverage**

```cmake
# CMakeLists.txt
enable_testing()
add_executable(DiaTests tests/DiaCore/TestDynamicArray.cpp)
target_link_libraries(DiaTests gtest gtest_main DiaCore)

# Run with coverage
add_custom_target(coverage
    COMMAND lcov --capture --directory . --output-file coverage.info
    COMMAND genhtml coverage.info --output-directory coverage_report
    DEPENDS DiaTests
)
```

---

## Gap Analysis

### Critical Gaps (P0)

**Missing Tests:**
1. DiaCore/Graph container (no tests)
2. DiaApplication/StateObject (no tests)
3. DiaInput/InputSourceManager (no tests)
4. Cluiche modules (minimal tests)

**Inadequate Coverage:**
1. FrameStream thread safety (needs stress tests)
2. Phase transitions under load (needs concurrency tests)
3. Module dependency resolution (edge cases)

**Blocked:**
1. DiaUI/Awesomium (deprecated, awaiting replacement)
2. DiaMaths/Transform2D (thread safety issue)

---

### High-Priority Gaps (P1)

**Missing Tests:**
1. DiaCore/Observer pattern
2. DiaMaths/Vector4D, Matrix22
3. DiaMaths/Line, Polygon shapes
4. DiaGraphics/Frame
5. DiaSFML backend implementations

**Inadequate Coverage:**
1. DiaCore/TypeDefinition serialization
2. DiaMaths/Transform hierarchy
3. DiaApplication/ProcessingUnit error handling

---

### Medium-Priority Gaps (P2)

**Missing Tests:**
1. DiaCore/Memory allocators
2. DiaMaths/Transform3D
3. DiaGraphics rendering pipeline
4. DiaSFML/SoundManager
5. Cluiche/RenderCanvasModule

---

## Testing Roadmap

### Phase 1: Foundation (Weeks 1-2)

**Goal:** Set up coverage infrastructure

1. Install coverage tool (OpenCppCoverage or Visual Studio)
2. Establish baseline coverage measurement
3. Generate initial coverage report
4. Document current state

**Deliverables:**
- Coverage tool configured
- Baseline report generated
- Coverage tracking process documented

---

### Phase 2: Critical Subsystems (Weeks 3-6)

**Goal:** 70%+ coverage on P0 subsystems

1. DiaCore containers (DynamicArray, HashTable, LinkList)
2. DiaCore type system (StringCRC, TypeRegistry)
3. DiaApplication framework (Module, Phase, ProcessingUnit)
4. FrameStream thread safety
5. DiaMaths vectors and matrices

**Deliverables:**
- 70%+ line coverage on DiaCore containers
- 70%+ line coverage on DiaApplication
- 70%+ line coverage on DiaMaths vectors/matrices

---

### Phase 3: High-Priority Subsystems (Weeks 7-10)

**Goal:** 60%+ coverage on P1 subsystems

1. DiaCore architecture patterns (Singleton, Observer, Components)
2. DiaMaths shapes (Circle, AABB, Line, Polygon)
3. DiaInput (InputEvent, InputSourceManager)
4. DiaWindow (IWindow interface)
5. DiaGraphics (ICanvas interface, Frame)

**Deliverables:**
- 60%+ line coverage on DiaCore architecture
- 60%+ line coverage on DiaMaths shapes
- 60%+ line coverage on DiaInput

---

### Phase 4: Medium-Priority Subsystems (Weeks 11-14)

**Goal:** 40%+ coverage on P2 subsystems

1. DiaCore memory and file I/O
2. DiaMaths Transform3D
3. DiaSFML backend implementations
4. Cluiche modules and levels
5. Integration test expansion

**Deliverables:**
- 40%+ line coverage on remaining subsystems
- Integration test suite expanded
- Performance benchmarks established

---

### Phase 5: Maintenance (Ongoing)

**Goal:** Maintain and improve coverage

1. Monitor coverage trends
2. Add tests for new features
3. Improve coverage in gap areas
4. Regression test suite
5. CI/CD coverage gates

**Deliverables:**
- Coverage tracked in CI/CD
- Coverage gates enforce minimums
- Monthly coverage reports

---

## Coverage Enforcement

### Pre-Commit Checks

```bash
# Git pre-commit hook
#!/bin/bash

# Run tests
./run_tests.sh

# Check coverage
coverage=$(opencppcoverage --sources Dia\* -- Cluiche.exe | grep "Line coverage:" | awk '{print $3}')

if [ "$coverage" -lt 60 ]; then
    echo "Coverage below 60%: $coverage%"
    exit 1
fi
```

---

### Pull Request Gates

**Requirements:**
- All tests pass
- Line coverage ≥ 60% overall
- New code has ≥ 70% coverage
- No coverage regressions > 5%

---

### Coverage Targets by Date

| Date | Target | Milestone |
|------|--------|-----------|
| 2026-04-30 | 40% overall | Baseline established |
| 2026-05-31 | 50% overall | Critical subsystems covered |
| 2026-06-30 | 60% overall | High-priority subsystems covered |
| 2026-07-31 | 65% overall | Medium-priority subsystems covered |
| 2026-08-31 | 70% overall | Target coverage achieved |

---

## Summary

**Overall Target:** 60%+ line coverage

**Current Status:** ❌ Not Yet Measured

**Priority Areas:**
1. DiaCore containers (DynamicArray, HashTable)
2. DiaApplication framework (Module, Phase, ProcessingUnit, FrameStream)
3. DiaMaths (Vector2D/3D, Matrix33/44)
4. Cluiche ProcessingUnits (Main, Render, Sim)

**Critical Gaps:**
- No coverage measurement infrastructure
- Graph, BitFlag containers untested
- Observer pattern untested
- DiaSFML backend untested
- Cluiche modules minimally tested

**Next Steps:**
1. Set up coverage tool (OpenCppCoverage)
2. Establish baseline coverage
3. Execute testing roadmap (Phases 1-5)
4. Enforce coverage gates in CI/CD

**[→ Testing Strategy](test.md)**  
**[→ Unit Testing](unit-testing.md)**  
**[→ Requirements Traceability](../03-requirements/traceability-matrix.md)**
