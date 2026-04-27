# Research: Explore — Animation & Rig Layer

**Session date:** 2026-04-26
**Folder:** docs/research/anim_rig_layer/

## Problem Space Overview

The COW dragon game needs to animate creatures that vary heavily in morphology (genetics-driven proportions, limb counts, tail lengths, wing shapes), across multiple locomotion states (grounded, perched, gliding, powered flight), with readable AI intent expression (hunger, caution, aggression). Traditional keyframe-heavy pipelines collapse under this combinatorial load. The game's animation doc explicitly recommends **Option C: hybrid procedural 2D rig with authored key poses and runtime solvers** — a small authored pose library anchored by a runtime system that generates final pose via procedural layers.

The Cluiche/Dia engine has no animation or rig infrastructure yet. DiaRigidBody2D and DiaSoftBody2D are specced and cover secondary motion (tail, neck chains, wing membranes) and collision. What is missing is the layer between the physics simulation and the game entity: a skeleton hierarchy to pin joints to, IK solvers for feet/wing/head contact, a procedural pose system, and a minimal clip playback system for signature moments (takeoff, special attacks).

The user has stated a strong preference for: keeping this layer simple, leaning procedural + physics-based, supporting AI-generated animation input, using industry-standard formats where possible, and verifying correctness through unit/integration tests rather than visual eyeballing.

## Existing Approaches

- **Full keyframe skeletal animation** (Unity Animator, Unreal AnimGraph) — author-heavy, poor fit for morphology variance
- **Procedural animation via oscillators + IK** (Godot's SkeletonIK, Rain World, Hollow Knight) — runtime solvers drive pose, small authored pose set as style anchor
- **FABRIK / CCD IK** — iterative IK algorithms, cheap to implement, good for limb chains up to ~6 bones
- **Two-bone analytical IK** — closed-form, very fast, standard for limbs with exactly 2 segments
- **Spring/damper secondary motion** — simple spring chains for tail, ears, frills (alternative to full PBD; cheaper but less accurate)
- **Pose blending / additive layers** — weight-blended pose samples; additive layer stacks locomotion + upper body + look-at
- **Data-driven pose graphs** — state machines with pose clips as nodes, transitions as edges (industry standard: Unreal AnimBP, Unity Animator)
- **Spine2D / DragonBones** — 2D skeletal animation tools with industry-standard JSON/binary export; AI tools can produce Spine-compatible rigs
- **glTF 2.0** — industry-standard 3D animation format; widely supported by AI generation tools; overkill for pure 2D but future-proofs 3D expansion
- **Physics-driven ragdoll blending** — blend between animated pose and physics simulation result (common for death/impact transitions)
- **Procedural locomotion** — gait cycle generated from oscillators + IK, no authored walk cycle needed (Overgrowth, Spore)

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| Skeleton dimensionality | 2D only / 2D with 2.5D tricks / 3D source | COW doc allows 2D-first; Transform2D already exists in DiaMaths |
| Pose data format | Custom binary / Spine2D JSON / glTF | Industry standard favors Spine2D for 2D; glTF for 3D source |
| IK algorithm | Two-bone analytical / FABRIK / CCD | Two-bone for limbs; FABRIK for tail/neck chains of variable length |
| Primary pose source | Authored clips only / Procedural only / Hybrid | COW explicitly recommends hybrid; user concurs |
| Authored clip trigger | Never / On demand (takeoff, special) / State machine | User: mostly procedural, keyframes for signature moments |
| Layer granularity | Monolithic / 2 layers (rig + anim) / 3+ layers | COW doc defines 8 layers; user wants "relatively simple" |
| Physics coupling | None / One-way (physics drives bones) / Two-way | DiaSoftBody2D two-way coupling exists for tail/membranes |
| AI-generated input format | Pose parameters / Clip files / Both | AI tools output Spine2D JSON or glTF; runtime consumes parameters |
| Morphology parameterisation | Hard-coded / Data-driven JSON / Genetics table | COW doc: genetics → parameter block → rig scaling |
| Debug rendering | None / Separate module / Baked into layer | COW doc: mandatory from day one; must be togglable by channel |

## Known Tradeoffs

- **FABRIK vs CCD IK**: FABRIK converges faster and handles long chains better; CCD is simpler to implement and test. For variable-length tail chains, FABRIK is preferred.
- **2D skeleton vs 3D source**: 2D skeleton is simpler and stays aligned with current DiaMaths; 3D source better serves AI generation tools and future camera freedom but adds pipeline weight.
- **Spine2D format vs custom**: Spine2D JSON is readable, widely tooled, and AI generators produce it — but requires a loader. A custom schema is simpler to own but isolates the team from tooling.
- **Procedural locomotion vs authored gait**: Procedural gait (oscillator + IK) needs no animator; authored gait is more controllable for style. Given no animator on team, procedural wins for locomotion.
- **Soft-body tail vs spring chain**: DiaSoftBody2D Rope is physically accurate; a damped spring chain is faster to iterate and test. For a 2D game at 60 Hz, spring chains are often sufficient.
- **Thin IK module vs baked-in rig**: Separating IK into its own module enables independent testing and reuse across rig families.

## Known Pitfalls (C++ / game engine context)

- **Transform hierarchy cost**: DR-014 flags `GetWorldMatrix()` as uncached — must fix before any animation system that traverses bone chains every frame.
- **IK singularities**: Two-bone IK degenerates when the target is exactly at max extension or at the root; needs clamping and hint vectors.
- **FABRIK with angle limits**: Unconstrained FABRIK can produce anatomically impossible poses; joint limits add complexity but are necessary for limbs.
- **PBD tail anchoring**: Pinned particles follow rigid body anchors with one frame of lag — must factor into bone→particle handoff order.
- **Animation clip interpolation drift**: Quaternion (or angle) interpolation needs normalization; floating-point accumulation over long clips produces subtle drift.
- **Genetics-driven bone count**: Variable segment counts (neck_01..neck_N) require the skeleton to be built at runtime from data, not hardcoded.
- **Debug draw cost**: If bone debug rendering is not gated by a flag, it will regress performance in release.
- **Testing procedural systems**: Procedural pose output is hard to assert deterministically — use canonical input fixtures (gravity=0, fixed skeleton, known target positions) and test against exact expected poses.

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| DiaMaths/Transform2D | Ready-made bone transform with parent-child hierarchy and world/local conversion |
| DiaMaths/Vector2D | IK target positions, force vectors |
| DiaMaths/Angle | Joint angle limits, rotation interpolation |
| DiaMaths/Matrix33 | TRS matrix for bone-to-world transforms |
| DiaRigidBody2D | Dragon body segments — provides COM, collision, world position for bone pinning |
| DiaSoftBody2D | Tail and neck chains (Rope), wing membrane (Cloth) — particles attach to bone endpoints |
| DiaGeometry2D | Terrain contact queries for IK foot placement, spatial queries |
| DiaCore/CRC | StringCRC for bone names (PD-001 compliant) |
| DiaCore/Json | Loading pose/skeleton data from JSON files |
| DiaApplication | Module lifecycle — animation update runs as a Module within SimProcessingUnit |
| DiaGraphics | Debug line rendering for bones, joints, IK targets |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | Bone names must use StringCRC (`spine_01`, `neck_02`) — no raw string maps |
| PD-002 ProcessingUnit/Phase/Module | Animation update must live inside a Module; runs in Sim ProcessingUnit phase |
| PD-003 Component system | Dragon entity owns animation component via IComponent/IComponentObject |
| PD-004 No STL in public APIs | Bone arrays, clip track arrays must use DiaCore containers (DynamicArrayC etc.) |
| PD-005 x64 Windows only | No cross-platform constraint; can use Windows-specific file paths for asset loading |
| PD-007 C++20 | Can use concepts to constrain IK solver templates; std::span for clip data access |

## Open Questions for Ideation

- Should DiaRig (skeleton + IK) and DiaAnimation (clip playback + procedural pose) be one module or two? COW doc separates them cleanly; user preference is "good to separate".
- Is there a third module warranted — e.g. `DiaProcAnim` for the oscillator/gait logic — or does that live inside DiaAnimation?
- What is the minimal viable skeleton for the Wyvern Phase 1 (COW build order)? How many bones and which IK chains?
- Does the engine need a full animation state machine now, or can a simple priority stack (procedural base + additive override + clip trigger) suffice?
- How does Spine2D JSON loader integrate — as part of DiaRig, or a separate DiaAsset/DiaAnimAsset loader?
- Should debug rendering live inside DiaRig/DiaAnimation or delegate entirely to DiaGraphics debug draw?
- What does "AI-generated animation" mean operationally — a pose parameter JSON file, a Spine2D export, a keyframe CSV? Need to pick one canonical input format.
- Should the spring-chain tail (simpler, easier to test) replace DiaSoftBody2D Rope for the animation layer, or complement it?
