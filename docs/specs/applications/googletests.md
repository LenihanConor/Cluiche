# Application Spec: GoogleTests

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose

GoogleTests (also known as Cluiche Test Suite) is the unit testing application for validating all Dia engine modules and game code. Built on the Google Test framework, it provides comprehensive test coverage with an intelligent dirty test tracking system that optimizes test execution by running only tests affected by recent code changes. This application serves developers writing and maintaining the engine, ensuring code quality and preventing regressions.

## Systems

| System | Description | Spec |
|--------|-------------|------|
| Test Runner | Main entry point with Google Test framework integration and dirty test tracking | TBD |
| Core Tests | Unit tests for DiaCore (containers, types, time, CRC, threading, events, architecture) | TBD |
| Maths Tests | Unit tests for DiaMaths (vectors, matrices, angles, trigonometry, intersection) | TBD |
| Graphics Tests | Unit tests for DiaGraphics (RGBA, Vertex, Transform, render states) | TBD |
| Input Tests | Unit tests for DiaInput (input state, action maps, event handling, profiles) | TBD |
| Dirty Tracking | Pre-build system that tracks file changes and generates test filters | TBD |

## Application-Specific Architecture

### Test Organization

Tests are organized to mirror the Dia engine module structure:

```
GoogleTests/
├── Main.cpp                     # Entry point with dirty test tracking
├── Core/                        # DiaCore module tests
│   ├── Containers/              # Container tests (Array, DynamicArray, HashTable, etc.)
│   ├── Type/                    # Type system tests (Enum, TypeTraits, MetaLogic)
│   ├── Time/                    # Time tests (TimeAbsolute, Timer, TimeServer)
│   ├── Threading/               # Threading tests (Mutex, ThreadPool, JobSystem, Atomic)
│   ├── Events/                  # Event tests (Delegate, EventQueue, EventDispatcher)
│   ├── CRC/                     # CRC hashing tests
│   ├── Architecture/            # Architecture pattern tests (Singleton, Component)
│   └── Memory/                  # Memory tests (SmartPointers)
├── Maths/                       # DiaMaths module tests
│   ├── Vector tests (Vector2D, Vector3D, Vector4D, HalfVector2D)
│   ├── Matrix tests (Matrix22, Matrix33)
│   ├── Angle and Trigonometry tests
│   ├── Shape tests (Circle2D)
│   └── Intersection tests
├── Graphics/                    # DiaGraphics module tests
│   ├── RGBA, Vertex, Transform
│   ├── PrimitiveType, RenderStates
│   └── Graphics primitive tests
└── Input/                       # DiaInput module tests
    ├── Input state and profiles
    ├── Action maps and contexts
    ├── Event handling (modern + legacy)
    └── Input recording
```

### Dirty Test Tracking

**Pre-Build Hook:** Python script (`Tools/dirty_test_tracker.py`) runs before compilation:
1. Detects which source files changed since last baseline (git commit)
2. Maps changed files to affected test files
3. Generates `.dirty/dirty_tests.json` with Google Test filter string

**Main.cpp Integration:**
- Loads dirty test filter from JSON
- Applies filter to Google Test framework
- Falls back to running all tests if no dirty tests found
- Can be overridden with `--gtest_filter` command-line flag

**Benefits:**
- Faster test cycles during development (only run affected tests)
- Full test suite still runs in CI/CD
- Transparent to developers (just build and run)

### Test Execution Model

**Entry Point:** `Cluiche/Tests/GoogleTests/Main.cpp`

```cpp
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // Load dirty test filter if no explicit filter provided
    if (no explicit --gtest_filter) {
        LoadDirtyTestFilter();  // Applies filter from .dirty/dirty_tests.json
    }
    
    return RUN_ALL_TESTS();  // 0 = pass, non-zero = fail
}
```

**Output:** Google Test XML reports, console output, exit codes for CI/CD

## Platform Dependencies

Which shared platform modules (Dia engine) does this app use?

- [x] DiaCore - All container, type, time, CRC, threading, event, architecture tests
- [x] DiaMaths - All math, vector, matrix, shape tests
- [x] DiaGraphics - Graphics primitive and render state tests
- [x] DiaInput - Input handling and event system tests
- [ ] DiaApplicationFlow - Not directly tested (tested via integration in CluicheTest app)
- [ ] DiaWindow - Not directly tested (integration tests only)
- [ ] DiaUI - Not directly tested (integration tests only)
- [ ] DiaPhysics - Not yet implemented
- [ ] DiaAI - Not yet implemented

**External Dependencies:**
- Google Test framework (`External/googletest/`)
- JsonCpp for dirty test filter parsing (`External/jsoncpp-master/`)

## Out of Scope

What this application deliberately does NOT do:

- **Not integration tests** - GoogleTests focuses on unit tests; integration testing happens in CluicheTest application
- **Not performance benchmarks** - No performance regression testing (future work)
- **Not UI/visual testing** - No screenshot comparison or visual validation
- **Not end-to-end tests** - No gameplay or full application flow tests
- **Not network/multiplayer tests** - No distributed system testing
- **Not manual testing** - Fully automated via Google Test framework

## Key Users / Personas

1. **Engine Developers** - Writing new Dia engine features; need fast feedback on tests
2. **Contributors** - Submitting PRs; need to verify no regressions before merge
3. **CI/CD Systems** - Automated builds; need reliable test execution and reporting
4. **QA Engineers** - Validating engine correctness; need comprehensive test coverage
5. **Maintainers** - Reviewing PRs and managing releases; need test reports and coverage data

## Decisions

<!-- Decisions specific to this application. Binding decisions cascade to all systems within GoogleTests.
     AI: Always check parent platform decisions (PLATFORM.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | Use Google Test framework exclusively | Industry standard, excellent tooling, mature ecosystem, CI/CD integration | All test systems | Accepted | Yes |
| AD-002 | Mirror Dia module structure in test organization | Makes it easy to find tests for any module; clear ownership; scales to many modules | All test systems | Accepted | Yes |
| AD-003 | Dirty test tracking is opt-in via pre-build hook | Doesn't break manual workflows; developers can opt out with --gtest_filter=*; CI/CD runs full suite | Test Runner | Accepted | No |
| AD-004 | Tests link against Dia .lib files, not source | Validates public API surface; faster incremental builds; matches real usage | All test systems | Accepted | Yes |
| AD-005 | Exit code 0 = all tests pass, non-zero = any failure | Standard convention for CI/CD integration; works with all build systems | Test Runner | Accepted | Yes |
| AD-006 | No test fixtures requiring external resources | Tests are self-contained; no database, network, or file system dependencies (except test data) | All test systems | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems · `No` = guidance only

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Systems | Should Dirty Tracking be its own system or part of Test Runner? | Separate system - it's a pre-build tool (Python script) independent of Test Runner (C++ executable). Different lifecycle and technology. |
| 2 | Dependencies | Are DiaWindow/DiaUI/DiaApplicationFlow tested anywhere? | No unit tests currently. Integration tests in CluicheTest application cover these. Consider adding unit tests for testable components. |
| 3 | Out of Scope | Should performance testing be added? | TBD - would require separate performance test suite with benchmarking framework (e.g., Google Benchmark). Out of scope for current unit tests. |
| 4 | Architecture | Should test organization match Dia's filesystem or module YAML structure? | Filesystem - easier for developers to navigate. Module YAML is for dependency analysis, not test organization. |
| 5 | Decisions | Should tests be allowed to depend on each other? | No - each test must be independent and runnable in isolation. This is a Google Test best practice. Add as AD-007 if needed. |
| 6 | Platform Dependencies | Should GoogleTests application depend on CluicheTest or just Dia? | Just Dia - GoogleTests validates engine only. CluicheTest has its own integration tests. Keep separation. |

## Status

`Active` - Primary unit testing application for the Dia engine
