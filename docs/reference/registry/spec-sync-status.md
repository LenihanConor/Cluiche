# Spec Sync Status

_Refresh by running `dia check spec-sync`. Commit the result to keep it visible in the site._

Symbols are extracted from each spec's **Public Interfaces** section (class/struct names only).
A symbol is ✅ if it appears anywhere under `Dia/` headers; ⚠️ if absent.

| Spec | Spec Status | Sync | Symbols Found | Missing |
|------|-------------|------|---------------|---------|
| cluicheeditor | Unknown | — | — | no Public Interfaces |
| applicationflow | Unknown | ✅ | EditorModelModule | — |
| diachatplugin | Done | — | — | no Public Interfaces |
| diaeditorapi | Unknown | — | — | no Public Interfaces |
| pluginbrowser | Unknown | — | — | no Public Interfaces |
| rendertestplugin | Unknown | — | — | no Public Interfaces |
| cluichetest | Unknown | — | — | no Public Interfaces |
| applicationflow | Unknown | — | — | no Public Interfaces |
| asset-pipeline | Unknown | — | — | no Public Interfaces |
| async-asset-loading | Unknown | — | — | no Public Interfaces |
| cluichetestscenarios | Unknown | — | — | no Public Interfaces |
| teststages | Unknown | ❌ | — | RigidBody2DTestModule |
| dia | Unknown | — | — | no Public Interfaces |
| asset-system-overview | Unknown | — | — | no Public Interfaces |
| diaaibroadcast | Unknown | ✅ | Callout, CalloutHandle, QueryFilter, CalloutRegistry | — |
| diaaibudget | Unknown | ⚠️ | IAIBudgetedSystem, AIBudgetScheduler, AIBudgetModule | PathfindingBudgetAdapter |
| diaaibudgetvisualdebugger | Done | ✅ | AIBudgetScheduler, AIBudgetResult, AIBudgetVisualDebugger | — |
| diaaiinspector | Done | ⚠️ | DiaAIInspectorPlugin, AIBudgetController, UtilityAIController, RulesController, HTNController | AIBudgetInspectorSource, UtilityAIInspectorSource, RulesInspectorSource, HTNInspectorSource |
| diaanimation2d | Done | ✅ | SpringNodeDef, SpringChainDef, SpringChain, Keyframe, KeyframeTrack, AnimClipDef, AnimClip, PlaybackMode, AnimClipPlayer, BoneMask, PoseLayer, PoseBlendStack, AnimationEvaluator | — |
| diaapi | Unknown | ✅ | CommandArgs | — |
| diaapplication | Unknown | ⚠️ | ProcessingUnit, Phase, Module, MessageBus, Message, ErrorCode, ErrorInfo | StateObject, StateEnum, RunningEnum, HotReloadManager, ReloadResult |
| diaapplicationeditor | Unknown | ⚠️ | ValidationResult | DiaApplicationEditor, ManifestEditorData, RuntimeState, PhaseTransition, ValidationError |
| diaapplicationflow | Unknown | ✅ | Application, ProcessingUnit, StartResult, StopResult, Module, ModuleRef, StreamWriter, StreamReader, EventStreamWriter, EventStreamReader, MyModule, ModuleStateInfo, IApplicationInspectable | — |
| diaapplicationfloweditor | Unknown | ✅ | DiaApplicationFlowEditorPlugin | — |
| diaapplicationflowinspector | Unknown | ✅ | DiaApplicationFlowInspectorPlugin | — |
| diaarchitecture | Approved | — | — | no Public Interfaces |
| diaassetcatalogue | Unknown | — | — | no Public Interfaces |
| diaassetcatalogueeditor | Unknown | ✅ | DiaAssetCatalogueEditor, AssetTypeEditorRegistry | — |
| diaassetpipeline | Unknown | — | — | no Public Interfaces |
| diaassetruntime | Unknown | — | — | no Public Interfaces |
| diaassetruntimeinspector | Unknown | ✅ | DiaAssetRuntimeInspector | — |
| diaassetruntimevisualdebugger | Approved | — | — | no Public Interfaces |
| diaattribute | Approved | ❌ | — | AttributeDefinition, AttributeSchema, ModifierOperation, AttributeModifier, ModifierTag, AttributeSet, AttributeSetComponent, AttributeChangedEvent, IAttributeObserver, AttributeObserverSubject, AttributeAccessorBridge |
| diaautomation | Unknown | ✅ | CheckpointResult, AutomationService, CheckpointEntry, PauseResumeEntry | — |
| diabehaviourtree | Done | ✅ | NodeResult, ActionRegistry, BehaviourTreeAsset, IBehaviourTreeEventListener, DecoratorContext, IDecoratorNode, DecoratorRegistry, BehaviourTreeComponent, BehaviourTreeSystem, SpyAction | — |
| diabehaviourtreevisualdebugger | Approved | ✅ | BehaviourTreeComponent, BehaviourTreeVisualDebugger, NodeVisit | — |
| diabgfx3d | Unknown | — | — | no Public Interfaces |
| diablackboard | Unknown | — | — | no Public Interfaces |
| diablackboardinspector | Unknown | — | — | no Public Interfaces |
| diablackboardvisualdebugger | Done | ✅ | Blackboard, BlackboardVisualDebugger, FormatterEntry | — |
| diablueprinteditor | Unknown | ✅ | DiaEntityTemplateEditorPlugin | — |
| diabugdetection | Approved | — | — | no Public Interfaces |
| diacamera2d | Done | ✅ | Camera2D, ViewportTransform, CameraRegistry2D, ICameraBehaviour, CameraBehaviourRegistry | — |
| diacamera3d | Unknown | — | — | no Public Interfaces |
| diacli | Unknown | — | — | no Public Interfaces |
| diacondition | Unknown | ✅ | IConditionContext, ConditionRegistry, ConditionOp, ConditionExpr, MockConditionContext | — |
| diacore | Unknown | — | — | no Public Interfaces |
| diadata | Unknown | — | — | no Public Interfaces |
| diadebugdomain | Done | ✅ | IDebugDomain, DiaDebugDomainRegistry | — |
| diadebugprotocol | Unknown | ⚠️ | MessageType, HandshakeRequest, HandshakeResponse | MessageHeader, SubscribeMessage, CoreMetricsPayload, DataUpdateMessage, EventMessage, CommandRequestMessage, CommandResponseMessage, ErrorMessage |
| diadebugserver | Unknown | ⚠️ | CoreMetrics, StateSerializer, Subscription, CommandDispatcher | DebugServerModule, SubscriptionManager |
| diaeconomy | Done | ✅ | ResourceDefinition, EconomySchema, EconomyInstance, TransactionResult, EconomySystem, PoolChangedEvent, TransactionClampedEvent, TransferCompletedEvent, EconomyObserverSubject, IEconomyObserver | — |
| diaeconomyinspector | Unknown | ⚠️ | DiaEconomyInspectorPlugin, EconomyInstancesController, EconomyModifiersController, EconomyEventsController, EconomySchemaController | EconomyInstancesSource, EconomyModifiersSource, EconomyEventsSource, EconomySchemaSource |
| diaeditor | Unknown | ⚠️ | LayoutMode, IEditorPlugin, IEditorPluginFactory, EditorPluginRegistry, ClassName, EditorModel, EditorViewController, EditorView, EditorManifestLoader, PluginEntry, IEditorCommand, CommandHistory, CommandDispatcher, GameConnectionManager, State, WebUIBridge, DockingLayout | ManifestEditorData, ConnectionInfo, ConnectionResult, WebSocketClient |
| diaeditorui | Unknown | — | — | no Public Interfaces |
| diaentity | Unknown | — | — | no Public Interfaces |
| diaentityinspector | Unknown | — | — | no Public Interfaces |
| diaentityspatial | Done | — | — | no Public Interfaces |
| diaentityspatialvisualdebugger | Done | ✅ | EntitySpatialGridOverlay, EntityOverlayConfig, EntitySpatialEntityOverlay, QueryDescriptor, Shape, EntitySpatialQueryOverlay | — |
| diaentityspawner | Done | — | — | no Public Interfaces |
| diaentityvisualdebugger | Approved | — | — | no Public Interfaces |
| diaenv | Unknown | — | — | no Public Interfaces |
| diaflowfield | Done | ✅ | FlowCell, FlowField, FlowFieldCache | — |
| diaflowfieldvisualdebugger | Approved | ✅ | FlowFieldVisualDebugger | — |
| diagame | Unknown | — | — | no Public Interfaces |
| diageometry2d | Unknown | — | — | no Public Interfaces |
| diageometry3d | Unknown | — | — | no Public Interfaces |
| diageometrybridge | Unknown | ❌ | — | Axis2D |
| diagraphics | Unknown | — | — | no Public Interfaces |
| diagraphics3d | Unknown | — | — | no Public Interfaces |
| diagridvisibility | Done | ✅ | VisibilityState, IVisibilityChangeObserver, GridVisibilitySystem | — |
| diagridvisibilityvisualdebugger | Done | ✅ | GridVisibilityDebugDomain | — |
| diahtn | Done | ✅ | TaskResult, OperatorRegistry, HTNDomain, HTNTask, HTNPlan, HTNPlanner, HTNPlannerComponent, MockHTNContext | — |
| diahtnvisualdebugger | Done | ✅ | HTNPlannerComponent, HTNVisualDebugger | — |
| diaik2d | Unknown | — | — | no Public Interfaces |
| dialighting2d | Done | ✅ | PointLight2D, LightRegistry2D | — |
| dialighting3d | Unknown | — | — | no Public Interfaces |
| dialighting3dvisualdebugger | Draft | — | — | no Public Interfaces |
| dialogger | Unknown | — | — | no Public Interfaces |
| diamailbox | Unknown | ✅ | Address, SubscriberId, OverflowPolicy, SubscriptionHandle, Mailbox, Visitor, IMailboxRouter | — |
| diamailboxvisualdebugger | Done | ✅ | Mailbox, MailboxVisualDebugger | — |
| diamaths | Unknown | ✅ | Angle, FloatMaths, HalfFloat, Random, Vector2D, Vector3D, Vector4D, VectorHalf2D, Matrix22, Matrix33, Transform2D, Matrix44, Matrix34, Quaternion, Transform3D | — |
| diamesh3d | Unknown | — | — | no Public Interfaces |
| diamesh3dvisualdebugger | Draft | — | — | no Public Interfaces |
| diamessagebus | Unknown | ✅ | BusSubscriptionHandle, IFlushAdapter, Bus, Pass, MessageBusModule, LedgerMessageEntry, LedgerSnapshot, PhysicsBusAdapter | — |
| diametrics | Unknown | — | — | no Public Interfaces |
| diaobjective | Done | ✅ | IObjectiveObserver, RewardEntry, ObjectiveClassification, ObjectiveState, ObjectiveDef, ObjectiveSet, ObjectiveSetComponent, CapturingObserver, Event, Type | — |
| diaobservation | Unknown | — | — | no Public Interfaces |
| diaorder | Unknown | — | — | no Public Interfaces |
| diapathfinding | Unknown | ✅ | CellCoord, SquareConnectivity, SquarePathGrid, HexPathGrid, IPathCostProvider, FlatCostProvider, PathResult, PathRequest, IPathResultObserver, PathfindingSystem | — |
| diapathfindingvisualdebugger | Done | ⚠️ | PathfindingVisualDebugger | PathGrid |
| diapicking | Unknown | — | — | no Public Interfaces |
| diapipeline | Unknown | — | — | no Public Interfaces |
| diapipelineeditor | Unknown | ✅ | PipelineEvent, RunSummary, PipelineLogTailer | — |
| diapython | Unknown | ✅ | Module, PythonObject, PythonArgs | — |
| diareflect | Unknown | — | — | no Public Interfaces |
| diarendertest | Done | ✅ | FrameCaptureWriter, RegionStat, FrameDiffResult, FrameDiff, CaptureMetadata, CaptureReportWriter, RuleResult, EvaluationResult, ExpectationEvaluator, MetricEntry, MetricsWriter | — |
| diarig2d | Unknown | — | — | no Public Interfaces |
| diarigidbody2d | Unknown | ⚠️ | BodyType, PointBodyDef, PointBody2D, IConstraint, RigidBodyDef, RigidBody2D, WorldDef, PhysicsWorld, CollisionEventType, CollisionEvent, ISpatialStructure | ContactPoint |
| diarigidbody2dvisualdebugger | Draft | — | — | no Public Interfaces |
| diarules | Unknown | ✅ | RuleActionRegistry, RuleDef, RuleSet, RuleSetComponent | — |
| diarulesvisualdebugger | Done | ✅ | RuleSetComponent, RulesVisualDebugger | — |
| diasavegame | Done | ✅ | ISaveable, SaveRegistry, SaveManager, SaveConfig, SaveContext, LoadContext, OnSaveStarted, OnSaveCompleted, OnLoadStarted, OnLoadCompleted, OnMigrationApplied | — |
| diascalarfield | Done | ✅ | CellIndex, SquareConnectivity, SquareFieldTopology, HexFieldTopology, UniformDecayParams, UniformDecayPolicy, FalloffCurve, DiaScalarField, WeightedField, OverlayColourMap, ScalarFieldOverlay | — |
| diascalarfieldinspector | Done | ✅ | DiaScalarFieldInspectorPlugin | — |
| diascalarfieldvisualdebugger | Done | ✅ | OverlayColourMap, ScalarFieldHeatmapOverlay, ScalarFieldGradientOverlay | — |
| diascene2d | Unknown | — | — | no Public Interfaces |
| diascene2dvisualdebugger | Approved | — | — | no Public Interfaces |
| diascene3d | Unknown | — | — | no Public Interfaces |
| diasceneeditor | Unknown | ✅ | DiaSceneEditorPlugin | — |
| diasensor | Done | — | — | no Public Interfaces |
| diaserializer | Unknown | ✅ | MetadataValue, MetadataEntry, SerializeResult, ISerializer | — |
| diasimtime | Unknown | ✅ | SimTimeContext, RenderTimeContext, MainTimeContext, SimModule, RenderModule, MainModule, TimeServer, SimTimeDomain, SimTimeDomainRegistry, SimTimeScheduler, SimTimePriority, ISimTimeBudgetedSystem, SimTimeTier, SimTimePolicy, SimTimeState, DiaSimTimeModule, SimTimeSaveState | — |
| diasoftbody2d | Unknown | — | — | no Public Interfaces |
| diastatemachine | Unknown | ⚠️ | StateInfo, TransitionInfo, TransitionRecord, IStateMachineInspectable, FlatStateMachine, HierarchicalStateMachine, PushdownAutomaton, CallbackRegistry, StateMachineBuilder, HierarchicalStateMachineBuilder, StateDef, TransitionDef, StateMachineDefinition, HierarchicalStateMachineDefinition, StateMachineComponent, StateMachineTracer | TraceVerbosity |
| diastatemachinevisualdebugger | Done | ✅ | StateMachineVisualDebugger, CachedGuardResult | — |
| diasteering | Done | ⚠️ | SteeringAgent, SteeringPipeline, SteeringSystem | SteeringGroup |
| diasteeringvisualdebugger | Done | ✅ | SteeringSystem, SteeringVisualDebugger | — |
| diatest | Unknown | — | — | no Public Interfaces |
| diatestharness | Unknown | — | — | no Public Interfaces |
| diathreading | Unknown | — | — | no Public Interfaces |
| diatriggerscript | Done | ✅ | TriggerType, SpatialParams, TemporalParams, StateParams, CountParams, ActionDef, TriggerDef, ActionContext, ITriggerActionHandler, TriggerActionRegistry, TriggerScriptModule, TriggerFiredEvent, MockActionHandler, Call | — |
| diauicef | Unknown | — | — | no Public Interfaces |
| diauiultralight | Unknown | — | — | no Public Interfaces |
| diautilityai | Done | ✅ | CurveShape, ResponseCurve, ScorerDef, ActionDef, GroupConsiderationContext, UtilitySelection, UtilitySet, UtilitySetComponent, UtilityScoreDrawer | — |
| diautilityaivisualdebugger | Done | ✅ | UtilityAIDebugDomain | — |
| diavisualdebugger | Unknown | — | — | no Public Interfaces |
| diawebsocket | Unknown | ⚠️ | MessageType, Message, Server, ConnectionState, Client | QueuedMessage |
| render-backend | Unknown | — | — | no Public Interfaces |
| example-app | Unknown | — | — | no Public Interfaces |
| example-system | Unknown | — | — | no Public Interfaces |
| googletests | Unknown | — | — | no Public Interfaces |
| googletestspeed | Unknown | — | — | no Public Interfaces |
