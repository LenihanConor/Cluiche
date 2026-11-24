**Spec:** @docs/specs/features/draoi/guarantee-set-injection.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `GuaranteeSpec` (`site_kinds`, guarantees, marker overlay) + loader + schema + sidecar (`guarantees.json`, catalogue aliases `json.guarantees`/`json.guarantees_schema`); `SiteRegistry` accepts data-declared kinds | RED-GREEN: load/fault tests (AC 4); data-only kind listed, drawn, saved (AC 5) | Done | sonnet | New files in `WorldGeneration/` |
| 2 | Three generic rules `reach_any`, `min_spacing`, `variant_cover` replacing the CoW classes; `DefaultValidationSet` built from the spec | RED-GREEN: non-CoW kinds (AC 3); existing guarantee tests pass with data (AC 1) | Done | sonnet | Byte-identical diagnostics via data nouns/keys |
| 3 | Session consumes the spec: `SetGuaranteeSpec`, label table, order contract; remove hard-coded names | Empty spec (AC 2), order/golden (AC 1) | Done | sonnet | Fixtures install shipped spec |
| 4 | Generic marker overlay in `WorldOverlayGeometry`/`WorldGenViewHost`; module bootstrap installs the spec | Overlay tests spec-driven (AC 3, 6) | Done | sonnet | UI ids and labels unchanged |
| 5 | Gap audit, edge tests, full suite once | Full CoW suite sharded x4 | Done | sonnet | No mutation checks. Strata: guarantee `zone`/`to_zone` become `stratum`/`to_stratum` in the Strata feature (@docs/specs/features/draoi/generic-strata.md), not here  Full sharded x4: 4337 pass on rerun (first run had 2 DiaValueSchemaIntegration failures that pass alone and on rerun: flaky, unrelated) |
