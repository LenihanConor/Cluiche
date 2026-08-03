**Spec:** @docs/specs/applications/dia/systems/diasteering/diasteering.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create DiaSteering.vcxproj static library + directory structure (Dia/DiaSteering/, Testing/, Docs/) | Build succeeds | Pending | haiku | |
| 2 | Register DiaSteering in Cluiche.sln under 3.0-Gameplay + add ProjectReference to GoogleTests.vcxproj | sln builds cleanly | Pending | haiku | |
| 3 | Implement SteeringAgent struct + Seek/Flee/Arrive/Wander/Pursue/Evade free functions | Unit tests for all 6 behaviours | Pending | sonnet | |
| 4 | Implement ObstacleAvoidance + Separation free functions | Unit tests for avoidance semantics | Pending | sonnet | |
| 5 | Implement SteeringPipeline (AddContribution/Clear/Evaluate, priority-group blending) | Unit tests for priority-group semantics (SD-003) | Pending | sonnet | |
| 6 | Implement SteeringSystem (AddAgent/RemoveAgent/UpdateAgentState/GetOutput/Update) + lifecycle logging | Unit tests for agent registry + output cache | Pending | sonnet | |
| 7 | Implement test utilities in DiaSteering/Testing/ (AssertSeekDirection, AssertArriveDeceleration, AssertSeparationDirection, MockObstacleSet) | Utility self-tests | Pending | sonnet | |
| 8 | Write GoogleTests (DiaSteering folder) — behaviours, pipeline, system, boundary, golden, stress | All tests pass via dia run googletest --filter="Steering*" | Pending | sonnet | |
| 9 | Write dia.steering.architecture.module.md YAML module doc | dia docs registry --dry-run clean | Pending | haiku | |
| 10 | Update module registry + backlog + mark spec Done | dia docs spec-done | Pending | haiku | |
