**Spec:** @docs/specs/applications/dia/systems/diasensor/diasensor.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaSensor vcxproj + module doc + result structs (SightResult, ProximityResult, DamageEvent, SoundEvent) + SensorResultsComponent header + cpp | Compile only | Pending | sonnet | |
| 2 | SightSensorComponent — DIA_COMPONENT with range/halfAngle/layerMask/tickInterval fields; QuerySector integration | Unit test: sight query populates sightResults | Pending | sonnet | |
| 3 | ProximitySensorComponent — DIA_COMPONENT with radius/layerMask/tickInterval; QueryCircle integration | Unit test: proximity query populates proximityResults | Pending | sonnet | |
| 4 | DamageSensorComponent — DIA_COMPONENT; polls DamageReceivedComponent stub; prunes stale events by window | Unit test: damage event recording + pruning | Pending | sonnet | |
| 5 | SoundSensorComponent + SoundEventList — SensorModule::EmitSound(); spatial filter by distance | Unit test: sound event spatial filtering | Pending | sonnet | |
| 6 | SensorModule — ApplicationFlow::Module; owns SoundEventList; tick countdown; Update()+RunAdapters() frame ordering | Unit test: tick rate skipping; frame order (sensors before adapters) | Pending | sonnet | |
| 7 | SensorBlackboardAdapter — abstract base + default impl writing ThreatBoard + AwarenessBoard to BlackboardComponent | Unit test: adapter distillation into blackboard slots | Pending | sonnet | |
| 8 | Test Utilities — SensorTestHelpers.h with domain setup helpers, synthetic result injection, blackboard assertions | Used by all tests | Pending | sonnet | |
| 9 | GoogleTest file — TestDiaSensor.cpp covering all 13 ACs; add to GoogleTests.vcxproj | dia run googletest --filter="DiaSensor*" | Pending | sonnet | |
| 10 | Registry + vcxproj wiring — add DiaSensor to module-registry.md; add ProjectReference to GoogleTests.vcxproj | dia run googletest passes | Pending | haiku | |
