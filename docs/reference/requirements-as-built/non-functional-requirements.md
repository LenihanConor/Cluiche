# Non-Functional Requirements

**Last Updated:** 2026-04-01

Non-functional requirements covering performance, reliability, maintainability, and portability.

---

## Overview

Non-functional requirements define quality attributes and constraints that affect the entire system.

**Categories:**
- **Performance** - Speed, throughput, resource usage
- **Reliability** - Stability, error handling, thread safety
- **Maintainability** - Code quality, documentation, testability
- **Portability** - Platform support, dependencies

**Related Documents:**
- **[→ Main Requirements](requirements.md)** - Complete requirements list
- **[→ Functional Requirements](functional-requirements.md)** - Feature requirements
- **[→ Performance Testing](../testing/performance-testing.md)** - Performance test strategy

---

## Performance Requirements

### NF-001: Target Frame Rate ✅ P0

**Requirement:**
Render thread must achieve 60 FPS consistently.

**Rationale:**
- 60 FPS is standard for smooth gameplay
- VSync-locked to monitor refresh rate
- User expectation for responsive feel

**Acceptance Criteria:**
- [ ] Render thread maintains 60 FPS under normal load
- [ ] Frame time ≤ 16.67ms (1000ms / 60 FPS)
- [ ] No frame drops during typical gameplay
- [ ] Render profiler shows < 16ms frame time

**Measurement:**
- Visual Studio Profiler (CPU Usage)
- In-game FPS counter
- Frame time histogram

**Implementation:**
- VSync enabled by default
- Render loop optimized (minimal work per frame)
- Deferred rendering where possible

**Test Cases:**
- Empty scene (baseline)
- 100 entities
- 1000 entities
- Stress test (until frame drops)

**Status:** ✅ Complete (60 FPS achieved in debug and release)

---

### NF-002: Sim Thread Performance 🚧 P1

**Requirement:**
Sim thread must run faster than Render thread (> 60 FPS).

**Rationale:**
- Sim thread not VSync-locked
- Should produce updates faster than consumption
- Sim updates queued for Render thread

**Acceptance Criteria:**
- [ ] Sim thread runs at > 60 FPS
- [ ] Sim update time < 16ms (ideally < 10ms)
- [ ] No Sim thread bottlenecks
- [ ] Sim profiler shows acceptable performance

**Measurement:**
- Sim thread FPS counter
- Sim update time profiling
- Queue depth monitoring (Sim → Render)

**Implementation:**
- Sim loop optimized
- Expensive operations deferred or parallelized
- Physics/AI limited in scope

**Test Cases:**
- Empty sim (baseline)
- 100 entities with physics
- 1000 entities with physics
- Stress test

**Status:** 🚧 In Progress (needs profiling and optimization)

---

### NF-003: Memory Usage ✅ P1

**Requirement:**
Application memory usage must remain reasonable (< 500 MB typical).

**Rationale:**
- Desktop application targets modest hardware
- Memory leaks unacceptable
- Predictable memory profile

**Acceptance Criteria:**
- [ ] Baseline memory < 100 MB
- [ ] Typical gameplay < 500 MB
- [ ] No memory leaks (constant memory over time)
- [ ] No excessive allocations per frame

**Measurement:**
- Visual Studio Profiler (Memory Usage)
- Task Manager monitoring
- Memory leak detection tools (Dr. Memory)

**Implementation:**
- Object pools for frequent allocations
- RAII for automatic cleanup
- Smart pointers where appropriate

**Test Cases:**
- Memory baseline (startup)
- Memory after level load
- Memory after 10 minutes gameplay
- Memory leak test (level transitions, 100 iterations)

**Status:** ✅ Complete (no known leaks, reasonable usage)

---

### NF-004: Startup Time ✅ P2

**Requirement:**
Application startup time < 3 seconds (from launch to game).

**Rationale:**
- Fast iteration during development
- Good user experience
- Quick testing

**Acceptance Criteria:**
- [ ] Startup time < 3 seconds (Debug)
- [ ] Startup time < 1 second (Release)
- [ ] Measured from process start to first rendered frame

**Measurement:**
- Stopwatch (manual)
- Profiler startup time
- Log timestamps (first line → first frame)

**Implementation:**
- Lazy initialization where possible
- Asset loading deferred to level load
- Minimal work in constructors

**Test Cases:**
- Cold startup (no cache)
- Warm startup (cached)

**Status:** ✅ Complete (< 2 seconds Debug, < 1 second Release)

---

### NF-005: Container Performance ✅ P1

**Requirement:**
Custom containers must have comparable performance to STL.

**Rationale:**
- Justify custom container implementation
- Avoid performance regressions
- Maintain developer productivity

**Acceptance Criteria:**
- [ ] DynamicArray comparable to std::vector (< 10% slower)
- [ ] HashTable comparable to std::unordered_map (< 20% slower)
- [ ] No O(n²) algorithms where O(n) or O(log n) expected

**Measurement:**
- Benchmark suite (add, remove, iterate, search)
- Compare against STL
- Profile in real usage

**Implementation:**
- Optimized implementations
- Cache-friendly data layouts
- Profiling-driven optimization

**Test Cases:**
- Micro-benchmarks (add, remove, find)
- Macro-benchmarks (real workloads)

**Status:** ✅ Complete (acceptable performance, some faster than STL)

**See [DiaCore API](../api/dia/core-api.md) for container details**

---

## Reliability Requirements

### NF-006: Test Coverage ❌ P1

**Requirement:**
Code coverage ≥ 60% for core systems.

**Rationale:**
- Tests catch regressions
- Confidence in refactoring
- Documentation via tests

**Acceptance Criteria:**
- [ ] DiaCore: ≥ 70% coverage
- [ ] DiaMaths: ≥ 70% coverage
- [ ] DiaApplicationFlow: ≥ 60% coverage
- [ ] Cluiche: ≥ 40% coverage (lower due to integration code)

**Measurement:**
- Code coverage tool (Visual Studio or external)
- Coverage reports per module

**Implementation:**
- Unit tests for all public APIs
- Integration tests for cross-module interactions
- Regression tests for bugs

**Test Cases:**
- All container operations
- All math operations
- Module lifecycle
- Phase transitions
- Level loading/unloading

**Status:** ❌ Not Started (no coverage measurement yet)

**[→ Test Coverage Targets](../testing/test-coverage-targets.md)**

---

### NF-007: Error Handling ✅ P0

**Requirement:**
System must handle errors gracefully without crashes.

**Rationale:**
- Crashes are unacceptable (lose work, bad UX)
- Errors should be logged and recoverable
- Development builds can assert for debugging

**Acceptance Criteria:**
- [ ] No crashes from null pointers (assertions in Debug)
- [ ] No crashes from out-of-bounds access (assertions in Debug)
- [ ] File load failures handled gracefully
- [ ] Invalid input rejected (not crash)

**Measurement:**
- Manual testing (error scenarios)
- Fuzzing (invalid inputs)
- Assertions fire in Debug, no crash in Release

**Implementation:**
- DIA_ASSERT for preconditions (Debug only)
- Null checks before dereference
- Bounds checks in containers
- Return codes for fallible operations

**Test Cases:**
- Load missing file
- Invalid level name
- Out-of-bounds access
- Null pointer scenarios

**Status:** ✅ Complete (assertions in place, no known crashes)

---

### NF-008: Thread Safety 🚧 P0

**Requirement:**
Multi-threaded code must be free of race conditions and deadlocks.

**Rationale:**
- Race conditions cause non-deterministic bugs
- Deadlocks hang application
- Threading bugs hard to diagnose

**Acceptance Criteria:**
- [ ] No race conditions in normal operation
- [ ] No deadlocks in normal operation
- [ ] Thread-safe APIs documented
- [ ] FrameStreams protect cross-thread communication
- [ ] Mutexes used for shared mutable state

**Measurement:**
- ThreadSanitizer (if available)
- Manual code review
- Stress testing (run for hours)

**Implementation:**
- FrameStream for cross-thread data
- std::mutex for shared state
- Const data shared freely
- Thread affinity documented

**Test Cases:**
- Concurrent module updates
- Concurrent FrameStream writes/reads
- Stress test (100+ updates/second)

**Status:** 🚧 In Progress (known issues in Transform2D, Random fixed)

**[→ Thread Safety Guide](../ai-guides/thread-safety-guide.md)**  
**[→ Thread Safety Testing](../testing/thread-safety-testing.md)**

---

### NF-009: Assertion Coverage ✅ P1

**Requirement:**
Critical invariants protected by assertions.

**Rationale:**
- Catch bugs early (fail fast)
- Document preconditions
- No overhead in Release

**Acceptance Criteria:**
- [ ] All public APIs check preconditions
- [ ] All array access bounds-checked
- [ ] All pointers checked before dereference
- [ ] All critical invariants asserted

**Measurement:**
- Code review
- Count DIA_ASSERT usage
- Test that assertions fire for invalid input

**Implementation:**
- DIA_ASSERT(condition, "message")
- Compiled out in Release (/DNDEBUG)

**Test Cases:**
- Assertions fire for invalid input
- Assertions don't fire for valid input
- No assertions in Release builds

**Status:** ✅ Complete (assertions widely used)

---

## Maintainability Requirements

### NF-010: Code Documentation ✅ P1

**Requirement:**
Public APIs must be documented.

**Rationale:**
- Developers can understand APIs without reading implementation
- Reduces onboarding time
- Serves as specification

**Acceptance Criteria:**
- [ ] All public classes have header comments
- [ ] All public methods have documentation
- [ ] Complex algorithms explained
- [ ] API documentation generated and published

**Measurement:**
- Manual review
- Documentation coverage tool
- Feedback from new developers

**Implementation:**
- Doxygen-style comments (/** ... */)
- API documentation in `/docs/05-api/`

**Status:** ✅ Complete (API documentation comprehensive)

**[→ API Documentation](../api/api-overview.md)**

---

### NF-011: Architecture Documentation ✅ P0

**Requirement:**
System architecture must be documented.

**Rationale:**
- New developers can understand system quickly
- Onboarding takes weeks instead of months
- Design decisions preserved

**Acceptance Criteria:**
- [ ] High-level architecture documented
- [ ] Threading model documented
- [ ] Module system explained
- [ ] Design rationale captured

**Measurement:**
- Documentation review
- Feedback from new developers
- Architecture diagrams exist

**Implementation:**
- Architecture docs in `/docs/reference/architecture/`
- Design rationale in `/docs/reference/design-rationale/`
- Mermaid diagrams for visualization

**Status:** ✅ Complete (comprehensive architecture documentation)

**[→ Architecture Documentation](../architecture/architecture.md)**

---

### NF-012: Coding Standards ✅ P1

**Requirement:**
Code must follow consistent style.

**Rationale:**
- Readability
- Maintainability
- Professionalism

**Acceptance Criteria:**
- [ ] Consistent naming (PascalCase, mCamelCase, kPascalCase)
- [ ] Consistent formatting (tabs, braces on new line)
- [ ] Consistent file organization
- [ ] Style guide documented

**Measurement:**
- Code review
- Automated linting (if available)

**Implementation:**
- Coding standards documented
- Code review enforces standards

**Status:** ✅ Complete (standards documented and followed)

**[→ Coding Standards](../development/coding-standards.md)**

---

### NF-013: Build System ✅ P0

**Requirement:**
Project must build reliably.

**Rationale:**
- Broken builds block development
- New developers must build successfully
- CI/CD requires reliable builds

**Acceptance Criteria:**
- [ ] Clean build from fresh clone
- [ ] Build configurations (Debug, Release) work
- [ ] External dependencies included or documented
- [ ] Build errors clear and actionable

**Measurement:**
- Fresh clone build test
- CI build (if configured)
- New developer feedback

**Implementation:**
- Visual Studio solution (.sln)
- External dependencies in repository
- Build guide documented

**Status:** ✅ Complete (builds reliably)

**[→ Building the Project](../getting-started/building-the-project.md)**

---

## Portability Requirements

### NF-014: Platform Support ✅ P1

**Requirement:**
Support Windows platform, designed for cross-platform.

**Rationale:**
- Primary development on Windows
- Architecture supports other platforms
- Future Linux/macOS support possible

**Acceptance Criteria:**
- [ ] Windows 10/11 support
- [ ] No Windows-specific code in Dia core
- [ ] Platform abstraction for graphics, input, audio
- [ ] Can build on Linux (future)

**Measurement:**
- Windows builds and runs
- No platform headers in core (grep check)
- Backend isolation verified

**Implementation:**
- Platform-agnostic core (Dia)
- Platform-specific backends (DiaSFML)

**Status:** ✅ Complete (Windows supported, architecture platform-agnostic)

---

### NF-015: External Dependencies ✅ P1

**Requirement:**
Minimize external dependencies, document all.

**Rationale:**
- Dependencies are maintenance burden
- License compliance required
- Build complexity

**Acceptance Criteria:**
- [ ] All dependencies documented (version, license, purpose)
- [ ] Dependencies included in repository or easy to obtain
- [ ] Core systems avoid external dependencies where possible

**Measurement:**
- Dependency audit
- License review

**Implementation:**
- External dependencies documented
- SFML (zlib license), JsonCpp (MIT)

**Status:** ✅ Complete (dependencies documented)

**[→ External Dependencies](../architecture/external-dependencies.md)**  
**[→ External Links](../registry/external-links.md)**

---

## Summary

**Total Non-Functional Requirements:** 15
- **Performance:** 5 requirements
- **Reliability:** 4 requirements
- **Maintainability:** 4 requirements
- **Portability:** 2 requirements

**Status Breakdown:**
- ✅ Complete: 12 (80%)
- 🚧 In Progress: 2 (13%)
- ❌ Not Started: 1 (7%)

**Priority Breakdown:**
- P0 (Critical): 5
- P1 (High): 9
- P2 (Medium): 1

**Key Gaps:**
- NF-006: Test Coverage (not measured yet)
- NF-002: Sim Thread Performance (needs profiling)
- NF-008: Thread Safety (Transform2D issue)

**Next Steps:**
1. Set up code coverage measurement (NF-006)
2. Profile Sim thread performance (NF-002)
3. Fix Transform2D thread safety (NF-008)

**[→ Main Requirements](requirements.md)**  
**[→ Functional Requirements](functional-requirements.md)**  
**[→ Performance Testing](../testing/performance-testing.md)**
