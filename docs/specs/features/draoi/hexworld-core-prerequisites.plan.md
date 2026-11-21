**Spec:** @docs/specs/features/draoi/hexworld-core-prerequisites.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Split `TerrainEnums.h`: cell vocabulary to core header in `World/`, `BarrierForm` stays in `WorldGeneration/`; fix includes; `BiomeRegistry.h` includes core only | Build + scoped CoW worldgen/biome tests; goldens unchanged | Done | sonnet | New `World/CellVocabulary.h`; `TerrainEnums.h` re-exports it. Scoped run: 565 passed, 4 perf skipped |
| 2 | Pin rule ids: registry stores `StringCRC` + name, generator rule table with 5 built-ins, `BiomeClassify` resolves via table | RED-GREEN: unknown rule fails load loud; biomes file with no `vent` biome loads and generates; goldens unchanged | Pending | sonnet | "Game without volcanoes" proof |
| 3 | Merged old 3+4 (inseparable: removing the enumerators forces every use site to change in one build). `zones.json` loader (sidecar-owned shares), `DepthZone` as index type, replace `kZoneNames[]` and all named-enumerator uses (~29 files) | RED-GREEN: zone table tests; `WorldJson` round trip; renamed-zones file loads and generates; goldens unchanged | Pending | sonnet | Zone count stays 3. Runs after 2 (both edit BiomeRegistry.cpp/.h) |
| 4 | Gap audit and edge tests | Full CoW suite once (sharded >= 2) | Pending | sonnet | No mutation checks |
