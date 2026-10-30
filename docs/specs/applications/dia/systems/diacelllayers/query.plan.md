**Spec:** @docs/specs/applications/dia/systems/diacelllayers/query.md
**Status:** In Progress

Feature 3 of DiaCellLayers (built fourth, after Features 2 and 4). **Prerequisite:** Feature 2 Done (`CellSelection`, `LayerReader`, selection commands). TDD throughout. Commit after each task. No arithmetic, functions or spatial operators (system decision); those would be a new feature.

**Done means (system-plan rule):** the `CellLayersViewer` stage E2E scenario is extended for this feature (Task 8).

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `QueryLexer`: identifiers `[A-Za-z_][A-Za-z0-9_.]*`, numbers `[+-] digits [. digits]` (no exponent/units), operators `< <= > >= == = !=`, parentheses, lower-case keywords `and`/`or`/`not`, token positions; 512-character limit (AC 7) | RED-GREEN: every token kind, positions, bad number, over-length | Done | sonnet | `QueryError { message, column, length, suggestion? }` defined here with DiaCore string types (PD-004); QueryError + QueryLexer, 17 tests GREEN. No RED run captured (tests+impl written before first build). QueryError uses fixed char buffers |
| 2 | `QueryParser`: grammar with precedence `not` > `and` > `or`, parentheses, comparison forms; depth limit 16; errors for unbalanced parentheses, missing operand, unexpected token (AC 3, 7) | RED-GREEN: each grammar rule, precedence table, all parse error kinds with column and length | Done | sonnet | AST nodes fixed-capacity (no STL in public headers); QueryParser, 32 tests; RED 31 failed on stub then GREEN. Eq covers = and ==; mValue double |
| 3 | `QueryResolver`: layer names to layers on the active grid by registered id first, then unique short schema key (after the last `/`); ambiguity lists candidates by full id; unknown name suggests the closest (ratio at least 0.6); enum ids looked up case-sensitively in the schema enum with closest-id suggestion; numeric operator on a category layer is an error (AC 2, 4, 5) | RED-GREEN: registered id, unique short key, ambiguous, unknown, wrong grid, exact/wrong-case/unknown enum | Pending | sonnet | Suggestion ratio helper lives here unless DiaCore already has one (check first, reuse if so) |
| 4 | `QueryEvaluator`: one bitset per comparison via `LayerReader` (one pass over values), `and`/`or`/`not` as bitset operations; NaN makes every comparison false including `!=` (AC 1, 2, 3, 6, 9) | RED-GREEN: each operator incl. equality boundaries, NaN per operator, expression-to-expected-set table, substituted value source returns scrubbed values | Pending | sonnet | Expression-to-expected-set helper goes in `Testing/`. `DIA_PROFILE_SCOPE` on parse and evaluate |
| 5 | Commands `cells.select.query "<expr>"` (replaces the selection, version bumps once) and `cells.query.check "<expr>"` (no change); error object `{ok:false, error:{message, column, length, suggestion?}}`; `CEL` Info for run and reject (AC 8) | RED-GREEN: both commands, selection unchanged on error, single version bump, check agrees with select count | Pending | sonnet | Same registration style as Feature 2's commands |
| 6 | Observation + perf: metrics (queries run, rejected, evaluate ms, last match count); Large grid, 6 comparisons across 4 layers within a frame (AC 10) | RED-GREEN: metric values; perf test | Pending | haiku | Open Q1: also time `check` against a 20-layer grid once the viewer exists; raise the debounce if needed |
| 7 | Panel card: query box with Run, inline error with highlighted span and suggestion, debounced live match count from `cells.query.check`, last 10 session queries; contract golden updated; both panel pages (AC 11, 12) | Golden + page tests; visual check against the mockup | Pending | sonnet | The mockup has a query box; user approves the visual check. Debounce lives in the page script |
| 8 | Host wiring + E2E: stage layers give enough variety to query (float + category with schema enum); extend `test_celllayers_smoke.py` for `cells.select.query` and `cells.query.check` (count agreement, error object with suggestion, selection unchanged on error, version bump once) | `dia test e2e --scenario="*celllayers*"` | Pending | sonnet | Scrub-consistency (AC 9) is covered by unit tests with a stub source; the full scrub E2E waits for DiaCellHistory |
| 9 | Finalize: `dia check deps`, STL grep, module doc, Observation Opportunity Scan, GoogleTests, spec Done via `dia docs spec-done`, system plan, backlog | `dia check deps`, `dia run googletest`, E2E | Pending | haiku | Not Done until Task 7's visual gate is approved |

## Open items carried from the spec

1. Live preview cost on a 20-layer grid (Task 6).
2. Scrub-consistency depends on DiaCellHistory's value-source hook; live values until then.
3. Spatial operators would be a new feature.
