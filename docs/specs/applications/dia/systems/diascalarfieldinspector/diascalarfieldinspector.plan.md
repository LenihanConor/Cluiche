**Spec:** @docs/specs/applications/dia/systems/diascalarfieldinspector/diascalarfieldinspector.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IDiaScalarField` virtual wrapper — `IDiaScalarField.h` + `DiaScalarFieldAdapter<T,P>` in DiaScalarField; snapshot ring buffer; write API delegation; vcxproj entries | `dia run googletest` (build smoke) | Done | sonnet | Game-side serialisation tools; SFI-001 revised — history lives in JS, not here |
| 2 | Module scaffold — `Dia/DiaScalarFieldInspector/` vcxproj, filters, module YAML; sln registration under 3.0-Gameplay | Build compiles | Done | haiku | GUID `9D90E0AC-3C29-403E-A65F-7723B3ABFBFB` |
| 3 | C++ plugin — `DiaScalarFieldInspectorPlugin.h/.cpp`: LiveConnectionPluginBase, subscribe to `scalarfield.state`, three write request handlers, REGISTER_EDITOR_PLUGIN; update vcxproj + filters + add DiaJson ref | `dia run googletest` (build smoke) | Pending | sonnet | |
| 4 | Web UI — `UI/index.html`: field selector, stats, heatmap grid, hover tooltip, 60-frame JS history ring + scrub slider, write authoring panels; `UI/package.json` | Manual / visual inspection | Pending | sonnet | Vanilla JS + CSS, same pattern as DiaBlackboardInspector |
| 5 | GoogleTests — `IDiaScalarField` adapter correctness, snapshot ring, GetWidth/GetHeight; wire into GoogleTests.vcxproj | `dia run googletest --filter="ScalarFieldInspector*"` | Pending | sonnet | |
