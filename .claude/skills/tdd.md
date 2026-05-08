---
name: tdd
description: Test-Driven Development for new features — RED (failing test) → GREEN (minimal implementation) → REFACTOR. Applies to all new feature work.
tags: [testing, tdd, red-green-refactor]
user_invocable: false
agent_invocable: true
---

# Test-Driven Development (New Features)

Applies to: all new feature implementation tasks (new public API, new behavior, new ACs).
Does NOT apply to: bug fixes in existing code, mechanical tasks (vcxproj edits, registry updates), refactors with existing test coverage, exploratory prototypes.

## The Cycle

### RED — Write a failing test first

1. Read the AC or task description.
2. Write a test that asserts the desired behavior. The test uses the API as a caller would.
3. Run the test: `dia run googletest --filter="SuiteName*"`
4. **Confirm it FAILS.** Quote the failure output.
   - If it passes: the behavior already exists (or the test is tautological). Investigate which.
   - If it fails to compile: that's fine if the function doesn't exist yet. But the test structure must be valid once the function signature exists.

### GREEN — Write the minimum code to pass

1. Implement only enough code to make the failing test pass. No more.
2. Run the test again.
3. **Confirm it PASSES.** Quote the passing output.
4. Do not add "nice to have" code, extra methods, or preemptive generalizations.

### REFACTOR — Clean up while green

1. Now that it passes, look at the implementation:
   - Duplication to extract?
   - Naming to improve?
   - Unnecessary allocation to remove?
2. After any refactor edit, re-run the test to confirm still GREEN.
3. This step is optional if the code is already clean from GREEN.

## How Tasks Map

A plan task for a new feature looks like:

```
| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | AC1: Graph accepts typed nodes | TestTypedNodeInsertion FAILS then PASSES | Not Started | sonnet | |
| 2 | AC2: Edges enforce type constraints | TestEdgeTypeConstraint FAILS then PASSES | Not Started | sonnet | |
```

The "Test" column names the test that defines done. The task isn't "implement X" — it's "make this test pass."

## When to Skip RED

You may skip the RED step (go straight to implement + test) for:
- Tasks that are purely structural (file creation, vcxproj wiring, namespace setup)
- Tasks where "failing" means "doesn't compile" and the test can't even be written yet (e.g., you need a class to exist before you can test its methods — write the empty class, THEN start RED-GREEN for its behavior)
- Bug fixes in existing code (write a regression test after fixing, confirming the bug is gone)

Even when skipping RED, you still need a passing test before marking Done (verification gate).

## Integration with `/gen-tests`

TDD covers the happy path and the ACs. After the feature is complete, run `/gen-tests` as an exhaustiveness audit to catch:
- Edge cases you didn't think of
- Stress/boundary tests
- Determinism/conservation tests (for physics/math)

TDD writes the *essential* tests. `/gen-tests` writes the *thorough* tests.

## Anti-Patterns

- **Test that can't fail:** `EXPECT_TRUE(true)` or testing the mock instead of the real code
- **Writing implementation first "just to see":** Then you're test-after. Delete the implementation, write the test, start over.
- **Gold-plating in GREEN:** Adding features the test doesn't require. Stop at green.
- **Skipping REFACTOR:** If you notice duplication or a bad name, clean it NOW while you have a passing test as a safety net. This is the cheapest moment to refactor.
- **One giant test per AC:** Break ACs into multiple small tests if the AC covers multiple behaviors.
