**Spec:** @docs/specs/applications/dia/systems/diascalarfieldinspector/diascalarfieldinspector.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IDiaScalarField` virtual wrapper — add `IDiaScalarField.h` + `DiaScalarFieldAdapter<T,P>` template to DiaScalarField; snapshot ring buffer (SFI-001); write API delegation; update `DiaScalarField.vcxproj` | `dia run googletest --filter="ScalarFieldInspector*"` | Pending | sonnet | Foundation for all other tasks |
| 2 | Module scaffold — create `Dia/DiaScalarFieldInspector/` with vcxproj, vcxproj.filters, module YAML; add to `Cluiche.sln` under 3.0-Gameplay | Build compiles | Pending | haiku | |
| 3 | `ScalarFieldInspectorPanel` implementation — all 6 features: field registry list, stats row, heatmap ImGui table, cell hover tooltip, scrub slider, write authoring form | Manual test + `dia run googletest --filter="ScalarFieldInspector*"` | Pending | opus | Complex ImGui visual component |
| 4 | GoogleTests — `IDiaScalarField` adapter correctness, snapshot ring, stats computation, Register/Unregister; wire into `GoogleTests.vcxproj` | `dia run googletest --filter="ScalarFieldInspector*"` | Pending | sonnet | |
