Throwaway fixtures for A4 WeatherModule AC 1 (not real manifests; no hosting stage exists until S-2.5).

Each directory holds a `.diaapp` plus a `host.diagame` that imports it. Composition-level checks (module
`dependencies` presence) only run on the composed `.diagame`, so validate `host.diagame`. Automated consumer:
`Dia/DiaCLI/tests/test_validate_dependencies_present.py`.

- `valid/`: WeatherModule listed after WorldStepModule, every declared dependency present. Accepted.
- `negative_order/`: WeatherModule listed before WorldStepModule. Rejected with REQUIRES_TICK_AFTER_ORDER (the module
  declares `requires_tick_after: ["WorldStepModule"]`).
- `negative_missing_dependency/`: WorldGenerationModule omitted. Rejected with REQUIRES_DEPENDENCY_MISSING naming
  WeatherModule and WorldGenerationModule.
