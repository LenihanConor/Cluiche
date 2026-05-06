# Test Completeness Registry

**Last Updated:** 2026-05-06 (ManifestImports: added AC5/AC6/AC7/deep-nesting tests; ManifestComposer: AC5 duplicate rejection, depth limit, comments)

Single source of truth for test coverage across all Dia modules. Updated alongside test commits.

**Supersedes:** The per-module tables in [test-coverage-targets.md](test-coverage-targets.md) (April 2026, stale).

**Test framework:** GoogleTest, compiled from source (`External/googletest/`)
**Test project:** `Cluiche/Tests/GoogleTests/GoogleTests.vcxproj`
**Run:** `Cluiche/Tests/GoogleTests/bin/exe/Debug/GoogleTests.exe`

---

## Totals

| Metric | Count |
|--------|-------|
| Test files | 190 |
| Total tests (TEST + TEST_F + TEST_P) | 3,019 |
| Death tests (EXPECT_DEATH / ASSERT_DEATH) | 149 |
| Float assertions (EXPECT_NEAR / EXPECT_FLOAT_EQ) | 1,340+ |
| Fixtures (TEST_F) | ~504 |
| Parameterized (TEST_P / TYPED_TEST) | 0 |

---

## Legend

- **Unit**: Standard functional tests (TEST / TEST_F)
- **Stress/Boundary**: Scale tests (large N, many iterations), edge cases (zero, max, NaN, overflow), death tests (EXPECT_DEATH)
- **Golden/Regression**: Known-correct reference values, determinism checks, past-bug guards
- **Integration**: Cross-module data flow tests
- **Invariant/Property**: Mathematical property verification over random inputs
- **Determinism**: Bit-identical reproducibility across runs
- **Conservation**: Physical law invariants (momentum, energy)

Gap markers: **ZERO** = no tests, **LOW** = under-tested relative to API surface, **OK** = reasonable coverage, **GOOD** = well covered

---

## DiaCore

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Architecture/Component | 12 | TestComponent.cpp | 12 | 0 | 0 | OK |
| Architecture/Singleton | 4 | TestSingleton.cpp | 4 | 0 | 0 | LOW — needs thread-safety stress |
| Architecture/Observer | **0** | — | 0 | 0 | 0 | **ZERO** — mutex-protected, max 16 observers |
| Containers/Array | 18 | TestArray.cpp | 18 | 0 | 0 | OK |
| Containers/ArrayC | 14 | TestArrayC.cpp | 14 | 0 | 0 | OK |
| Containers/ArrayIterators | 12 | TestArrayIterators.cpp | 12 | 0 | 0 | OK |
| Containers/BitArray | 103 | TestBitArray.cpp | 97 | 6 (death) | 0 | GOOD — but 4 near-identical type variants, ideal for TYPED_TEST |
| Containers/CircularBuffer | 15 | TestCircularBuffer.cpp | 15 | 0 | 0 | OK — needs overflow stress |
| Containers/DynamicArray | 25 | TestDynamicArray.cpp | 25 | 0 | 0 | OK — needs stress at scale |
| Containers/DynamicArrayC | 22 | TestDynamicArrayC.cpp | 22 | 0 | 0 | OK — needs stress at scale |
| Containers/Graph | **0** | — | 0 | 0 | 0 | **ZERO** — Graph.h, GraphC.h, GraphNode.h, GraphEdge.h |
| Containers/HashTable | 30 | TestHashTable.cpp | 30 | 0 | 0 | OK — needs collision stress |
| Containers/LinkListC | 12 | TestLinkListC.cpp | 12 | 0 | 0 | OK |
| Containers/String | 8 | TestString.cpp | 8 | 0 | 0 | OK |
| Containers/StringWriter | 6 | TestStringWriter.cpp | 6 | 0 | 0 | OK |
| CRC/StringCRC | 28 | TestCRC.cpp | 28 | 0 | 0 | GOOD |
| Events/Delegate | 29 | TestDelegate.cpp | 29 | 0 | 0 | GOOD |
| Events/EventDispatcher | 27 | TestEventDispatcher.cpp | 27 | 0 | 0 | GOOD |
| Events/EventQueue | 22 | TestEventQueue.cpp | 22 | 0 | 0 | GOOD |
| Memory/SmartPointers | 28 | TestSmartPointers.cpp | 28 | 0 | 0 | OK — needs RefPtr stress, concurrent access |
| Memory/Allocators | **0** | — | 0 | 0 | 0 | **ZERO** — alignment helpers, IAllocator |
| FilePath | **0** | — | 0 | 0 | 0 | **ZERO** — FilePath, Path, PathStore |
| Threading/Atomic | 43 | TestAtomic.cpp | 43 | 0 | 0 | GOOD — needs multi-thread contention stress |
| Threading/JobSystem | 29 | TestJobSystem.cpp | 29 | 0 | 0 | GOOD — needs deep dependency chain stress |
| Threading/Mutex | 28 | TestMutex.cpp | 28 | 0 | 0 | GOOD |
| Threading/ThreadPool | 24 | TestThreadPool.cpp | 24 | 0 | 0 | OK — needs 1000-task stress |
| Time/TimeAbsolute | 20 | TestTimeAbsolute.cpp | 20 | 0 | 0 | OK |
| Time/TimeRelative | 38 | TestTimeRelative.cpp | 34 | 4 (death) | 0 | GOOD |
| Time/TimeServer | 21 | TestTimeServer.cpp | 21 | 0 | 0 | OK |
| Time/Timer | 13 | TestTimer.cpp | 13 | 0 | 0 | OK |
| Time/TimerExpiry | 6 | TestTimerExpiry.cpp | 6 | 0 | 0 | OK |
| Time/TimerSystem | 6 | TestTimerSystem.cpp | 6 | 0 | 0 | OK |
| Type/Enum | 17 | TestEnum.cpp | 17 | 0 | 0 | OK |
| Type/JsonTypes | 14 | TestJsonTypes.cpp | 14 | 0 | 0 | OK |
| Type/MetaLogic | 5 | TestMetaLogic.cpp | 5 | 0 | 0 | OK |
| Type/TypeTraits | 14 | TestTypeTraits.cpp | 14 | 0 | 0 | OK |
| Type/Types | 11 | TestTypes.cpp | 11 | 0 | 0 | OK |

**Core totals: 34 files, 744 tests** | Gaps: Graph, Observer, FilePath, Allocators (zero); stress tests needed for containers/threading

---

## DiaMaths

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Core/Angle | 41 | TestAngle.cpp | 41 | 0 | 0 | GOOD |
| Core/CoreMaths | 35 | TestCoreMaths.cpp | 35 | 0 | 0 | GOOD |
| Core/FloatMaths | 121 | TestFloatMaths.cpp | 78 | 32 (death) + 11 (boundary) | 0 | GOOD — needs NaN/Inf/subnormal boundary |
| Core/HalfFloat | 12 | TestHalfFloat.cpp | 12 | 0 | 0 | OK |
| Core/Easing | **0** | — | 0 | 0 | 0 | **ZERO** — 31 easing functions |
| Core/Interpolation | **0** | — | 0 | 0 | 0 | **ZERO** — Lerp, SmoothStep, Remap, MoveTowards |
| Core/Trigonometry | 24 | TestTrigonometry.cpp | 24 | 0 | 0 | OK |
| Vector/Vector2D | 38 | TestVector2D.cpp | 38 | 0 | 0 | GOOD |
| Vector/Vector3D | 43 | TestVector3D.cpp | 43 | 0 | 0 | GOOD |
| Vector/Vector4D | 34 | TestVector4D.cpp | 34 | 0 | 0 | GOOD |
| Vector/HalfVector2D | 16 | TestHalfVector2D.cpp | 16 | 0 | 0 | OK |
| Vector/VectorUtils | 33 | TestVectorUtils.cpp | 33 | 0 | 0 | GOOD |
| Matrix/Matrix22 | 97 | TestMatrix22.cpp | 97 | 0 | 0 | GOOD — needs golden composition tests |
| Matrix/Matrix33 | 30 | TestMatrix33.cpp | 30 | 0 | 0 | GOOD — needs golden composition tests |
| Shape/Circle2D | 30 | TestCircle2D.cpp | 30 | 0 | 0 | GOOD |
| Shape/IntersectionClassify | 5 | TestIntersectionClassify.cpp | 5 | 0 | 0 | LOW |

**Maths totals: 14 files, 561 tests** | Gaps: Easing, Interpolation (zero); golden-value for matrices; NaN/Inf boundary; property/invariant tests

---

## DiaGeometry2D

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Shapes/Primitives | 80 | TestShapePrimitives.cpp | 80 | 0 | 0 | GOOD — but many shapes untested (Arc, Capsule, Sector, OORect, Plane, Triangle) |
| Intersection | 28 | TestIntersectionTests.cpp | 28 | 0 | 0 | OK — many shape-pair combos missing |
| Transform | 28 | TestGeometry2DTransform.cpp | 28 | 0 | 0 | GOOD |
| Handle | 16 | TestHandle.cpp | 16 | 0 | 0 | OK |
| Spatial/SpatialGrid | 29 | TestSpatialGrid.cpp | 29 | 0 | 0 | GOOD |
| Spatial/BVH | **0** | — | 0 | 0 | 0 | **ZERO** — BVH.h |
| Spatial/Quadtree | **0** | — | 0 | 0 | 0 | **ZERO** — Quadtree.h |
| EdgeCases | 16 | TestEdgeCases.cpp | 16 | 0 | 0 | OK |

**Geometry2D totals: 6 files, 197 tests** | Gaps: BVH, Quadtree (zero); intersection golden values; spatial stress tests

---

## DiaRigidBody2D

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Bodies/PhysicsBody | 18 | TestPhysicsBody.cpp | 18 | 0 | 0 | OK |
| Bodies/Sleeping | 10 | TestBodySleeping.cpp | 10 | 0 | 0 | OK |
| Collision/Detection | 6 | TestCollisionDetection.cpp | 6 | 0 | 0 | LOW |
| Collision/Events | 8 | TestCollisionEvents.cpp | 8 | 0 | 0 | OK |
| Collision/Layers | 9 | TestCollisionLayers.cpp | 9 | 0 | 0 | OK |
| Collision/Response | 7 | TestCollisionResponse.cpp | 7 | 0 | 0 | OK |
| Constraints | 8 | TestConstraints.cpp | 8 | 0 | 0 | OK |
| Force/Integration | 11 | TestForceAndIntegration.cpp | 11 | 0 | 0 | OK |
| PhysicsIntegration | 12 | TestPhysicsIntegration.cpp | 12 | 0 | 0 | OK |
| PhysicsWorld | 13 | TestPhysicsWorld.cpp | 13 | 0 | 0 | OK |
| Robustness | 12 | TestRobustness.cpp | 0 | 12 | 0 | GOOD (boundary/stress) |
| SpatialQueries | 7 | TestSpatialQueries.cpp | 7 | 0 | 0 | OK |
| TriggerVolumes | 8 | TestTriggerVolumes.cpp | 8 | 0 | 0 | OK |

**RigidBody2D totals: 13 files, 129 tests** | Gaps: Golden simulation traces, determinism, conservation laws, scale stress (100+ bodies)

---

## DiaSoftBody2D

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Particle | 5 | TestParticle.cpp | 5 | 0 | 0 | OK |
| Rope | 19 | TestRope.cpp | 19 | 0 | 0 | GOOD |
| Cloth | 14 | TestCloth.cpp | 14 | 0 | 0 | GOOD |
| SoftBodyWorld | 19 | TestSoftBodyWorld.cpp | 19 | 0 | 0 | GOOD |
| GeometryCollision | 10 | TestGeometryCollision.cpp | 10 | 0 | 0 | OK |
| RigidBodyCoupling | 6 | TestRigidBodyCoupling.cpp | 6 | 0 | 0 | OK |
| Integration | 13 | TestSoftBodyIntegration.cpp | 13 | 0 | 0 | OK |
| Logging | 10 | TestSoftBodyLogging.cpp | 10 | 0 | 0 | OK |
| Regression | 11 | TestSoftBodyRegression.cpp | 0 | 0 | 11 | GOOD (regression) |
| Stress | 10 | TestSoftBodyStress.cpp | 0 | 10 | 0 | GOOD (stress) |

**SoftBody2D totals: 10 files, 117 tests** | Gaps: Golden simulation traces, determinism, conservation laws

---

## DiaRig2D

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Invariant | Determinism | Integration | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-----------|-------------|-------------|-------|
| Bone | 4 | Rig/TestBone.cpp | 4 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Skeleton | 9 | Rig/TestSkeleton.cpp | 9 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Pose / FK | 7 | Rig/TestPose.cpp | 7 | 0 | 0 | 0 | 0 | 0 | GOOD |
| BlendPoses | 7 | Rig/TestBlendPoses.cpp | 7 | 0 | 0 | 0 | 0 | 0 | GOOD |
| SkeletonJson | 10 | Rig/TestSkeletonJson.cpp | 10 | 0 | 0 | 0 | 0 | 0 | GOOD |
| SkeletonComponent | 7 | Rig/TestSkeletonComponent.cpp | 7 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Golden/FK math | 8 | Rig2D/TestRig2DGolden.cpp | 0 | 0 | 8 | 0 | 0 | 0 | GOOD — hand-calc FK, scale, angle wrap |
| Invariant/Property | 6 | Rig2D/TestRig2DInvariant.cpp | 0 | 0 | 0 | 6 | 0 | 0 | GOOD — t=0/1 identity, 500-iter random |
| Stress | 7 | Rig2D/TestRig2DStress.cpp | 0 | 7 | 0 | 0 | 0 | 0 | GOOD — 128 bones, 1000 iters, extreme values |
| Boundary | 10 | Rig2D/TestRig2DBoundary.cpp | 0 | 10 | 0 | 0 | 0 | 0 | GOOD — death tests, near-zero scale, max metadata, GetRequiredBoneIndex |
| Determinism | 3 | Rig2D/TestRig2DDeterminism.cpp | 0 | 0 | 0 | 0 | 3 | 0 | GOOD — bit-identical FK+blend across runs |
| Integration | 4 | Rig2D/TestRig2DIntegration.cpp | 0 | 0 | 0 | 0 | 0 | 4 | GOOD — JSON→FK parity, full pipeline, modify-reset cycle |

**Rig2D totals: 12 files, 82 tests** | GOOD — all test types covered; no conservation law tests (N/A for FK/blend)

---

## DiaIK2D

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Invariant | Determinism | Integration | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-----------|-------------|-------------|-------|
| IKChainDef / structs | 4 | IK/TestIKChainDef.cpp | 4 | 0 | 0 | 0 | 0 | 0 | GOOD |
| IKSolver registration | 5 | IK/TestIKSolverRegistration.cpp | 5 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Two-Bone Solver | 6 | IK/TestTwoBoneSolver.cpp | 6 | 0 | 0 | 0 | 0 | 0 | GOOD |
| FABRIK Solver | 5 | IK/TestFABRIKSolver.cpp | 5 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Look-At Constraint | 8 | IK/TestLookAtConstraint.cpp | 8 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Chained Solves | 2 | IK/TestIKChainedSolves.cpp | 2 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Logging | 3 | IK/TestIKLogging.cpp | 3 | 0 | 0 | 0 | 0 | 0 | GOOD |
| Golden-value | 9 | IK/TestIKGolden.cpp | 0 | 0 | 9 | 0 | 0 | 0 | GOOD — law of cosines, reach weight blend, look-at directions |
| Invariant/Property | 7 | IK/TestIKInvariant.cpp | 0 | 0 | 0 | 7 | 0 | 0 | GOOD — weight=0/1 identity, idempotent FABRIK, 500-iter random |
| Stress | 6 | IK/TestIKStress.cpp | 0 | 6 | 0 | 0 | 0 | 0 | GOOD — 32 chains, 128-bone, 1000 solve loops, extreme targets |
| Boundary | 11 | IK/TestIKBoundary.cpp | 0 | 11 | 0 | 0 | 0 | 0 | GOOD — DIA_ASSERT death tests, inverted limits, exact reach limit |
| Determinism | 3 | IK/TestIKDeterminism.cpp | 0 | 0 | 0 | 0 | 3 | 0 | GOOD — bit-identical across two runs for all three solvers |
| Integration | 4 | IK/TestIKIntegration.cpp | 0 | 0 | 0 | 0 | 0 | 4 | GOOD — FK→IK→FK pipeline, translated root, sequential solves, blend interop |

**IK2D totals: 13 files, 73 tests** | GOOD — all test types covered; no conservation law tests (N/A for IK math)

---

## DiaStateMachine

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| FlatStateMachine | 21 | TestFlatStateMachine.cpp, TestStateMachineExtended.cpp, TestStateMachineGaps.cpp | 19 | 2 (stress) | 0 | GOOD — self-transition, guards, listener, wildcard, per-frame counter reset |
| HierarchicalStateMachine | 16 | TestHierarchicalStateMachine.cpp, TestStateMachineExtended.cpp, TestStateMachineGaps.cpp | 16 | 0 | 0 | GOOD — LCA, history, enter/exit order, 3-level cross-branch, active path |
| PushdownAutomaton | 15 | TestPushdownAutomaton.cpp, TestStateMachineExtended.cpp | 15 | 0 | 0 | GOOD — push/pop, pause/resume, listener, stack inspection |
| CallbackRegistry | 10 | TestCallbackRegistry.cpp | 10 | 0 | 0 | GOOD — register/find/has for all 3 types |
| StateMachineComponent | 5 | TestCallbackRegistry.cpp | 5 | 0 | 0 | GOOD — component ID, attach/get, typed cast |
| StateMachineTracer | 8 | TestStateMachineTracer.cpp, TestStateMachineGaps.cpp | 6 | 2 (rate-limiter boundary) | 0 | GOOD — verbosity, enable/disable, rate limit, reset, kFull no-crash |
| StateMachineDefinition | 3 | TestStateMachineGaps.cpp | 3 | 0 | 0 | GOOD — Clone independent copy, IsValid |
| TransitionRecorder | 10 | TestStateMachineGaps.cpp | 10 | 0 | 0 | GOOD — all 5 callback types, sequence order, clear, FSM integration |
| IStateMachineInspectable | covered via above | — | — | — | — | Interface tested through all concrete machines |

**StateMachine totals: 7 files, 88 tests** | JSON data-driven loading intentionally excluded (consumers own parsing)

---

## DiaApplication

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| BasicApplication | 3 | TestBasicApplication.cpp | 3 | 0 | 0 | LOW |
| Lifecycle | 8 | TestApplicationLifecycle.cpp | 8 | 0 | 0 | OK |
| Loader | 8 | TestApplicationLoader.cpp | 8 | 0 | 0 | OK |
| TypeRegistry | 21 | TestApplicationTypeRegistry.cpp | 21 | 0 | 0 | GOOD |
| CluicheManifests | 27 | TestCluicheManifests.cpp | 27 | 0 | 0 | GOOD — includes AC7 composed-import integration test |
| HotReload | 6 | TestHotReload.cpp | 6 | 0 | 0 | OK |
| ManifestLoader | 24 | TestManifestLoader.cpp | 24 | 0 | 0 | GOOD |
| ManifestImports | 16 | TestManifestImports.cpp | 16 | 0 | 0 | GOOD — AC1-AC9, duplicate reject, relative path, diamond-dedup, deep nesting |
| MessageBus | 13 | TestMessageBus.cpp | 13 | 0 | 0 | OK |
| MetricsCollector | 9 | TestMetricsCollector.cpp | 9 | 0 | 0 | OK |
| Introspector | 10 | TestApplicationIntrospector.cpp | 10 | 0 | 0 | OK |
| ModuleRef | 7 | TestModuleRef.cpp | 7 | 0 | 0 | OK |
| PhaseTransition | 10 | TestPhaseTransition.cpp | 10 | 0 | 0 | OK |
| StateObject | **0** | — | 0 | 0 | 0 | **ZERO** — base class for Module/Phase/PU |

**Application totals: 13 files, 164 tests** | Gaps: StateObject (zero); module lifecycle integration

---

## DiaGraphics

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Misc/PrimitiveType | 12 | TestPrimitiveType.cpp | 12 | 0 | 0 | OK |
| Misc/RenderStates | 16 | TestRenderStates.cpp | 16 | 0 | 0 | OK |
| Misc/RGBA | 5 | TestRGBA.cpp | 5 | 0 | 0 | LOW |
| Misc/Transform | 21 | TestTransform.cpp | 21 | 0 | 0 | GOOD |
| Misc/Vertex | 8 | TestVertex.cpp | 8 | 0 | 0 | OK |
| Frame/FrameData | **0** | — | 0 | 0 | 0 | **ZERO** — FrameData, EntityFrameData, DebugFrameData, UIFrameData |
| Frame/Visitors | **0** | — | 0 | 0 | 0 | **ZERO** — EntityFrameDataVisitor, DebugFrameDataVisitor |
| Interface/ICanvas | **0** | — | 0 | 0 | 0 | **ZERO** — tested only via FakeCanvas fixture |

**Graphics totals: 5 files, 62 tests** | Gaps: FrameData types, visitors, ICanvas (all zero)

---

## DiaInput

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| ActionContext | 23 | TestActionContext.cpp | 23 | 0 | 0 | GOOD |
| ActionMap | 18 | TestActionMap.cpp | 18 | 0 | 0 | OK |
| InputProfile | 13 | TestInputProfile.cpp | 13 | 0 | 0 | OK |
| InputRecorder | 21 | TestInputRecorder.cpp | 21 | 0 | 0 | GOOD |
| InputState | 15 | TestInputState.cpp | 15 | 0 | 0 | OK |
| LegacyEventConverter | 23 | TestLegacyEventConverter.cpp | 23 | 0 | 0 | GOOD |
| ModernEvents | 29 | TestModernEvents.cpp | 29 | 0 | 0 | GOOD |

**Input totals: 7 files, 143 tests** | OK coverage

---

## DiaEditor

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| CommandHistory | 5 | TestCommandHistory.cpp | 5 | 0 | 0 | LOW |
| DockingLayout | 10 | TestDockingLayout.cpp | 10 | 0 | 0 | OK |
| EditorApplication | 10 | TestEditorApplication.cpp | 10 | 0 | 0 | OK |
| EditorLifecycle | 2 | TestEditorApplicationLifecycle.cpp | 2 | 0 | 0 | LOW |
| EditorModel | 4 | TestEditorModel.cpp | 4 | 0 | 0 | LOW |
| PluginPipeline | 4 | TestEditorPluginPipeline.cpp | 4 | 0 | 0 | LOW |
| PluginRegistry | 6 | TestEditorPluginRegistry.cpp | 6 | 0 | 0 | OK |
| ToolbarItem | 4 | TestEditorToolbarItem.cpp | 4 | 0 | 0 | LOW |
| GameConnectionController | 20 | TestGameConnectionController.cpp | 20 | 0 | 0 | GOOD |
| GameConnectionMessages | 15 | TestGameConnectionControllerMessages.cpp | 15 | 0 | 0 | OK |
| GameConnectionLifecycle | 10 | TestGameConnectionLifecycle.cpp | 10 | 0 | 0 | OK |
| GameConnectionManager | 17 | TestGameConnectionManager.cpp | 17 | 0 | 0 | OK |
| GameConnectionIntegration | 3 | TestGameConnectionManagerIntegration.cpp | 3 | 0 | 0 | LOW |
| WebUIBridge | 3 | TestWebUIBridge.cpp | 3 | 0 | 0 | LOW |

**Editor totals: 14 files, 113 tests** | Several LOW components but acceptable for editor tooling

---

## DiaLogger

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| AssertSinkBridge | 11 | TestAssertSinkBridge.cpp | 11 | 0 | 0 | OK |
| ISink | 18 | TestISink.cpp | 18 | 0 | 0 | GOOD |
| LogLevel | 19 | TestLogLevel.cpp | 19 | 0 | 0 | GOOD |
| Logger | 13 | TestLogger.cpp | 13 | 0 | 0 | OK |
| ThreadLogBuffer | 9 | TestThreadLogBuffer.cpp | 9 | 0 | 0 | OK |

**Logger totals: 5 files, 70 tests** | OK coverage

---

## DiaPython

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| ErrorHandling | 15 | TestErrorHandling.cpp | 15 | 0 | 0 | OK |
| Integration | 10 | TestIntegration.cpp | 10 | 0 | 0 | OK |
| Lifecycle | 13 | TestLifecycle.cpp | 13 | 0 | 0 | OK |
| Module | 22 | TestModule.cpp | 22 | 0 | 0 | GOOD |
| ScriptExecution | 30 | TestScriptExecution.cpp | 30 | 0 | 0 | GOOD |
| TypeConversion | 26 | TestTypeConversion.cpp | 26 | 0 | 0 | GOOD |

**Python totals: 6 files, 116 tests** | GOOD coverage

---

## DiaAPI / DiaCLI

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| ArgumentParser | 22 | CLI/TestArgumentParser.cpp | 22 | 0 | 0 | GOOD |
| CommandRegistry | 17 | CLI/TestCommandRegistry.cpp | 17 | 0 | 0 | OK |
| EventSystem | 9 | CLI/TestEventSystem.cpp | 9 | 0 | 0 | OK |
| HelpSystem | 13 | CLI/TestHelpSystem.cpp | 13 | 0 | 0 | OK |
| PythonBindings | 8 | CLI/TestPythonBindings.cpp | 8 | 0 | 0 | OK |
| DiaCLI E2E | 6 | DiaCLI/TestDiaCLI.cpp | 6 | 0 | 0 | LOW |

**CLI totals: 6 files, 75 tests** | OK coverage

---

## DiaWebSocket

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Client | 10 | TestWebSocketClient.cpp | 10 | 0 | 0 | OK |
| ClientState | 7 | TestWebSocketClientState.cpp | 7 | 0 | 0 | OK |
| Error | 7 | TestWebSocketError.cpp | 7 | 0 | 0 | OK |
| Integration | 12 | TestWebSocketIntegration.cpp | 12 | 0 | 0 | OK |
| Server | 13 | TestWebSocketServer.cpp | 13 | 0 | 0 | OK |
| Transport | 10 | TestWebSocketTransport.cpp | 10 | 0 | 0 | OK |

**WebSocket totals: 6 files, 59 tests** | OK coverage

---

## DiaDebugProtocol / DiaDebugServer

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| Proto | 20 | TestDebugProtocolProto.cpp | 20 | 0 | 0 | OK |
| WireCompat | 15 | TestDebugProtocolWireCompat.cpp | 15 | 0 | 0 | OK |
| CommandDispatcher | 10 | TestCommandDispatcher.cpp | 10 | 0 | 0 | OK |
| ServerProtocol | 19 | TestDebugServerProtocol.cpp | 19 | 0 | 0 | OK |
| StateSerializer | 9 | TestStateSerializer.cpp | 9 | 0 | 0 | OK |
| SubscriptionManager | 13 | TestSubscriptionManager.cpp | 13 | 0 | 0 | OK |

**Debug totals: 6 files, 86 tests** | OK coverage

---

## DiaProtobuf

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| ProtoStructConverter | 13 | Dia/TestProtoStructConverter.cpp | 13 | 0 | 0 | OK |
| ProtoJsonCodec | **0** | — | 0 | 0 | 0 | **ZERO** — ToJson/FromJson for protobuf messages |

**Protobuf totals: 1 file, 13 tests** | Gaps: ProtoJsonCodec (zero)

---

## DiaUI

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| BoundMethod | 17 | TestBoundMethod.cpp | 17 | 0 | 0 | OK |
| Page | 4 | TestPage.cpp | 4 | 0 | 0 | LOW |
| UIDataBuffer | 7 | TestUIDataBuffer.cpp | 7 | 0 | 0 | OK |

**UI totals: 3 files, 28 tests** | LOW overall

---

## DiaUICEF

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| MimeTypes | 10 | TestCEFMimeTypes.cpp | 10 | 0 | 0 | OK |
| PageManagement | 3 | TestCEFPageManagement.cpp | 3 | 0 | 0 | LOW |
| PixelConversion | 4 | TestCEFPixelConversion.cpp | 4 | 0 | 0 | LOW |
| URLParsing | 8 | TestCEFURLParsing.cpp | 8 | 0 | 0 | OK |
| JavaScriptBridge | **0** | — | 0 | 0 | 0 | **ZERO** |

**UICEF totals: 4 files, 25 tests** | LOW — JS bridge untested

---

## DiaApplicationEditor

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| ManifestSerializer | 20 | TestManifestSerializer.cpp | 20 | 0 | 0 | GOOD |

**ApplicationEditor totals: 1 file, 20 tests** | OK for small module

---

## DiaPipelineEditor

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| BuildTrigger | 10 | TestBuildTrigger.cpp | 10 | 0 | 0 | OK |
| PluginBridge | 4 | TestPipelineEditorPluginBridge.cpp | 4 | 0 | 0 | LOW |
| LogTailer | 17 | TestPipelineLogTailer.cpp | 17 | 0 | 0 | OK |
| TargetParser | 7 | TestPipelineTargetParser.cpp | 7 | 0 | 0 | OK |
| RunHistoryStore | 13 | TestRunHistoryStore.cpp | 13 | 0 | 0 | OK |

**PipelineEditor totals: 5 files, 51 tests** | OK coverage

---

## DiaAssetCatalogueEditor

| Component | Tests | Files | Unit | Stress/Boundary | Golden/Regression | Notes |
|-----------|-------|-------|------|-----------------|-------------------|-------|
| UpdateRecordCommand | 9 | TestDiaAssetCatalogueEditorExhaustive.cpp | 9 | 0 | 0 | OK |
| ApplyRulesCommand | 7 | TestDiaAssetCatalogueEditorExhaustive.cpp | 7 | 0 | 0 | OK |
| CatalogueRulesEngine.GetRule | 8 | TestDiaAssetCatalogueEditorExhaustive.cpp | 8 | 0 | 0 | OK |
| ManualOverrideFlags (AssetRecord) | 9 | TestDiaAssetCatalogueEditorExhaustive.cpp | 9 | 0 | 0 | OK |
| LoadManifestCommand | ~10 | TestDiaAssetCatalogueEditorExhaustive.cpp | ~10 | 0 | 0 | OK |
| SaveManifestCommand | ~8 | TestDiaAssetCatalogueEditorExhaustive.cpp | ~8 | 0 | 0 | OK |
| AddAssetCommand | ~8 | TestDiaAssetCatalogueEditorExhaustive.cpp | ~8 | 0 | 0 | OK |
| DeleteAssetCommand | ~6 | TestDiaAssetCatalogueEditorExhaustive.cpp | ~6 | 0 | 0 | OK |
| CommandHistory (undo/redo) | ~8 | TestDiaAssetCatalogueEditorExhaustive.cpp | ~8 | 0 | 0 | OK |

**AssetCatalogueEditor totals: 1 file, ~73 tests** | All 8 features + manual override system covered

---

## Integration Tests (cross-module)

| Component | Tests | Files | Type | Notes |
|-----------|-------|-------|------|-------|
| DebugProtocol E2E | 15 | TestDebugProtocolEndToEnd.cpp | Integration | GOOD — round-trip serialization |
| FrameStream Pipeline | 9 | TestFrameStreamPipeline.cpp | Integration | OK |
| Input Pipeline | 9 | TestInputPipeline.cpp | Integration | OK |
| Manifest RoundTrip | 24 | TestManifestRoundTrip.cpp | Integration | GOOD |
| TypeRegistry Seeding | 12 | TestTypeRegistrySeeding.cpp | Integration | OK |
| WebUIBridge Dispatch | 17 | TestWebUIBridgeDispatch.cpp | Integration | OK |

**Integration totals: 6 files, 86 tests** | Gaps: Physics-Geometry, Module lifecycle, Input-Application missing

---

## Zero-Test Modules

| Module | Headers | Reason | Priority |
|--------|---------|--------|----------|
| DiaWindow | 6 .h, 3 .cpp | Platform abstraction — testable Settings/Style subset | P2 |
| DiaSFML | 7 .h, 7 .cpp | SFML backend — skipped per user decision | Deferred |
| DiaUIUltralight | 1 .h, 1 .cpp | Minimal UI backend alternative | P3 |

---

## Test Fixtures & Shared Infrastructure

| Fixture | Location | Used By |
|---------|----------|---------|
| FakeInputSource | Fixtures/FakeInputSource.h | Input, Integration |
| FakeCanvas | Fixtures/FakeCanvas.h | Graphics |
| ProcessingUnitFixture | Fixtures/ProcessingUnitFixture.h | Application |
| RobustnessFixture (inline) | RigidBody2D/TestRobustness.cpp | RigidBody2D |
| ConstraintFixture (inline) | RigidBody2D/TestConstraints.cpp | RigidBody2D |
| QueryFixture (inline) | RigidBody2D/TestSpatialQueries.cpp | RigidBody2D |
| LayerFixture (inline) | RigidBody2D/TestCollisionLayers.cpp | RigidBody2D |
| IntegrationFixture (inline) | RigidBody2D/TestPhysicsIntegration.cpp | RigidBody2D |
| WorldFixture (inline) | RigidBody2D/TestCollisionDetection.cpp | RigidBody2D |
| TriggerFixture (inline) | RigidBody2D/TestTriggerVolumes.cpp | RigidBody2D |

**Note:** 7 inline fixtures in RigidBody2D duplicate world setup — candidates for `Dia/DiaRigidBody2D/Testing/` consolidation.

---

## Agentic Test Type Coverage

| Type | Present? | Where | Gaps |
|------|----------|-------|------|
| Unit | Yes (2,700+) | All modules | Graph, Observer, FilePath, Allocator, Easing, Interpolation, FrameData, BVH, Quadtree, StateObject, ProtoJsonCodec |
| Boundary (EXPECT_DEATH) | Yes (149) | Core/Containers (88), Maths (32), Time (4), Architecture (4), IK2D (11), Rig2D (10) | Missing in physics (RigidBody2D), geometry, application |
| Stress | Partial | SoftBody2D (10), RigidBody2D Robustness (12), Rig2D (7), IK2D (6) | Missing for containers at scale, threading, RigidBody2D 100+ bodies |
| Golden-value | Partial | Rig2D (8), IK2D (9) | Needed for maths (matrices, easing), geometry intersections, physics traces |
| Determinism | Partial | Rig2D (3), IK2D (3) | Needed for RigidBody2D, SoftBody2D |
| Conservation law | No | — | Needed for physics (momentum, energy) |
| Property/invariant | Partial | Rig2D (6), IK2D (7) | Needed for maths (commutativity, normalization), containers (size invariants) |
| Parameterized (TYPED_TEST) | No | — | BitArray (4 types), containers, protocol messages |
| Integration | Partial (90+) | Integration/ directory, Rig2D (4), IK2D (4) | Missing Physics-Geometry, Module lifecycle, Input-Application |
