Generate comprehensive tests for a Dia module or component.

Usage: `/gen-tests <target> [options]`

Arguments:
- `<target>`: Module path (e.g., `DiaCore/Containers/Graphs`) or module name (e.g., `DiaSoftBody2D`)
- `--type=<unit|stress|golden|invariant|determinism|conservation|integration|all>`: Test types to generate (default: all applicable)
- `--dry-run`: Show proposed tests without writing files
- `--update-registry`: Only update the test-completeness-registry.md from current test files (no new tests)

## Instructions for Claude

When this skill is invoked, follow these steps strictly:

### Step 1 — Resolve Target

1. Map the target to a directory under `Dia/`:
   - Full path: `DiaCore/Containers/Graphs` → `Dia/DiaCore/Containers/Graphs/`
   - Module name: `DiaSoftBody2D` → `Dia/DiaSoftBody2D/`
   - Component shorthand: `Observer` → search for it under `Dia/`
2. If ambiguous, ask the user to clarify.
3. Read ALL public header files (`.h`) in the target directory.

### Step 2 — Audit Existing Coverage

1. Read `docs/reference/testing/test-completeness-registry.md`
2. Find the target's row(s) — note existing test counts by type (Unit, Stress/Boundary, Golden/Regression, etc.)
3. Search `Cluiche/Tests/GoogleTests/` for any existing test files covering this target
4. Read existing test files to understand what's already covered and what patterns are used

### Step 3 — Analyze API Surface

For each public class/function in the headers:
1. List public methods, constructors, operators
2. Classify the component type:
   - **Math/numeric**: needs golden-value + invariant/property tests
   - **Container/data structure**: needs stress (scale) + invariant (size consistency) tests
   - **Physics/simulation**: needs stress + golden-value + determinism + conservation tests
   - **State machine/lifecycle**: needs boundary (illegal transitions) + stress (rapid transitions)
   - **Serialization/protocol**: needs golden-value (round-trip) + boundary (malformed input)
   - **Interface/abstract**: needs mock-based unit tests
   - **Platform/OS**: needs testable pure-logic subset only
3. Note template parameters, inheritance, DIA_ASSERT preconditions (these become EXPECT_DEATH tests)

### Step 4 — Propose Tests

For each applicable test type, propose specific tests following these patterns:

**Unit tests**: `TEST(ComponentName, MethodName_Scenario_ExpectedBehavior)`
- One test per public method per interesting scenario
- Construction (default, parameterized, copy, move)
- Accessors (get/set round-trip)
- Operators (arithmetic, comparison, assignment)

**Boundary tests**: Focus on edges
- Zero, negative, max, min, empty, null, NaN, Inf, subnormal
- Out-of-bounds indices (EXPECT_DEATH)
- Precondition violations (EXPECT_DEATH for DIA_ASSERT)
- Capacity limits (full containers, max observers)

**Stress tests**: Scale and duration
- Large N (1000+ elements, 100+ bodies, 200+ particles)
- Rapid cycles (add/remove in loop)
- Sustained simulation (120+ steps, check no NaN/Inf)
- Multi-threaded contention (if API is mutex-protected)
- Pattern: follow `TestSoftBodyStress.cpp` — construct, simulate N steps, verify no NaN/Inf

**Golden-value tests**: Known-correct reference outputs
- Hand-calculated values (e.g., sqrt(9)==3, rotation matrix at 90 degrees)
- Analytical solutions (e.g., free fall: y = y0 + v*t + 0.5*g*t^2)
- Endpoint invariants (e.g., easing f(0)==0, f(1)==1)
- Use EXPECT_NEAR with documented tolerance

**Invariant/property tests**: Mathematical properties over random inputs
- Use `std::mt19937` with fixed seed for reproducibility
- 500-1000 iterations per invariant
- Commutativity: op(a,b) == op(b,a)
- Identity: op(a, identity) == a
- Inverse: op(a, inverse(a)) == identity
- Normalization: |normalize(v)| == 1 for non-zero v

**Determinism tests**: Bit-identical across runs (physics only)
- Run simulation twice with identical setup
- Compare all state (positions, velocities) with EXPECT_FLOAT_EQ
- Pattern: build world, step N times, record state, rebuild, step N times, compare

**Conservation law tests**: Physical invariants (physics only)
- Total momentum: sum(m*v) constant in closed system (no external forces)
- Total energy: sum(0.5*m*v^2) constant for elastic collisions
- Center of mass: CoM velocity constant without external forces

**Integration tests**: Cross-module data flow
- Only when the target interacts with other modules
- Uses existing fixtures (FakeInputSource, FakeCanvas, ProcessingUnitFixture)

Present the proposal as a table:
```
| # | Test Name | Type | What It Verifies |
```

If `--dry-run`, stop here and show the proposal.

### Step 5 — Check for Reusable Helpers

Before writing tests, check:
1. Does `Dia/<Module>/Testing/` exist? If yes, use its builders/factories.
2. Does the module need a new Testing/ helper? If the same setup (world creation, body creation, shape creation) would repeat 3+ times, create a builder in `Dia/<Module>/Testing/`.
3. Helpers must NOT depend on GoogleTest — pure library code only.
4. Existing fixtures in `Cluiche/Tests/GoogleTests/Fixtures/` (FakeInputSource, FakeCanvas, ProcessingUnitFixture) can be used directly.

### Step 6 — Write Tests

1. Determine the test file path: `Cluiche/Tests/GoogleTests/<Category>/<TestFileName>.cpp`
   - Category maps from module: DiaCore→Core/<Subdir>, DiaMaths→Maths, DiaGeometry2D→Geometry2D, etc.
   - File name: `Test<ComponentName>.cpp` (if new) or `Test<ComponentName><Type>.cpp` for stress/golden/invariant
   - If existing file covers the component, add to it unless it would exceed ~300 lines

2. Write the test file following conventions:
   - `#include <gtest/gtest.h>` first
   - Module includes next: `#include <DiaModule/Path/Header.h>`
   - `using namespace` only for the module under test
   - Test naming: `TEST(ModuleName_Component, Method_Scenario_Expected)`
   - Stress prefix: `TEST(ModuleName_Stress, Description)`
   - Golden prefix: `TEST(ModuleName_Golden, Description)`
   - Invariant prefix: `TEST(ModuleName_Invariant, Description)`
   - Determinism prefix: `TEST(ModuleName_Determinism, Description)`
   - Conservation prefix: `TEST(ModuleName_Conservation, Description)`
   - Arrange-Act-Assert structure, no comments unless non-obvious
   - EXPECT_NEAR for floats (document epsilon), EXPECT_DEATH for preconditions

3. If a Testing/ helper was needed (Step 5), write it first:
   - Location: `Dia/<Module>/Testing/<HelperName>.h`
   - Add to the module's `.vcxproj` and `.vcxproj.filters`

4. Add test `.cpp` to `GoogleTests.vcxproj` and `GoogleTests.vcxproj.filters`

### Step 7 — Build and Run

1. Build: `msbuild Cluiche/Cluiche.sln /p:Configuration=Debug /p:Platform=x64 /nologo /v:minimal`
2. If build fails, fix and retry
3. Run: `Cluiche/Tests/GoogleTests/bin/exe/Debug/GoogleTests.exe --gtest_filter=<NewTestPattern>`
4. If tests fail, diagnose and fix
5. Iterate until all new tests pass

### Step 8 — Update Registry

1. Read `docs/reference/testing/test-completeness-registry.md`
2. Update the target's row(s) with new test counts by type
3. Update the file(s) column
4. Update the Notes column (remove "ZERO" if applicable)
5. Update the totals at the top if significant

### Step 9 — Report

Show a summary:
```
## Tests Generated for <Target>
- New file(s): <paths>
- Tests added: X unit, Y stress, Z golden, ...
- Total new: N tests
- Registry updated: <component rows changed>
```
