# System Spec: DiaAnimation2D

## Parent Application
@docs/specs/applications/dia.md

**Status:** `Done`

---

## Purpose

DiaAnimation2D is the animation playback and blending system for the Dia engine. It sits at the top of the animation stack, consuming DiaRig2D's skeleton/pose data and optionally driving DiaIK2D targets. DiaAnimation2D provides four core capabilities: secondary motion via damped spring chains, authored clip playback from keyframe data, procedural locomotion via oscillators, and layered pose composition via a blend stack.

```
DiaAnimation2D  — spring chains, clip playback, locomotion oscillator, pose blend stack
DiaIK2D         — solvers: two-bone, FABRIK, look-at (post-process on poses)
DiaRig2D        — skeleton definition, forward kinematics, pose representation
```

DiaAnimation2D is a stateful system (spring chains carry velocity, clip players carry playback time) but has no physics or rendering dependencies. It reads and writes `Pose` local transforms, following the same contract as DiaIK2D. Game code (e.g. DragonAnimDriver) decides what to play and when; DiaAnimation2D provides the machinery.

**Dependency chain:**
`DiaAnimation2D -> DiaRig2D -> DiaMaths -> DiaCore`
`DiaAnimation2D -> DiaLogger`

---

## Responsibilities

- Define `SpringChainDef` and implement `SpringChain`: angular spring-damper chain driving bone local rotations for secondary motion (tail wobble, frill follow-through, neck sway), with optional gravity, runtime-tunable parameters, and internal sub-stepping for stability
- Define `AnimClipDef`, `KeyframeTrack`, `Keyframe` and implement `AnimClip`: keyframe data container with per-bone tracks, sampled at arbitrary time via interpolation. Validate at construction (zero-duration, unsorted keyframes, duplicate bone tracks → DIA_ASSERT)
- Implement `AnimClipPlayer`: playback controller (one-shot/looping, speed, current time) that samples an `AnimClip` into a `Pose`. Re-calling `Play()` while playing restarts from zero
- Define `PoseLayer`, `BoneMask` and implement `PoseBlendStack`: priority-ordered stack of weighted pose layers with per-bone masking, evaluated bottom-up via cascading lerp to produce a final blended `Pose`. Zero-weight layers skipped. Output pose must not alias any layer input pose
- Implement `AnimationEvaluator`: orchestrator that owns intermediate poses and runs the full animation pipeline in the correct order (FK → clip sample → spring update → blend → IK post-process → final FK). Registers animation sources (clip players, spring chains), manages their output poses internally, and exposes a single `Evaluate(dt, outPose)` call
- Load animation clip data from JSON (DiaCore/Json) — both Spine2D v4 animation sections and a minimal custom keyframe format
- Provide shortest-arc angle interpolation for rotation blending (consistent with DiaRig2D's `BlendPoses`)
- Provide `SpringParamUtils`: utility to convert artist-friendly frequency(Hz)/damping-ratio to internal stiffness(k)/damping(d) values
- Emit structured debug logs to the `Animation` DiaLogger channel (debug builds only): clip play/stop events, spring chain resets, blend stack layer changes
- Provide test utilities under `DiaAnimation2D/Testing/`: clip builders, spring chain test helpers, pose comparison utilities — shipped with the library, consumer opt-in via include
- Provide `dia.animation2d.architecture.module.md` YAML module documentation
- Provide `DiaAnimation2D.vcxproj` static library registered in `Cluiche.sln`

---

## Non-Responsibilities

- Skeleton definition, bone hierarchy, forward kinematics, pose representation — DiaRig2D
- Inverse kinematics solvers — DiaIK2D
- Animation state machines / blend trees — game code uses DiaStateMachine to drive DiaAnimation2D; DiaAnimation2D has no state machine dependency
- Physics-driven secondary motion (collision, PBD rope) — DiaSoftBody2D
- Mesh deformation / skinned mesh rendering — DiaGraphics concern
- Visual debug rendering of animation state — future DiaAnimation2DVisualDebugger
- Root motion extraction — deferred (DiaAnimation2D can diff root bone across frames but no API in v1)
- 3D animation — future DiaAnimation3D
- Animation editing UI — future DiaAnimation2DEditor
- Retargeting (playing clips authored for one skeleton on a different skeleton) — deferred
- Additive blending (adding a delta pose on top of a base) — deferred; PoseBlendStack does weighted override blending only in v1
- Event/notify system (trigger game events at specific clip times) — deferred
- Animation compression — deferred
- Position-based secondary motion (bob/bounce on dangling parts) — deferred; v1 spring chains drive rotation only
- Application scheduling — game code calls animation APIs from their phases/modules; `AnimationEvaluator` handles internal ordering

---

## Public Interfaces

### SpringChainDef / SpringChain

```cpp
namespace Dia::Animation2D {
    struct SpringNodeDef {
        float stiffness       = 50.0f;    // Spring constant k (higher = stiffer)
        float damping         = 5.0f;     // Damping coefficient d (higher = less oscillation)
        float maxAngularVelocity = 20.0f; // Radians/sec clamp; prevents multi-revolution jumps
    };

    struct SpringChainDef {
        Dia::Core::StringCRC                                    id;
        Dia::Core::StringCRC                                    rootBoneId;     // Pinned to this skeleton bone
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC> boneIds;    // Chain bones in order; must be contiguous in skeleton
        SpringNodeDef                                           defaultNode;    // Applied to all nodes unless overridden
        Dia::Core::Containers::DynamicArrayC<SpringNodeDef>     nodeOverrides;  // Per-node overrides; empty = use default
        Dia::Maths::Vector2D                                    gravityDirection = {0.0f, -1.0f};
        float                                                   gravityStrength  = 0.0f; // 0 = no gravity; applied as torque per node
    };

    class SpringChain {
    public:
        explicit SpringChain(const SpringChainDef& def, const Dia::Rig2D::Skeleton& skeleton);

        // Advance simulation by dt. Reads chain root world rotation from worldTransforms.
        // Internally sub-steps at max 1/120s. Writes local rotations back to pose.
        // Caller must have run ComputeWorldTransforms before calling this.
        void Update(float dt,
                    Dia::Rig2D::Pose& pose,
                    const Dia::Core::Containers::DynamicArrayC<Dia::Rig2D::BoneTransform>& worldTransforms);

        // Apply an external torque to a specific node (e.g. wind, impact).
        // Torque is consumed on the next Update call and then cleared.
        void ApplyExternalTorque(int nodeIndex, float torque);

        // Snap all nodes to the current pose (zero velocity). Use after teleport.
        void Reset(const Dia::Rig2D::Pose& pose);

        // Runtime parameter tuning (e.g. genetics overrides). No destroy/recreate needed.
        void SetNodeStiffness(int nodeIndex, float stiffness);
        void SetNodeDamping(int nodeIndex, float damping);
        void SetNodeMaxAngularVelocity(int nodeIndex, float maxAngularVelocity);
        void SetGravity(const Dia::Maths::Vector2D& direction, float strength);

        const Dia::Core::StringCRC& GetId() const;
        int GetNodeCount() const;
    };

    // Convert artist-friendly parameters to internal spring constants.
    // frequency: oscillation speed in Hz (e.g. 2.0 = 2 cycles/sec)
    // dampingRatio: 0 = undamped, 1 = critically damped, >1 = overdamped
    // mass: virtual mass of the node (default 1.0)
    SpringNodeDef SpringParamsFromFrequency(float frequency, float dampingRatio, float mass = 1.0f);
}
```

### AnimClip / AnimClipPlayer

```cpp
namespace Dia::Animation2D {
    struct Keyframe {
        float                   time;       // Seconds from clip start
        Dia::Rig2D::BoneTransform transform;
    };

    struct KeyframeTrack {
        Dia::Core::StringCRC                                boneId;
        Dia::Core::Containers::DynamicArrayC<Keyframe>      keyframes;  // Sorted by time ascending
    };

    struct AnimClipDef {
        Dia::Core::StringCRC                                    id;
        float                                                   duration;   // Seconds; last keyframe time
        Dia::Core::Containers::DynamicArrayC<KeyframeTrack>     tracks;
    };

    class AnimClip {
    public:
        explicit AnimClip(const AnimClipDef& def);

        const Dia::Core::StringCRC& GetId() const;
        float GetDuration() const;
        int   GetTrackCount() const;

        // Sample clip at time t, write results into outPose for tracked bones.
        // Bones not in any track are left unchanged. Partial keyframes (e.g. rotation
        // only) use bind pose for omitted fields — Sample is a pure write, no read-modify.
        // Time is clamped to [0, duration].
        void Sample(float time,
                    const Dia::Rig2D::Skeleton& skeleton,
                    Dia::Rig2D::Pose& outPose) const;
    };

    enum class PlaybackMode { kOneShot, kLooping };

    class AnimClipPlayer {
    public:
        // Begin playback of a clip. Does not own the clip — caller manages lifetime.
        // Calling Play() while already playing restarts from time 0.
        void Play(const AnimClip& clip, PlaybackMode mode = PlaybackMode::kOneShot);
        void Stop();
        void SetSpeed(float speed);     // 1.0 = normal, 0.5 = half, 2.0 = double

        // Advance playback time by dt * speed.
        void Update(float dt);

        // Sample the current playback state into outPose.
        // No-op if not playing.
        void Sample(const Dia::Rig2D::Skeleton& skeleton,
                    Dia::Rig2D::Pose& outPose) const;

        bool  IsPlaying() const;
        float GetCurrentTime() const;
        float GetNormalizedTime() const;    // [0, 1]
        const AnimClip* GetCurrentClip() const;
    };
}
```

### PoseBlendStack

```cpp
namespace Dia::Animation2D {
    struct BoneMask {
        Dia::Core::StringCRC                                        id;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC>  boneIds;
    };

    struct PoseLayer {
        Dia::Core::StringCRC    id;
        const Dia::Rig2D::Pose* pose;       // Non-owning; source pose for this layer
        float                   weight;     // [0, 1]; 0-weight layers are skipped during Evaluate
        int                     priority;   // Lower = evaluated first (base); higher = evaluated later (override)
        const BoneMask*         boneMask;   // nullptr = affects all bones
    };

    class PoseBlendStack {
    public:
        explicit PoseBlendStack(const Dia::Rig2D::Skeleton& skeleton);

        // Add/remove layers. Layers are sorted by priority for evaluation.
        void AddLayer(const PoseLayer& layer);
        void RemoveLayer(Dia::Core::StringCRC layerId);
        void SetLayerWeight(Dia::Core::StringCRC layerId, float weight);
        void SetLayerPriority(Dia::Core::StringCRC layerId, int priority);

        // Evaluate: blend all layers in priority order via cascading lerp.
        // result = lerp(result, layer[i], weight[i]) per layer, low priority first.
        // Zero-weight layers are skipped. outPose must NOT alias any PoseLayer::pose.
        void Evaluate(Dia::Rig2D::Pose& outPose) const;

        int  GetLayerCount() const;
        bool HasLayer(Dia::Core::StringCRC layerId) const;
        void Clear();
    };
}
```

### AnimationEvaluator

```cpp
namespace Dia::Animation2D {
    class AnimationEvaluator {
    public:
        explicit AnimationEvaluator(Dia::Rig2D::Skeleton& skeleton);

        AnimationEvaluator(const AnimationEvaluator&)            = delete;
        AnimationEvaluator& operator=(const AnimationEvaluator&) = delete;

        // Source registration — evaluator allocates and owns the AnimClipPlayer/SpringChain
        // and an intermediate Pose for each source. Returned pointers are NON-OWNING;
        // their lifetime is managed by the evaluator and they are invalidated by UnregisterSource.
        AnimClipPlayer* RegisterClipPlayer(Dia::Core::StringCRC sourceId,
                                           int blendPriority = 0,
                                           const BoneMask* boneMask = nullptr);

        SpringChain* RegisterSpringChain(Dia::Core::StringCRC sourceId,
                                         const SpringChainDef& def,
                                         int blendPriority = 0,
                                         const BoneMask* boneMask = nullptr);

        void UnregisterSource(Dia::Core::StringCRC sourceId);
        void SetSourceWeight(Dia::Core::StringCRC sourceId, float weight);
        void SetSourcePriority(Dia::Core::StringCRC sourceId, int priority);
        void SetSourceBoneMask(Dia::Core::StringCRC sourceId, const BoneMask* boneMask);

        // Run the full animation pipeline for this frame:
        //   1. FK (compute world transforms from current pose + root transform)
        //   2. Each clip player: Update(dt), Sample into evaluator-owned pose
        //   3. Each spring chain: Update(dt, evaluator-owned pose, world transforms)
        //   4. PoseBlendStack: Evaluate all sources into outPose
        // Caller runs DiaIK2D post-process and final FK after this returns.
        void Evaluate(float dt,
                      const Dia::Rig2D::BoneTransform& rootTransform,
                      const Dia::Rig2D::Skeleton& skeleton,
                      Dia::Rig2D::Pose& outPose);

        // Direct access for advanced use cases.
        PoseBlendStack&       GetBlendStack();
        const PoseBlendStack& GetBlendStack() const;
    };
}
```

### JSON Animation Clip Format

```json
{
    "id": "dragon_takeoff",
    "duration": 0.5,
    "tracks": [
        {
            "bone": "spine",
            "keyframes": [
                { "time": 0.0, "position": [0, 0], "rotation": 0, "scale": [1, 1] },
                { "time": 0.25, "position": [0, 0.2], "rotation": -0.1, "scale": [1, 1] },
                { "time": 0.5, "position": [0, 0.5], "rotation": -0.3, "scale": [1, 1] }
            ]
        },
        {
            "bone": "left_wing",
            "keyframes": [
                { "time": 0.0, "rotation": 0 },
                { "time": 0.5, "rotation": -1.2 }
            ]
        }
    ]
}
```

Keyframe fields are optional — omitted fields use the skeleton's bind pose value (pure write, no read-modify-write). Bone references use names (resolved to indices via `Skeleton::FindBoneIndex` at load time). Bones not found in the target skeleton are logged as warnings and skipped.

---

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Damped Spring Chain | `SpringChainDef`, `SpringNodeDef`, `SpringChain` class. Angular spring-damper integration, external torque, gravity, reset, runtime parameter tuning. Internal sub-stepping. Writes Pose local rotations. | [damped-spring-chain.md](../../features/dia/diaanimation2d/damped-spring-chain.md) | Done |
| Keyframe Clip Player | `AnimClipDef`, `KeyframeTrack`, `Keyframe`, `AnimClip`, `AnimClipPlayer`. Clip sampling with linear interpolation, one-shot/looping playback, speed control. Clip validation at construction. | [keyframe-clip-player.md](../../features/dia/diaanimation2d/keyframe-clip-player.md) | Done |
| Animation Clip Loader | JSON loader for animation clip data — custom format and Spine2D v4 animation sections. Uses DiaCore/Json. Spine2D coordinate/unit conversion at load time. | [animation-clip-loader.md](../../features/dia/diaanimation2d/animation-clip-loader.md) | Done |
| Pose Blend Stack | `PoseLayer`, `BoneMask`, `PoseBlendStack`. Priority-ordered stack with per-bone masking, cascading lerp evaluation. Zero-weight skip. Output alias protection. BoneMask copied by value into each layer (no pointer lifetime dependency). | [pose-blend-stack.md](../../features/dia/diaanimation2d/pose-blend-stack.md) | Done |
| Animation Evaluator | `AnimationEvaluator` orchestrator. Owns `AnimClipPlayer`, `SpringChain`, and intermediate poses. Non-copyable. Registers sources, runs full pipeline (FK → clip → spring → blend). | [animation-evaluator.md](../../features/dia/diaanimation2d/animation-evaluator.md) | Done |
| Spring Parameter Utilities | `SpringParamsFromFrequency()`: convert artist-friendly frequency(Hz)/damping-ratio to internal k/d values. | [spring-parameter-utilities.md](../../features/dia/diaanimation2d/spring-parameter-utilities.md) | Done |
| Procedural Locomotion Oscillator | Sine-based gait oscillator driving IK targets for limb locomotion. Data-driven per-limb phase/frequency/amplitude. | TBD | Deferred |
| Test Utilities | `DiaAnimation2D/Testing/`: clip builders, spring chain helpers, pose comparison utilities. Ships with library; consumer opt-in via include. | [test-utilities.md](../../features/dia/diaanimation2d/test-utilities.md) | Done |

---

## Dependencies on Other Systems

**Required:**
- **DiaRig2D** — `Skeleton` (bone hierarchy, `FindBoneIndex`), `Pose` (read/write local transforms), `BoneTransform`, `BlendPoses`
- **DiaMaths** — `Vector2D`, angle interpolation, trigonometric functions
- **DiaCore** — `StringCRC` (IDs for clips, chains, layers, bones), `DynamicArrayC` (tracks, keyframes, node arrays), `DIA_ASSERT`, `Json` (clip loading)
- **DiaLogger** — `Animation` channel for structured debug logging

**Explicitly excluded:**
- **DiaIK2D** — no dependency; DiaAnimation2D produces poses, DiaIK2D post-processes them. Game code orchestrates the order.
- **DiaStateMachine** — no dependency; game code uses DiaStateMachine to decide what DiaAnimation2D plays. DiaAnimation2D is a dumb executor.
- **DiaGraphics** — no rendering; visual debug belongs to a future DiaAnimation2DVisualDebugger
- **DiaApplicationFlow** — no dependency; callers invoke animation APIs from within their phases/modules
- **DiaRigidBody2D / DiaSoftBody2D** — no physics coupling; spring chains are pure angular spring-dampers, not PBD

**Dependents:**
- Game code (CluicheTest, future games) — creates spring chains, plays clips, evaluates blend stacks
- Future DiaAnimation2DVisualDebugger — reads animation state for debug rendering
- Future DiaAnimation2DEditor — uses animation APIs for editor preview

---

## Out of Scope

- Skeleton definition, FK, pose representation — DiaRig2D
- Inverse kinematics — DiaIK2D
- Animation state machines / blend trees — game code orchestrates via DiaStateMachine
- Physics-driven secondary motion (PBD rope, collision) — DiaSoftBody2D
- Root motion extraction — deferred (requires diffing root bone across frames; no API in v1)
- 3D animation / quaternion interpolation — future DiaAnimation3D
- Retargeting (cross-skeleton clip application) — deferred
- Additive blending (delta pose on top of base) — deferred; v1 blend stack does weighted override only
- Animation events / notifies (game callbacks at clip timestamps) — deferred
- Animation compression (quantization, curve fitting) — deferred
- Cubic / Hermite spline interpolation — deferred; v1 uses linear interpolation only
- Bone stretching during clip playback — deferred
- Multi-clip blend (blend tree with N inputs) — deferred; v1 uses two-pose lerp via DiaRig2D::BlendPoses per layer
- Skinned mesh deformation — DiaGraphics concern
- Visual debug rendering — future DiaAnimation2DVisualDebugger
- Editor UI — future DiaAnimation2DEditor

---

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AND-001 | Spring chain uses angular spring-damper integration, not PBD | Cheaper than DiaSoftBody2D PBD constraints, fully deterministic on fixed dt, sufficient for visual follow-through. PBD Rope remains the tool for collision-aware physics chains. | Damped Spring Chain | Accepted | Yes |
| AND-002 | AnimClip uses linear interpolation only (lerp position/scale, shortest-arc angle lerp) | Cubic/Hermite splines add complexity with minimal visual benefit for 2D at 60 Hz. Consistent with DiaRig2D's `BlendPoses` interpolation. Deferred to a future feature if needed. | Keyframe Clip Player | Accepted | Yes |
| AND-003 | PoseBlendStack is a flat priority-ordered stack, not a blend tree | A full blend tree is over-engineered for the procedural-first approach. The stack covers the primary use case: base procedural pose + IK overlay + one-shot clip. Game code handles logic; DiaAnimation2D handles blending. | Pose Blend Stack | Accepted | Yes |
| AND-004 | DiaAnimation2D has no DiaStateMachine dependency | Animation state machines are a game-code concern. Game code reads DiaStateMachine state and calls DiaAnimation2D APIs (Play, Stop, SetLayerWeight). This keeps DiaAnimation2D a dumb executor. | All features | Accepted | Yes |
| AND-005 | SpringChain writes to Pose local rotations, same contract as DiaIK2D | Consistent with the animation stack convention: all systems read/write `Pose` local transforms. `Skeleton` is immutable definition. FK is caller-responsibility after animation evaluation. | Damped Spring Chain | Accepted | Yes |
| AND-006 | AnimClipPlayer does not own the AnimClip | Non-owning pointer — caller manages clip lifetime. Multiple players can reference the same clip. Consistent with `IKSolver` non-owning `Skeleton&` pattern (SD-008). | Keyframe Clip Player | Accepted | Yes |
| AND-007 | Animation clips loaded from JSON via DiaCore/Json (jsoncpp) | No new parser dependency. Supports both a minimal custom format and Spine2D v4 animation sections. Consistent with DiaRig2D's JSON skeleton loader. | Animation Clip Loader | Accepted | Yes |
| AND-008 | Procedural Locomotion Oscillator deferred to Wave 3 | Gait feel requires iteration that is hard to pre-specify. Needs its own research session against a working skeleton + IK. Do not spec until Wave 2 is stable. | Procedural Locomotion Oscillator | Accepted | Yes |
| AND-009 | Per-bone BoneMask uses StringCRC bone IDs, resolved to indices lazily at Evaluate time | Skeleton is not required at AddLayer time. Resolution is deferred to each Evaluate call using a scratch buffer, avoiding a skeleton dependency at construction. BoneMask IDs are copied by value into a compact internal LayerBoneMask at AddLayer to eliminate pointer lifetime issues. | Pose Blend Stack | Accepted | Yes |
| AND-010 | No STL in public APIs | Consistent with PD-004 / AD-002. | All features | Accepted | Yes |
| AND-011 | Test utilities ship inside `DiaAnimation2D/Testing/` | Platform-wide pattern (DiaRig2D, DiaIK2D, DiaStateMachine, DiaSoftBody2D). Consumer opt-in via include. | Test Utilities | Accepted | Yes |
| AND-012 | Spring chain integration uses semi-implicit Euler | Explicit Euler is unstable at high stiffness; full implicit Euler is overkill for visual-only secondary motion. Semi-implicit (update velocity then position) is stable, simple, and deterministic. | Damped Spring Chain | Accepted | Yes |
| AND-013 | SpringChain sub-steps large dt internally with a max step of 1/120s | Variable dt from gameplay can cause instability at high stiffness. Rather than requiring callers to sub-step, `SpringChain::Update(dt)` internally subdivides dt into steps of at most ~8.3ms (1/120). Deterministic for any dt; bounded cost (max 8 sub-steps at 60 Hz). | Damped Spring Chain | Accepted | Yes |
| AND-014 | SpringChain bones must be contiguous in the skeleton's flat array | The chain walks bone indices sequentially for FK reads. Non-contiguous chains would require arbitrary-index gathers and complicate partial FK propagation. DIA_ASSERT at construction that all `boneIds` resolve to a contiguous ascending index range. | Damped Spring Chain | Accepted | Yes |
| AND-015 | SpringChain requires FK to have been run before Update so world-space data is fresh | SpringChain reads world-space root bone rotation to compute chain drive. `AnimationEvaluator` handles this automatically (runs FK as step 1). For raw API users, the evaluation order is: (1) FK, (2) clip sample, (3) spring update, (4) blend stack evaluate. Game code then runs DiaIK2D post-process and final FK. | Damped Spring Chain | Accepted | Yes |
| AND-016 | PoseBlendStack uses cascading lerp, not normalized weighted average | Evaluation is `result = lerp(result, layer[i], weight[i])` per layer bottom-up. Weight 1.0 = full override. This is intuitive for artists (each layer is "how much does this override what's below") and matches Unity/Unreal layer conventions. Weights do not need to sum to 1. | Pose Blend Stack | Accepted | Yes |
| AND-017 | Clip bones not found in skeleton are silently skipped with a DIA_LOG_WARNING | Bone name mismatch between clip and skeleton is common (shared clips across rig variants). Assert would crash on valid use cases. Warning logs the mismatch once per clip load. Bone ID→index resolution happens at `AnimClip` construction, not per-sample. | Animation Clip Loader | Accepted | Yes |
| AND-018 | Spine2D loader converts degrees to radians and Y-up to engine convention at load time | Spine2D uses degrees for rotation and Y-up coordinates. Dia uses radians. All conversion happens once in the loader — runtime data is always in Dia conventions. No per-frame conversion cost. | Animation Clip Loader | Accepted | Yes |
| AND-019 | SpringChain clamps angular velocity to a configurable maximum (default 20 rad/s) | Prevents multi-revolution jumps after hitches or extreme external torques. The max is per-node, settable via `SpringNodeDef`. 20 rad/s (~3 revolutions/sec) is generous for visual follow-through. | Damped Spring Chain | Accepted | Yes |
| AND-020 | BoneMask with invalid bone IDs: DIA_LOG_WARNING and skip at AddLayer time | Same pattern as AND-017. Invalid bone names in a mask are logged and excluded from the resolved index set. This allows shared masks across skeleton variants without crashing. | Pose Blend Stack | Accepted | Yes |
| AND-021 | AnimationEvaluator orchestrates the full pipeline and owns intermediate poses | Eliminates the data flow contradiction (spring chains write in-place but blend stack needs separate source poses). Evaluator allocates one Pose per registered source, runs the pipeline in the correct order, and blends into the target pose. Callers who want manual control can still use the raw APIs directly. | Animation Evaluator | Accepted | Yes |
| AND-022 | AnimClip::Sample is a pure write — partial keyframes use bind pose for omitted fields | Using "current pose" for omitted fields makes Sample a read-modify-write, creating order-dependent bugs when multiple clips sample into the same pose. Bind pose is deterministic and order-independent. | Keyframe Clip Player | Accepted | Yes |
| AND-023 | AnimClipPlayer::Play() while already playing restarts from time 0 | Re-triggering is the most common animation API call pattern (interrupt and restart). Silent continue or assert are both surprising. Restart-from-zero matches Unity/Unreal convention. | Keyframe Clip Player | Accepted | Yes |
| AND-024 | PoseBlendStack::Evaluate output must not alias any PoseLayer::pose input | Read-while-write aliasing produces undefined behavior. DIA_ASSERT in debug that outPose address differs from all layer pose pointers. AnimationEvaluator guarantees this by construction. | Pose Blend Stack | Accepted | Yes |
| AND-025 | PoseBlendStack layers use explicit integer priority, not push-order | Enables insert-anywhere without clearing and rebuilding. Lower priority = evaluated first (base). Higher priority = evaluated later (override). Matches intuitive layer ordering. | Pose Blend Stack | Accepted | Yes |
| AND-026 | PoseBlendStack skips layers with weight == 0.0f during Evaluate | Explicit fast-path optimization. A zero-weight layer has no effect on the output — skipping it avoids unnecessary per-bone lerp. Documented behavior so callers can rely on it for disabling layers cheaply. | Pose Blend Stack | Accepted | Yes |
| AND-027 | SpringChainDef includes optional gravity (direction + strength) | Hanging chains (jaw, frill, dangling parts) need gravity pull. Without it, callers must compute gravity-as-torque per node per frame. Gravity is applied internally as torque during Update. Strength 0 = disabled (default). | Damped Spring Chain | Accepted | Yes |
| AND-028 | SpringChain exposes runtime setters for stiffness, damping, maxAngularVelocity, gravity | Genetics system overrides spring parameters per dragon variant at runtime. Destroy/recreate would reset simulation state (velocities). Setters allow hot-tuning without state loss. | Damped Spring Chain | Accepted | Yes |
| AND-029 | SpringParamsFromFrequency utility converts Hz/damping-ratio to k/d | Artists think in frequency and damping ratio; programmers use raw k/d. Utility converts: `k = (2π * freq)² * mass`, `d = 2 * dampingRatio * (2π * freq) * mass`. Keeps k/d as internal representation. | Spring Parameter Utilities | Accepted | Yes |
| AND-030 | AnimClipDef validates at AnimClip construction: zero-duration, unsorted keyframes, duplicate bone tracks → DIA_ASSERT | These are authoring errors, not runtime conditions. Catching at construction prevents silent corruption during sampling. Empty tracks are silently skipped (valid — track may be intentionally unused). | Keyframe Clip Player | Accepted | Yes |

**Status values:** `Proposed` . `Accepted` . `Rejected` . `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system . `No` = guidance only

---

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Clip IDs, chain IDs, layer IDs, bone mask IDs, bone references all use StringCRC |
| PD-004 | Platform | No STL containers in public APIs | `DynamicArrayC` for keyframe arrays, track arrays, bone ID arrays, layer arrays |
| PD-005 | Platform | x64 only | `DiaAnimation2D.vcxproj` targets x64 exclusively; no Win32 configurations |
| PD-006 | Platform | Visual Studio project files are source of truth | `DiaAnimation2D.vcxproj` and `.vcxproj.filters` created and manually maintained; all files explicitly listed |
| PD-007 | Platform | C++20 required | All code compiled under `/std:c++20` |
| PD-008 | Platform | `Directory.Build.props` owns OutDir/IntDir/toolchain | `DiaAnimation2D.vcxproj` must NOT override OutDir, IntDir, PlatformToolset, WindowsTargetPlatformVersion, or LanguageStandard |
| PD-009 | Platform | All generated non-binary output under `Cluiche/out/<AppName>/` | Any generated animation output goes under `Cluiche/out/` |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create `dia.animation2d.architecture.module.md` with public API, responsibilities, and dependency declarations |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 |
| AD-003 | Dia App | Namespace `Dia::<Module>::` | All code in `Dia::Animation2D::` namespace |
| AD-005 | Dia App | Component-based entities (IComponent/IComponentObject) | Future AnimationComponent wraps DiaAnimation2D state for entity attachment (deferred — not in v1) |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Spring Chain | Should SpringChain support variable per-node stiffness/damping, or a single value for the whole chain? | Both. `SpringChainDef` has a `defaultNode` applied to all nodes, with an optional `nodeOverrides` array for per-node tuning. Typical use: stiffer near root, looser at tip. Per-node costs one extra float read — negligible. |
| 2 | Spring Chain | What integration method should the spring chain use? | Semi-implicit Euler (AND-012). Update velocity first (`v += a * dt`), then angle (`angle += v * dt`). Stable for typical stiffness values (k < 500), simple, deterministic. If instability arises at extreme stiffness, clamp angular velocity rather than switching integrators. |
| 3 | Spring Chain | Should SpringChain interact with DiaIK2D (e.g. spring chain on a tail that also has FABRIK)? | No interaction in v1. Spring chains and IK both write to Pose local rotations — last writer wins. Game code must choose which bones are spring-driven vs IK-driven. The PoseBlendStack with bone masks is the arbitration mechanism. |
| 4 | Clip Player | Should AnimClip support partial keyframes (e.g. only rotation, no position)? | Yes — keyframe fields are optional. Omitted fields use the skeleton's bind pose value (AND-022). This makes Sample() a pure write — no read-modify-write, no order-dependent bugs when multiple clips sample into the same pose. |
| 5 | Clip Player | How does time clamping work for one-shot vs looping clips? | One-shot: time clamps to [0, duration], `IsPlaying()` returns false after reaching duration. Looping: time wraps via `fmod(time, duration)`. Speed can be negative for reverse playback — one-shot clamps to 0, looping wraps backward. |
| 6 | Clip Player | Should AnimClipPlayer support blend weight for cross-fading between clips? | Not internally — cross-fading is a PoseBlendStack concern. Two AnimClipPlayers sample into two separate Poses; the blend stack cross-fades via layer weights. This avoids duplicating blend logic. |
| 7 | Blend Stack | How does the PoseBlendStack handle bone conflicts across layers? | Bottom-up evaluation. For each bone, the final transform is the weighted blend of all layers that include that bone (via BoneMask). Layers without a mask affect all bones. Higher layers override lower layers proportionally to their weight. Weight 1.0 = full override; weight 0.5 = 50% blend with layers below. |
| 8 | Blend Stack | Should PoseBlendStack support additive blending (base + delta) in addition to override blending? | Not in v1. Override (weighted lerp) covers the primary use case. Additive blending requires a reference pose to compute deltas, which adds complexity. Deferred as a future feature if clip layering needs it. |
| 9 | Clip Loader | Should the clip loader support Spine2D animation data directly, or require a conversion step? | Direct load from Spine2D v4 JSON animation sections. The loader reads timelines (rotate, translate, scale per bone), converts to `AnimClipDef` format. Slots, draw order, and attachment timelines are ignored (rendering concern). No conversion step — load at runtime. |
| 10 | Architecture | Should DiaAnimation2D provide an AnimationComponent (IComponent) for entity integration? | Not in v1. AnimClipPlayer and SpringChain are lightweight enough to manage directly in game code. A future AnimationComponent feature spec can wrap them if the pattern proves cumbersome. Deferred to avoid premature abstraction. |
| 11 | Architecture | How does DiaAnimation2D interact with the ProcessingUnit/Phase execution model? | DiaAnimation2D has no DiaApplicationFlow dependency. `AnimationEvaluator::Evaluate(dt)` runs the internal pipeline in the correct order (FK → clip → spring → blend). Game code calls `Evaluate(dt)` from within their own phases, then runs DiaIK2D post-process and final FK. The raw APIs (SpringChain, AnimClipPlayer, PoseBlendStack) are still public for advanced manual orchestration. |
| 12 | Testing | How are spring chains tested deterministically given floating-point integration? | Fixed dt with known initial conditions produces exact expected output. Tests use `dt = 1/60`, known stiffness/damping, and assert angular positions within tight tolerance (1e-5). The semi-implicit Euler integrator is deterministic for identical inputs. |
| 13 | Clip Player | Should clips support non-uniform track lengths (different bones have keyframes at different times)? | Yes — each `KeyframeTrack` has its own keyframe array. A bone with keyframes at [0, 0.5] and another at [0, 0.25, 0.5] are both valid in the same clip. Sampling at any time interpolates within each track independently. |
| 14 | Spring Chain | What happens to a SpringChain during a teleport (large position change)? | Call `SpringChain::Reset(pose)` to snap all nodes to the current pose with zero velocity. Without reset, a teleport causes extreme spring forces on the next `Update`, producing violent oscillation. Document this contract. |
| 15 | Blend Stack | Can the blend stack have zero layers? | Yes — `Evaluate()` with zero layers is a no-op (outPose unchanged). This is safe and expected during setup or when all animation is disabled. |
| 16 | Spring Chain | How does SpringChain handle variable/large dt from gameplay hitches? | `Update(dt)` internally sub-steps with a max step of 1/120s (AND-013). At 60 Hz normal gameplay this means 1 sub-step. A 100ms hitch produces ~12 sub-steps — bounded cost, stable result. No caller sub-stepping required. |
| 17 | Spring Chain | Must spring chain bones be contiguous in the skeleton's flat bone array? | Yes (AND-014). Bones must resolve to a contiguous ascending index range. DIA_ASSERT at `SpringChain` construction if not. This simplifies FK reads and partial propagation. Game code must author skeleton definitions with chain bones adjacent. |
| 18 | Spring Chain | SpringChain reads world-space root bone rotation — but Pose only stores local transforms. Who runs FK first? | The caller (AND-015). The documented evaluation order puts FK before spring chain update. SpringChain does not internally call `ComputeWorldTransforms` — it expects the caller to have fresh world-space data available. This is the same contract as DiaIK2D's `SetRootTransform()` requirement. |
| 19 | Blend Stack | Is PoseBlendStack evaluation cascading lerp or normalized weighted average? | Cascading lerp (AND-016): `result = lerp(result, layer[i], weight[i])` bottom-up. Weight 1.0 = full override of everything below. Weights do not need to sum to 1. This matches Unity/Unreal convention and is intuitive for artists. |
| 20 | Clip Loader | What happens when a clip references a bone name that doesn't exist in the target skeleton? | DIA_LOG_WARNING at `AnimClip` construction, bone track silently skipped at sample time (AND-017). This is common with shared clips across skeleton variants. Resolution is bone ID→index at construction, not per-sample. |
| 21 | Clip Loader | How does the Spine2D loader handle coordinate/unit conventions? | Spine2D uses degrees and Y-up. The loader converts to radians and Dia's axis convention at load time (AND-018). Runtime data is always in Dia units. No per-frame conversion. |
| 22 | Spring Chain | What prevents multi-revolution angular velocity jumps after a hitch or extreme torque? | `SpringNodeDef::maxAngularVelocity` clamp (AND-019), default 20 rad/s (~3 rev/sec). Applied per sub-step after velocity integration. Configurable per node for artistic control. |
| 23 | Blend Stack | What if a BoneMask references a bone name not in the skeleton? | DIA_LOG_WARNING at `AddLayer` time, invalid bone ID excluded from the resolved index set (AND-020). Same graceful-degradation pattern as clip bone mismatch (AND-017). Allows shared masks across skeleton variants. |
| 24 | Architecture | Who allocates the intermediate Pose objects that PoseLayer pointers reference? | `AnimationEvaluator` owns them (AND-021). It allocates one Pose per registered source at registration time and reuses across frames. For callers using the raw PoseBlendStack API directly, the caller pre-allocates N+1 poses (N sources + 1 output) and must ensure outPose does not alias any layer input (AND-024). |
| 25 | Spring Chain | Can multiple SpringChains write to the same bones on the same skeleton? | Yes, but last writer wins — same as DiaIK2D chain overlap (AI Q5 in DiaIK2D spec). The PoseBlendStack with bone masks is the correct arbitration tool. If two spring chains overlap without the blend stack, the second `Update` call overwrites the first. Document this as caller responsibility. |
| 26 | Clip Player | What happens when `Play()` is called while already playing? | Restarts from time 0 (AND-023). The previous playback state is discarded. This matches Unity/Unreal convention and is the expected behavior for re-triggering clips (e.g. repeated roar). |
| 27 | Blend Stack | Can the output pose alias a layer's source pose in `Evaluate()`? | No — DIA_ASSERT in debug (AND-024). Read-while-write aliasing produces undefined behavior. `AnimationEvaluator` guarantees this by construction since it owns separate intermediate poses. |
| 28 | Blend Stack | How does layer ordering work? | Explicit integer priority (AND-025). Lower priority = base, higher priority = override. Layers can be inserted at any priority without clearing the stack. Layers at the same priority are evaluated in registration order. |
| 29 | Blend Stack | What happens to zero-weight layers during `Evaluate()`? | Skipped entirely (AND-026). No per-bone lerp is performed. This is both an optimization and a documented behavior — callers can use weight 0 to cheaply disable a layer without removing it. |
| 30 | Spring Chain | How do artists tune spring parameters? | `SpringParamsFromFrequency(hz, dampingRatio)` converts to internal k/d (AND-029). Artists set "2 Hz, 0.7 damping ratio" instead of "k=50, d=5". Runtime setters (AND-028) allow live tuning without destroying the chain. |
| 31 | Spring Chain | Do hanging chains (jaw, frill) need gravity? | Yes — `SpringChainDef` includes optional gravity direction + strength (AND-027). Applied internally as torque per node. Default strength 0 = disabled. Avoids callers computing gravity-as-torque manually every frame. |
| 32 | Clip Player | What validation happens at AnimClip construction? | DIA_ASSERT on zero-duration clips, unsorted keyframes, duplicate bone tracks (AND-030). These are authoring errors. Empty tracks are silently skipped (valid use case — track may be intentionally unused). |
| 33 | Architecture | Can callers bypass AnimationEvaluator and use the raw APIs directly? | Yes. SpringChain, AnimClipPlayer, and PoseBlendStack are all public. AnimationEvaluator is the recommended path for typical use. Raw API access is available for advanced use cases (custom pipeline ordering, manual pose management). |
| 34 | Spring Chain | Is rotation-only sufficient for secondary motion, or do dangling parts need position bobbing? | Rotation-only is sufficient for v1. Position bobbing on dangling parts is an additive-pose concern — deferred with additive blending. Most 2D secondary motion (tail wag, frill wobble, jaw bounce) reads naturally as rotation chains. |
| 35 | Spring Chain | Can spring chain parameters be changed at runtime without destroying the chain? | Yes (AND-028). `SetNodeStiffness`, `SetNodeDamping`, `SetNodeMaxAngularVelocity`, and `SetGravity` allow hot-tuning. Simulation state (velocities, positions) is preserved. This supports genetics-driven per-dragon-variant tuning. |
| 36 | Clip Player | Should AnimClip flatten its keyframe data into a single contiguous array for cache performance? | Deferred (premature optimization). Per-track `DynamicArrayC` is simple and correct. Profile later — if sampling is a hot path, a contiguous-array layout with offset/count per track can be added without API changes. |

---

## Status

`Done` — Implemented 2026-05-02. 120 tests passing. Architectural fixes applied 2026-05-02 (pointer stability, const, named constants). Plan: `diaanimation2d.plan.md`
