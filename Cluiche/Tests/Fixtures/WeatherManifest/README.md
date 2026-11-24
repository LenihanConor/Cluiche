Throwaway fixtures for A4 WeatherModule AC 1 (not real manifests; no hosting stage exists until S-2.5).

- `valid/weather_hosting.diaapp`: WeatherModule listed after WorldStepModule. `dia validate manifest` accepts it.
- `negative_order/weather_before_worldstep.diaapp`: WeatherModule listed before WorldStepModule. `dia validate manifest`
  rejects it with REQUIRES_TICK_AFTER_ORDER (the module declares `requires_tick_after: ["WorldStepModule"]`).
