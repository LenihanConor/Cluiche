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
| cluichetest | Unknown | — | — | no Public Interfaces |
| applicationflow | Unknown | — | — | no Public Interfaces |
| asset-pipeline | Unknown | — | — | no Public Interfaces |
| async-asset-loading | Unknown | — | — | no Public Interfaces |
| cluichetestscenarios | Unknown | — | — | no Public Interfaces |
| teststages | Unknown | ❌ | — | RigidBody2DTestModule |
| dia | Unknown | — | — | no Public Interfaces |
| asset-system-overview | Unknown | — | — | no Public Interfaces |
| diaanimation2d | Done | ✅ | SpringNodeDef, SpringChainDef, SpringChain, Keyframe, KeyframeTrack, AnimClipDef, AnimClip, PlaybackMode, AnimClipPlayer, BoneMask, PoseLayer, PoseBlendStack, AnimationEvaluator | — |
| diaapi | Unknown | ✅ | CommandArgs | — |
| diaapplication | Unknown | ⚠️ | ProcessingUnit, Phase, Module, Message, ErrorCode, ErrorInfo | StateObject, StateEnum, RunningEnum, MessageBus, HotReloadManager, ReloadResult |
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
| diaautomation | Unknown | ✅ | CheckpointResult, AutomationService, CheckpointEntry, PauseResumeEntry | — |
| diabgfx3d | Approved | ⚠️ | Canvas3D, ShaderProgram, MaterialDescriptor, MaterialRegistry, GpuMesh, MeshGpuCache, MeshRenderer | SkinnedMeshRenderer |
| diablueprinteditor | Unknown | ✅ | DiaEntityTemplateEditorPlugin | — |
| diabugdetection | Approved | — | — | no Public Interfaces |
| diacamera2d | Done | ✅ | Camera2D, ViewportTransform, CameraRegistry2D, ICameraBehaviour, CameraBehaviourRegistry | — |
| diacamera3d | Done | ✅ | PerspectiveParams, OrthoParams, ProjectionType, Camera3D, ViewportTransform3D, CameraRegistry3D, ICameraBehaviour3D, CameraBehaviourRegistry3D | — |
| diacli | Unknown | — | — | no Public Interfaces |
| diacore | Unknown | — | — | no Public Interfaces |
| diadata | Unknown | — | — | no Public Interfaces |
| diadebugprotocol | Unknown | ⚠️ | MessageType, HandshakeRequest, HandshakeResponse | MessageHeader, SubscribeMessage, CoreMetricsPayload, DataUpdateMessage, EventMessage, CommandRequestMessage, CommandResponseMessage, ErrorMessage |
| diadebugserver | Unknown | ⚠️ | CoreMetrics, StateSerializer, Subscription, CommandDispatcher | DebugServerModule, SubscriptionManager |
| diaeditor | Unknown | ⚠️ | LayoutMode, IEditorPlugin, IEditorPluginFactory, EditorPluginRegistry, ClassName, EditorModel, EditorViewController, EditorView, EditorManifestLoader, PluginEntry, IEditorCommand, CommandHistory, CommandDispatcher, GameConnectionManager, State, WebUIBridge, DockingLayout | ManifestEditorData, ConnectionInfo, ConnectionResult, WebSocketClient |
| diaeditorui | Unknown | — | — | no Public Interfaces |
| diaentity | Unknown | ⚠️ | EntityTag, Domain, TComponent, IComponent, FieldDesc, ComponentTypeDesc, ComponentRegistry, AddressKind, EntityRouter, ParentComponent, ChildBufferComponent, QueryView, Iterator, IEntityInspectable | DonkeyFeetComponent, FieldType |
| diaentityinspector | Unknown | ✅ | DiaEntityInspectorPlugin | — |
| diaenv | Unknown | — | — | no Public Interfaces |
| diagame | Unknown | ⚠️ | DiaGameConfig, DiaGameManifest, DiaStageManifest, JsonDiaGameSerializer, JsonDiaStageSerializer, DiaGameManifestLoader | GameFileComposer, GameLoader |
| diageometry2d | Unknown | ⚠️ | AARect, OORect, Circle, Line, Ray, Triangle, Arc, Capsule, IntersectionClassify, IntersectionTests, Transform, SpatialGrid, Quadtree, BVH | Ellipse, IntersectionPoint |
| diageometry3d | Unknown | ✅ | AABB, OOBB, Sphere, Capsule, Triangle, Cylinder, Ray, Plane, Frustum, IntersectionClassify, IntersectionTests, ISpatialStructure3D, SpatialGrid3D, Def | — |
| diageometrybridge | Unknown | ❌ | — | Axis2D |
| diagraphics | Approved | ✅ | FrameData, DebugFrameData, DebugFrameDataVisitor | — |
| diagraphics3d | Approved | ✅ | Camera3D, DirectionalLight, PointLight, Mesh3DDrawCommand, Mesh3DFrameData, FrameData3D | — |
| diaik2d | Done | ✅ | JointLimitDef, IKChainDef, PoleVector, IKSolver | — |
| dialighting2d | Done | ✅ | PointLight2D, LightRegistry2D | — |
| dialighting3d | Approved | ✅ | PointLight3D, DirectionalLight3D, SpotLight3D, AmbientLight3D, LightRegistry3D, ILightBehaviour3D, LightBehaviourRegistry3D | — |
| dialogger | Unknown | ✅ | LogLevel, LogEntry, ISink, Logger, DebugOutputSink | — |
| diamailbox | Unknown | ✅ | Address, SubscriberId, OverflowPolicy, SubscriptionHandle, Mailbox, Visitor, IMailboxRouter | — |
| diamaths | Unknown | ✅ | Angle, FloatMaths, HalfFloat, Random, Vector2D, Vector3D, Vector4D, VectorHalf2D, Matrix22, Matrix33, Transform2D, Matrix44, Matrix34, Quaternion, Transform3D | — |
| diamesh3d | Done | ✅ | Vertex3D, Submesh, Mesh3DAsset, State, Mesh3DAssetHandler | — |
| diametrics | Unknown | — | — | no Public Interfaces |
| diaobservation | Unknown | ⚠️ | SessionManager, SessionConfig, LogLevel, LogEntry, ISink, Logger, StdOutSink, DebugOutputSink, ObservationFileSink, ScopedZone, Profiler, ScopeRecord, MetricRegistry, Counter, Gauge, Histogram, HealthStatus, Health, IHealthReporter | TraceRegistry, Span |
| diapicking | Unknown | ✅ | PickResult, PickEvent, PickTrigger, PickLayer, PickAddress, PickRouter | — |
| diapipeline | Unknown | — | — | no Public Interfaces |
| diapipelineeditor | Unknown | ✅ | PipelineEvent, RunSummary, PipelineLogTailer | — |
| diapython | Unknown | ✅ | Module, PythonObject, PythonArgs | — |
| diareflect | Unknown | — | — | no Public Interfaces |
| diarig2d | Approved | ✅ | Bone, SkeletonDef, Skeleton, BoneTransform, Pose, SkeletonComponent | — |
| diarigidbody2d | Unknown | ⚠️ | BodyType, PointBodyDef, PointBody2D, IConstraint, RigidBodyDef, RigidBody2D, WorldDef, PhysicsWorld, CollisionEventType, CollisionEvent, ISpatialStructure | ContactPoint |
| diarigidbody2dvisualdebugger | Unknown | ❌ | — | DiaRigidBodyVisualDebugger |
| diascene2d | Approved | ✅ | LayerDef, CameraEntry, LightEntry, EntityInstance, Scene2D, LayerTable, SceneLoadContext, SceneLoadErrors, SceneLoader2D | — |
| diascene3d | Draft | ⚠️ | EntityInstance | SceneNodeType, SceneNode3D, CameraNode, LightNode, StaticMeshEntry, Scene3D, SceneGraph3D, SceneSubmitContext3D, SceneLoadContext3D, SceneLoadErrors3D, SceneLoader3D |
| diasceneeditor | Unknown | ✅ | DiaSceneEditorPlugin | — |
| diaserializer | Unknown | ✅ | MetadataValue, MetadataEntry, SerializeResult, ISerializer | — |
| diasoftbody2d | Unknown | ✅ | Particle, RopeDef, Rope, ClothDef, Cloth, WorldDef, SoftBodyWorld | — |
| diastatemachine | Unknown | ⚠️ | StateInfo, TransitionInfo, TransitionRecord, IStateMachineInspectable, FlatStateMachine, HierarchicalStateMachine, PushdownAutomaton, CallbackRegistry, StateMachineBuilder, HierarchicalStateMachineBuilder, StateDef, TransitionDef, StateMachineDefinition, HierarchicalStateMachineDefinition, StateMachineComponent, StateMachineTracer | TraceVerbosity |
| diatest | Unknown | — | — | no Public Interfaces |
| diatestharness | Unknown | — | — | no Public Interfaces |
| diathreading | Unknown | — | — | no Public Interfaces |
| diauicef | Unknown | ⚠️ | CEFUISystem, CEFPage, CEFSchemeHandler, CEFSchemeHandlerFactory, CEFRenderHandler | CEFBrowserManager, CEFMessageBridge |
| diauiultralight | Unknown | ✅ | UISystem | — |
| diavisualdebugger | Unknown | ✅ | IVisualDebugger, DebugLayerManager, LayerEntry, DebugColourPalette, DebugFrameData, DebugPrimitiveText2D | — |
| diawebsocket | Unknown | ⚠️ | MessageType, Message, Server, ConnectionState, Client | QueuedMessage |
| render-backend | Approved | ⚠️ | Canvas, ITexture, Mesh3DDrawCommand, Camera3D, DirectionalLight, Mesh3DFrameData | IRenderOverlay |
| example-app | Unknown | — | — | no Public Interfaces |
| example-system | Unknown | — | — | no Public Interfaces |
| googletests | Unknown | — | — | no Public Interfaces |
| googletestspeed | Unknown | — | — | no Public Interfaces |
