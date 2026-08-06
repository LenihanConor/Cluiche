**Spec:** @docs/specs/applications/dia/systems/diasensor/diasensor.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaSensor vcxproj + module doc + result structs (SightResult, ProximityResult, DamageEvent, SoundEvent) + SensorResultsComponent header + cpp | Compile only | Done | sonnet | DamageReceivedComponent stub defined in DiaSensor (not coupled to combat system) |
| 2 | SightSensorComponent — DIA_COMPONENT with range/halfAngle/layerMask/tickInterval fields; QuerySector integration | Unit test: sight query populates sightResults | Done | sonnet | Tick takes Domain& for position lookup; default forward dir = (1,0) |
| 3 | ProximitySensorComponent — DIA_COMPONENT with radius/layerMask/tickInterval; QueryCircle integration | Unit test: proximity query populates proximityResults | Done | sonnet | |
| 4 | DamageSensorComponent — DIA_COMPONENT; polls DamageReceivedComponent stub; prunes stale events by window | Unit test: damage event recording + pruning | Done | sonnet | Rebuilds kept-array pattern for prune (DynamicArrayC has no remove-if) |
| 5 | SoundSensorComponent + SoundEventList — SensorModule::EmitSound(); spatial filter by distance | Unit test: sound event spatial filtering | Done | sonnet | Dual-radius gate: emissionRadius AND hearingRadius both satisfied to hear |
| 6 | SensorModule — ApplicationFlow::Module; owns SoundEventList; tick countdown; Update()+RunAdapters() frame ordering | Unit test: tick rate skipping; frame order (sensors before adapters) | Done | sonnet | EntityCountdowns struct per slot; GetBlackboard() on abstract base resolved in T7 |
| 7 | SensorBlackboardAdapter — abstract base + default impl writing ThreatBoard + AwarenessBoard to BlackboardComponent | Unit test: adapter distillation into blackboard slots | Done | sonnet | BindBlackboard() call required after entity setup; lastKnownEnemyPosition stays (0,0) — SightResult has no abs pos |
| 8 | Test Utilities — SensorTestHelpers.h with domain setup helpers, synthetic result injection, blackboard assertions | Used by all tests | Done | sonnet | |
| 9 | GoogleTest file — TestDiaSensor.cpp covering all 13 ACs; add to GoogleTests.vcxproj | dia run googletest --filter="DiaSensor*" | Done | sonnet | 24 tests pass; pool registration fix: DefaultSensorBlackboard registered under base kTypeId |
| 10 | Registry + vcxproj wiring — add DiaSensor to module-registry.md; add ProjectReference to GoogleTests.vcxproj | dia run googletest passes | Done | haiku | |
