# Feature Spec: DiaCellLayers Query

**Parent:** @docs/specs/applications/dia/systems/diacelllayers/diacelllayers.md
**Builds on:** @docs/specs/applications/dia/systems/diacelllayers/registry-and-viewer.md (Feature 1), @docs/specs/applications/dia/systems/diacelllayers/selection.md (Feature 2)

**Gameplay Domains:** debug, spatial

## Summary

A small query language that selects cells by their layer values, for example `vegetation < 0.2 and biome = Grassland`. It delivers the parser, the evaluator, the `cells.select.query` command that replaces the selection with the matching cells, a `cells.query.check` command that validates a query and counts matches without changing the selection, precise error messages with a "did you mean", and a query box in the viewer card. No scripting and no arithmetic.

## Problem

Clicking, boxing and picking regions cannot express "all the dry, overgrazed grassland cells" or the checks S-3's scenarios make ("cells below 0.2 vegetation in the Coast region"). A tiny comparison language, shared by the panel, the console and scenario files, covers those without a scripting engine.

## Goals

1. **Compare layers to numbers and category ids**, joined with `and`, `or`, `not` and parentheses.
2. **Readable names.** Layers are written by registered id or by short schema key when that is unique.
3. **Precise errors.** A bad query returns a message, the position of the problem and, when close, a suggestion; the selection is unchanged.
4. **Scrub-consistent.** A query runs against the values the viewer shows, so what you see is what you query.
5. **Fast.** A query over all cells of a Large grid and several layers completes within a frame budget.
6. **Previewable.** The UI can check a query and show the match count as the user types.

## Non-Goals

- Arithmetic, functions, aggregates (`mean`, `count`), string operations or variables (no arithmetic in v1, per the system spec's resolved decision).
- Neighbour or spatial operators (adjacent to, within N cells).
- Queries across several grids, or against layers of another grid.
- Saving queries in the viewer (scenario files hold their own strings; the card keeps recent queries for the session).
- Adding to or subtracting from the selection (a query replaces it, like the other tools).

## Architecture

Lives in `Dia/DiaCellLayers/` (namespace `Dia::CellLayers`).

```
QueryLexer / QueryParser   text → AST with source positions
QueryResolver              layer names → layers on the active grid; enum ids → values
QueryEvaluator             AST → bitset (one bitset per comparison; and/or/not as bitset operations)
QueryError                 { message, column, length, suggestion? }
```

### Grammar

```
query      := or
or         := and { "or" and }
and        := not { "and" not }
not        := "not" not | primary
primary    := "(" or ")" | comparison
comparison := name op number          // float (numeric) layers
            | name ( "=" | "==" | "!=" ) enumId      // category layers
op         := "<" | "<=" | ">" | ">=" | "==" | "=" | "!="
name       := identifier             // [A-Za-z_][A-Za-z0-9_.]*
number     := [+-] digits [ "." digits ]
```

- **Precedence:** `not` binds tighter than `and`, which binds tighter than `or`; parentheses override.
- **Keywords** `and`, `or`, `not` are lower-case only. Enum ids are matched **exactly as written in the schema, case-sensitively** (a wrong case gets a suggestion). Numbers are plain decimals: no exponent, units or percent.
- `=` and `==` are synonyms. Numeric layers take all of `< <= > >= == = !=`; category layers take only `=`, `==` and `!=`, and a numeric comparison on a category layer is an error.
- **Limits:** at most 512 characters and nesting depth 16; longer input is an error.
- **NaN and unset values:** a comparison involving NaN is false for every operator, including `!=`, so unset cells are never selected by accident.

### Name resolution

- A name is first matched against the registered layer ids of the **active grid**.
- Otherwise it is matched against each layer's **short schema key**, the part of `schemaKey` after the last `/` (for `cow.weather.cell/precipitation`, `precipitation`).
- If one layer matches, it is used. If more than one matches, the error lists the candidates by full id; if none, the error offers the closest layer name when close enough (ratio at least 0.6).
- An enum id is looked up in the layer's schema enum; an unknown id gets the closest id as a suggestion.

### Evaluation

- Each comparison reads its layer through Feature 2's `LayerReader` and produces a bitset (one pass over the layer's values).
- `and`, `or` and `not` are bitset operations; the result replaces the selection (Feature 2's `CellSelection`), so the version increments once.
- `LayerReader` returns the values the viewer shows for each layer. Until DiaCellHistory installs its value source (scrubbing and delta display), those are the live values; with it, they are the scrubbed sample's values.

### Commands

| Command | Effect |
|---|---|
| `cells.select.query "<expr>"` | Parse, resolve and evaluate; replace the selection with the matches. Returns `{ ok, count }`. On error returns `{ ok: false, error: { message, column, length, suggestion? } }` and changes nothing |
| `cells.query.check "<expr>"` | Same parse and resolve and evaluation, but the selection is not changed. Returns `{ ok, count }` or the same error object. The card uses it for the live preview |

### Viewer card

A text box with Run, an inline error display that highlights the offending span and shows the suggestion, a live match count from `cells.query.check` (debounced), and the last 10 queries of the session. The card JSON is added to the panel contract golden.

### Logging and observation

- **Channel:** `CEL`.
- **Info:** a query run (text, matches); a query rejected (message).
- **Traces / Profiling:** `DIA_PROFILE_SCOPE` on parse and on evaluate.
- **Metrics:** queries run, queries rejected, evaluate ms, last match count.
- **Health:** none beyond Feature 1's.

## Acceptance Criteria

1. Each numeric operator (`<`, `<=`, `>`, `>=`, `==`, `=`, `!=`) selects the correct cells on a fixture layer, including at equality boundaries.
2. A category comparison with `=`, `==` and `!=` selects the correct cells; a numeric operator on a category layer is an error.
3. `not` binds tighter than `and`, and `and` tighter than `or`; parentheses override; a table of expressions produces the expected sets.
4. Layers resolve by registered id or by unique short key on the active grid; an ambiguous short key errors with candidates; an unknown name errors with the closest suggestion when close.
5. Enum ids are case-sensitive; a wrong case or unknown id errors with a suggestion; keywords are lower-case only.
6. A comparison involving NaN is false for every operator, including `!=`.
7. Parse errors (unbalanced parentheses, missing operand, unexpected token, bad number, over 512 characters, depth over 16) return a message with column and length, and leave the selection unchanged.
8. `cells.select.query` replaces the selection with the matches and increments the selection version once; `cells.query.check` returns the same count without changing the selection.
9. The query uses the values `LayerReader` returns; with a value source installed that returns scrubbed values, results match the scrubbed sample.
10. A query over the 6,656 cells of a Large grid with 6 comparisons across 4 layers evaluates within a frame budget.
11. The card shows the inline error with the highlighted span and suggestion, the debounced match count, and the last 10 queries; the card JSON is in the panel contract golden.
12. Visual match against the mockup's query box.

## Test Strategy

- **GoogleTests (TDD):**
  - Lexer and parser: every grammar rule, positions, all error kinds.
  - A precedence table of expressions against hand-computed expected sets.
  - Name resolution: registered id, unique short key, ambiguous, unknown, wrong grid.
  - Enum matching: exact, wrong case, unknown.
  - NaN behaviour for each operator.
  - Commands: select and check, error objects, selection unchanged on error, version increments once.
  - `LayerReader` substitution with a stub value source.
- **Test utilities** in `DiaCellLayers/Testing/`: grid and layer fixtures, an expression-to-expected-set helper.
- **Panel contract golden** and page tests for the card.
- **Perf:** Large-grid evaluation timing.
- **Visual gate:** S-3 testbed against the mockup.

## Binding Decisions

| Source | Decision | How this feature complies |
|---|---|---|
| CL-003 | v1 selection includes query | Built here |
| CL-005 | Generic, no game types | Names and enum ids come from the schema |
| Resolved Decision 2 (system spec) | No arithmetic in v1 | Grammar has none |
| Editor actions via DiaAPI | Every action is a command | `cells.select.query` and `cells.query.check` |
| PD-004 | No STL in public APIs | DiaCore types for AST and results |
| VS-004 | Strict coverage | Enum ids and layer short keys come from schema entries |

No conflicts.

## Dependencies

| Dependency | Use | Status |
|---|---|---|
| Features 1 and 2 | Registry, grid, selection, `LayerReader` | Approved |
| DiaValueSchema Feature 1 | Enum values, short keys | Approved |
| DiaCellHistory value source | Scrub-consistent values | Draft (TBD); live values until then |
| S-3 scenarios | Use `cells.select.query` strings | Approved, not built |

## Open Design Questions

1. **Live preview cost.** `cells.query.check` runs the full evaluation for the card's preview. With debouncing it is cheap at Large, but check it against a grid of 20 layers once the viewer exists and raise the debounce if needed.
2. **Scrub-consistency dependency.** The "what you see" rule depends on DiaCellHistory's value-source hook, not yet specced. Until then queries use live values; confirm the hook shape when DiaCellHistory's viewer feature is written.
3. **Spatial operators.** If scenarios later need "next to water" or "within N cells", that is a new grammar addition and a new feature, not a quiet extension.

## Status

`Done` (2026-10-29). Plan: [query.plan.md](query.plan.md). Visual gate (AC 12) approved by the user 2026-10-29. The panel sends queries longer than one bridge argument (63 characters) as `query_part` chunks followed by `query_check` / `query_run` with the part count, so the panel accepts the full 512 characters. Structured error objects reach E2E clients: `DebugServer.cpp` puts the error's `message` in the response message and the whole `{message, column, length, suggestion?}` object in the payload (`AutomationError.payload` in the E2E client). Scrub-consistency (AC 9) is covered with a stub value source; the full scrub E2E waits for DiaCellHistory.
