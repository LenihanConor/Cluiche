# Research: Ideate — Animation & Rig Layer

**Input:** docs/research/anim_rig_layer/explore.md

## Architecture Frame

All candidates assume the agreed 3-layer + game driver split:
```
DiaRig        — skeleton data, bone hierarchy, rest pose, sockets
DiaIK         — FABRIK, two-bone analytical, look-at (pure math)
DiaAnimation  — clip playback, procedural oscillators, pose blending
[game code]   — DragonAnimDriver (reads AI/genetics/world → drives DiaAnimation)
```

Each candidate below is a specific architectural choice *within* that frame — the candidates are not alternatives to the frame itself, they are design decisions about how to build each layer and how they connect.

---

## Candidates

### Candidate 1: Minimal Flat Skeleton (DiaRig)
**Home module/system:** DiaRig (new module)
**Size:** S

**Description:**
A `Skeleton` class owns a flat array of `Bone` structs. Each `Bone` holds a `StringCRC` name, a parent index (−1 = root), a local `Transform2D` (position, rotation, scale), and a world-space `Transform2D` (cached, dirty-flagged to solve DR-014). The skeleton is built from a JSON definition file (loaded via DiaCore/Json) that specifies bone name, parent name, and rest-pose transform. Bone count is data-driven so Wyvern (N neck segments) and Heavy Quadruped (different limb count) load from different files without code changes. Sockets (horn attach, rider mount, VFX point) are named anchor bones with zero length.

This is the absolute minimum rig data structure — no IK, no blending, just a clean hierarchy that other systems can read and write. DiaSoftBody2D Rope particles pin to bone world positions. DiaRigidBody2D bodies can read bone world positions for collision volume placement.

**Primary value:** Gives every other animation system a stable, tested skeleton to work against; solves DR-014 dirty-flag caching in one place.

---

### Candidate 2: Two-Algorithm IK Module (DiaIK)
**Home module/system:** DiaIK (new module)
**Size:** S

**Description:**
Two solver classes, nothing else. `TwoBoneIK` — closed-form analytical solver for exactly 2-segment limb chains (leg, arm, wing finger). Takes root position, mid-joint hint vector, and target; returns two joint angles in O(1). `FabrikIK` — iterative solver for variable-length chains up to N bones (neck, tail, spine). Takes bone chain slice, target position, and optional joint angle limits; runs up to `maxIterations` (default 10) until within `tolerance`. Both are stateless free functions operating on arrays of `Transform2D` — no heap allocation, fully deterministic on fixed inputs, trivially unit-testable.

Joint angle limits on FABRIK are per-bone min/max `Angle` values stored alongside the chain definition in DiaRig. Debug output (each iteration's intermediate positions) is behind a compile flag.

**Primary value:** Exhaustively testable pure-math layer — every solver can be validated against known geometric cases with exact expected angles.

---

### Candidate 3: Spine2D JSON Loader (DiaRig asset pipeline)
**Home module/system:** DiaRig (loader sub-system)
**Size:** S

**Description:**
A `SpineLoader` class reads a Spine2D v4 skeleton JSON file and populates a `Skeleton` + rest-pose data. Spine2D is the industry standard 2D skeletal format; AI generation tools (Cascadeur export, Rive, Adobe Animate) can produce compatible output. The loader uses DiaCore/Json (jsoncpp) so no new dependencies are needed. It maps Spine bone names to StringCRC keys, discards slots/draworder (rendering concern), and imports bone hierarchy + rest transforms only. Animation clips stored in the Spine JSON are passed to a separate `SpineAnimClipLoader` (Candidate 5).

This gives the team an authoring path today: generate a skeleton in any Spine-compatible tool (or ask an AI to produce a Spine JSON), drop the file in the asset folder, load at runtime. No bespoke editor needed at this stage.

**Primary value:** Connects the engine to the AI-generation toolchain immediately without building a custom editor.

---

### Candidate 4: Damped Spring Chain (DiaAnimation — secondary motion)
**Home module/system:** DiaAnimation (new module)
**Size:** S

**Description:**
A `SpringChain` class simulates a 1D chain of N spring-damper nodes driving bone rotations for tail, neck, and frill secondary motion. Each node has a rest angle, current angle, angular velocity, spring stiffness `k`, and damping `d`. Per-frame update: `angularAccel = -k*(angle - restAngle) - d*angularVelocity + externalTorque`. The chain root is pinned to a skeleton bone. The chain writes back to skeleton bone local rotations each frame.

This is simpler and cheaper than DiaSoftBody2D Rope — no PBD constraint projection, no particle collision, just a spring integrator. It is sufficient for tail/frill wobble at 60 Hz, easier to tune by parameter, and fully deterministic for testing (fixed input → fixed output). DiaSoftBody2D Rope remains the choice for physics-accurate tail collision with terrain.

**Primary value:** Secondary motion with zero physics dependency, trivially testable, tunable by two floats per bone.

---

### Candidate 5: Keyframe Clip Player (DiaAnimation — authored moments)
**Home module/system:** DiaAnimation
**Size:** S

**Description:**
An `AnimClip` holds a flat array of `KeyframeTrack` — one track per bone, each track a sorted array of `(time, localTransform)` keyframe pairs. `AnimClipPlayer` samples a clip at a given time using linear interpolation (slerp on angle, lerp on position/scale), writes results into target skeleton bones by StringCRC name. Playback is one-shot or looping; blend weight parameter allows additive layering over a procedural base pose. Clip data is loaded from Spine2D JSON animation sections (extending Candidate 3's loader) or from a trivial custom JSON format for manually authored key poses.

Kept intentionally minimal — no state machine, no blend tree, no transition curves. The DragonAnimDriver (game code) decides when to trigger a clip. This covers the signature moments (takeoff, landing, roar) without building a full animation graph.

**Primary value:** Enables AI-generated signature clips to play on demand without a full state machine; additive blend keeps it non-destructive over procedural base pose.

---

### Candidate 6: Procedural Locomotion Oscillator (DiaAnimation — gait)
**Home module/system:** DiaAnimation
**Size:** M

**Description:**
A `LocomotionOscillator` drives the primary walk/trot/hop cycle procedurally. Each limb has a phase offset (0–1), a stride frequency (Hz), and a stride amplitude. Per-frame: `phase += frequency * dt`; foot IK target position = rest position + sin(phase * 2π) * amplitude * strideVector. The oscillator feeds IK target positions into DiaIK's TwoBoneIK solver (Candidate 2), which writes final joint angles back to skeleton bones. Wing flap uses the same oscillator with different frequency/amplitude parameters. Gait parameters (frequency, amplitude, phase offsets per limb) are data-driven from a JSON profile, which the genetics system can override.

The oscillator itself has no rendering dependency and is fully deterministic — `Update(dt)` on fixed dt produces identical output every run, enabling exact regression tests.

**Primary value:** Eliminates the walk-cycle authoring problem entirely — locomotion emerges from two floats per limb, scalable to any rig family without new clips.

---

### Candidate 7: Look-At Constraint (DiaIK — attention direction)
**Home module/system:** DiaIK
**Size:** S

**Description:**
A `LookAtConstraint` rotates a bone (head, eye, upper neck) to face a world-space target, clamped to a min/max angle arc. Input: current bone world transform, target world position, up-vector hint, min/max angle limits. Output: new local rotation for the bone. Implemented as a stateless free function alongside TwoBoneIK and FabrikIK. A `LookAtSmoother` wrapper adds a max turn rate (degrees/sec) so rapid target changes produce smooth head tracking rather than snapping.

COW doc flags look-at as a primary AI-readability tool — the dragon's head direction communicates intent (hunger, threat, curiosity) to the player. This is a low-cost, high-readability win.

**Primary value:** Makes dragon AI intent readable through head orientation without any authored clips; trivially testable (target at known position → expect exact output angle).

---

### Candidate 8: Pose Blend Stack (DiaAnimation — layering)
**Home module/system:** DiaAnimation
**Size:** S

**Description:**
A `PoseBlendStack` holds an ordered array of `PoseLayer` — each layer specifies a bone mask (which bones it affects, by StringCRC), a weight [0,1], and a pose source (clip player, oscillator output, or IK result). The stack evaluates bottom-up: base procedural pose first, then additive overrides (look-at, clip trigger). Final bone transforms are the weighted blend of all contributing layers per bone. No full blend tree — just a priority-ordered stack with per-bone masking.

This is the minimum needed to combine: (a) oscillator-driven locomotion on legs/wings, (b) look-at on head/neck, (c) one-shot clip on full body for takeoff. Without this, the three systems stomp each other's bone writes.

**Primary value:** Prevents systems from fighting over bone ownership; enables clean combination of procedural + IK + clip without a full animation graph.

---

### Candidate 9: Skeleton Debug Renderer (DiaRig — debug draw)
**Home module/system:** DiaRig (debug sub-system, delegates to DiaGraphics)
**Size:** S

**Description:**
A `SkeletonDebugRenderer` reads a `Skeleton` and draws via `ICanvas` (DiaGraphics abstraction): bone lines (parent→child), joint local axes (small X/Y triads), socket markers, IK target positions, and spring chain rest/current angle arcs. Each channel is independently togglable via a bitmask (`DebugChannels` enum). Rendering is entirely opt-in and stripped from Release builds via `#ifdef DIA_DEBUG`. No separate debug module needed — this is a thin adapter between DiaRig data and DiaGraphics draw calls.

COW doc marks debug visibility as mandatory from Phase 1. Without this, validating IK, socket placement, and spring chain behaviour requires running the game and eyeballing — which directly contradicts the heavy testing preference.

**Primary value:** Makes procedural system state inspectable without a full editor; required for validating that IK solvers and spring chains are producing sane output.

---

## Coverage Map

| Candidate | Layer | Size | Design Axis Covered |
|-----------|-------|------|---------------------|
| 1 Minimal Flat Skeleton | DiaRig | S | Skeleton dimensionality, DR-014 fix |
| 2 Two-Algorithm IK | DiaIK | S | IK algorithm, testability |
| 3 Spine2D JSON Loader | DiaRig | S | AI-generated input format, industry standard |
| 4 Damped Spring Chain | DiaAnimation | S | Secondary motion, physics coupling |
| 5 Keyframe Clip Player | DiaAnimation | S | Authored clip trigger, AI-generated clips |
| 6 Procedural Locomotion Oscillator | DiaAnimation | M | Primary pose source, procedural locomotion |
| 7 Look-At Constraint | DiaIK | S | AI intent readability, IK completeness |
| 8 Pose Blend Stack | DiaAnimation | S | Layer granularity, pose blending |
| 9 Skeleton Debug Renderer | DiaRig | S | Debug rendering, test validation |

**Scope coverage:** 8×S + 1×M — all candidates are small. The M candidate (Oscillator) is the most novel engineering but has well-understood algorithmic foundations. The full set together forms a complete Phase 1 Wyvern animation layer with no XL bets. There is no "big framework" candidate — consistent with the user's constraint to keep the layer simple.
