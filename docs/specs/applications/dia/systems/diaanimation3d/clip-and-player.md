# Feature Spec: clip-and-player

## Parent System
@docs/specs/applications/dia/systems/render-backend/render-backend.md

**Hard dependencies (specced separately):**
- @docs/specs/applications/dia/systems/diamaths/diamaths.md — `Quaternion` (Slerp), `Vector3D`
- This-batch features: `mesh-asset-and-loader` (cgltf vendoring), `skeleton-and-pose` (Pose3D, Skeleton3D)

**Pattern reference:** @docs/specs/applications/dia/systems/diaanimation2d/diaanimation2d.md (Approved — 2D analogue)

**Research:** @docs/research/render_backend_swap/summary.md

## Status
`Approved`

## Summary

Create a new `Dia/DiaAnimation3D/` module that owns animation clip data + playback for 3D skeletal animation. After this feature:

- `Dia::Animation3D::AnimationClip3D` — array of per-bone position/rotation/scale curves with keyframes (time + value)
- `Dia::Animation3D::ClipPlayer3D` — given a clip, current time, and a target `Pose3D`, samples the clip at that time and writes interpolated bone transforms into the pose
- glTF animation loader — parses `animations[].channels` + `animations[].samplers` from glTF
- Sampling supports STEP, LINEAR, CUBICSPLINE interpolation modes (glTF spec)
- Looping, ping-pong, clamp playback modes
- An `AnimationComponent3D` ties together a clip (asset id) and a player; updates the bound `SkeletonComponent3D`'s `Pose3D` each tick

This feature mirrors the existing `DiaAnimation2D` shape (keyframe clip player, pose blend stack) with 3D types substituted. Pose blending across multiple clips lives in a Phase 2.5 feature (out of scope here).

## Problem

Phase 2 needs animated characters. Animation is a separate concern from rig data:

- DiaRig3D (`skeleton-and-pose`) owns the skeleton + pose data structure
- DiaAnimation3D owns the time-domain curves and the sampling logic that produces a Pose3D at time `t`

glTF stores animations as channels (`{node, path: translation|rotation|scale}` × samplers `{input: keyframe times, output: keyframe values, interpolation: STEP|LINEAR|CUBICSPLINE}`). The loader maps glTF nodes onto Skeleton3D bones via name match (or skin joint index — implementation detail).

## Goals

- Create `Dia/DiaAnimation3D/` module with `dia.animation3d.architecture.module.md`
- `AnimationClip3D` data structure: per-bone PRS curves with keyframes
- `KeyframeCurve<T>` — sorted keyframes (time, value) with sample(t) operation
- Three interpolation modes per curve: STEP, LINEAR, CUBICSPLINE (CUBICSPLINE matches glTF Hermite spline)
- `ClipPlayer3D::Sample(clip, time, outPose)` — for each bone in clip, look up curves and interpolate; write to outPose's `LocalBoneTransform`
- Playback modes on `ClipPlayer3D`: Once, Loop, PingPong
- glTF loader: `AnimationClip3DLoader::LoadFromGltf(path, animationIndex, skeleton, outClip)` — maps channels to bone indices via skeleton joint name match
- `AnimationComponent3D` IComponent: holds clip asset id + player; `Update(dt)` advances time, samples into bound SkeletonComponent3D's pose
- Tests under `Dia/DiaAnimation3D/Testing/`
- Per-bone curve cap: matches `Skeleton3D::kMaxBones = 256`; per-curve keyframe cap: `kMaxKeyframes = 1024`

## Non-Goals

- **Pose blending across clips** — locomotion blend tree, IK pass post-animation, etc. Defer to a later feature (`pose-blend-stack-3d`)
- **Animation state machines** — DiaStateMachine is the host; DiaAnimation3D provides the per-state clip player only
- **Morph target animation** — out of scope (mesh attribute morphs)
- **Camera / light animation** — only skeletal animation
- **Inverse kinematics during animation** — DiaIK3D handles this (post-animation pass)
- **Procedural / additive animation layers** — Phase 2.5
- **Root motion extraction** — out of scope
- **Animation event system** — frame events, sound triggers, etc. — out of scope
- **Compression** — vendored cgltf parses raw glTF; no quantisation / curve compression

## Public Interfaces

### `Dia::Animation3D::KeyframeCurve`

```cpp
// Dia/DiaAnimation3D/KeyframeCurve.h
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaMaths/Quaternion/Quaternion.h>

namespace Dia { namespace Animation3D {

enum class Interpolation : unsigned char
{
    Step        = 0,
    Linear      = 1,
    CubicSpline = 2     // glTF CUBICSPLINE — Hermite spline w/ in/out tangents
};

template<typename T, unsigned int kMaxKeyframes>
struct KeyframeCurve
{
    struct Keyframe
    {
        float time;       // seconds from clip start
        T     value;
        // For CUBICSPLINE: in-tangent and out-tangent are stored in adjacent
        // value slots (glTF format): [inT0, v0, outT0, inT1, v1, outT1, ...].
        // For STEP / LINEAR: one value per keyframe.
    };

    Interpolation                                                interpolation;
    Dia::Core::Containers::DynamicArrayC<Keyframe, kMaxKeyframes> keyframes;

    T Sample(float time) const;   // expects keyframes sorted by time
};

// Common typedefs
using PositionCurve = KeyframeCurve<Dia::Maths::Vector3D, 1024>;
using RotationCurve = KeyframeCurve<Dia::Maths::Quaternion, 1024>;
using ScaleCurve    = KeyframeCurve<Dia::Maths::Vector3D, 1024>;

} }
```

### `Dia::Animation3D::AnimationClip3D`

```cpp
// Dia/DiaAnimation3D/AnimationClip3D.h
#pragma once

#include "DiaAnimation3D/KeyframeCurve.h"
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Animation3D {

struct BoneCurves
{
    int             boneIndex;       // index into target Skeleton3D
    PositionCurve   positionCurve;   // empty curve = bone position is bind pose
    RotationCurve   rotationCurve;
    ScaleCurve      scaleCurve;
};

class AnimationClip3D
{
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"AnimationClip3D"};
    static constexpr unsigned int kMaxBones = 256;

    AnimationClip3D();

    Dia::Core::StringCRC GetAssetId() const { return mAssetId; }
    float                GetDuration() const { return mDuration; }   // clip length in seconds
    unsigned int         GetBoneCurveCount() const;
    const BoneCurves&    GetBoneCurves(unsigned int idx) const;

    // Loader-side population.
    void Populate(Dia::Core::StringCRC assetId, float duration,
                  const Dia::Core::Containers::DynamicArrayC<BoneCurves, kMaxBones>& curves);

private:
    Dia::Core::StringCRC                                          mAssetId;
    float                                                         mDuration;
    Dia::Core::Containers::DynamicArrayC<BoneCurves, kMaxBones>   mCurves;
};

} }
```

### `Dia::Animation3D::ClipPlayer3D`

```cpp
// Dia/DiaAnimation3D/ClipPlayer3D.h
#pragma once

#include "DiaAnimation3D/AnimationClip3D.h"
#include <DiaRig3D/Pose3D.h>

namespace Dia { namespace Animation3D {

enum class PlayMode : unsigned char
{
    Once     = 0,
    Loop     = 1,
    PingPong = 2
};

class ClipPlayer3D
{
public:
    ClipPlayer3D();

    void Play(const AnimationClip3D* clip, PlayMode mode = PlayMode::Loop);
    void Stop();
    void SetTime(float time);
    void SetSpeed(float speed);   // default 1.0; negative reverses

    void Update(float dt);

    // Sample current state into outPose.
    // Bones not in the clip retain outPose's existing value (caller initialises
    // outPose from skeleton.bind pose before first sample).
    void SampleInto(Dia::Rig3D::Pose3D& outPose) const;

    bool  IsPlaying() const;
    float GetTime() const;
    float GetNormalizedTime() const;   // 0.0 .. 1.0

private:
    const AnimationClip3D* mClip;
    PlayMode               mMode;
    float                  mTime;
    float                  mSpeed;
    bool                   mPlaying;
};

} }
```

### `Dia::Animation3D::AnimationClip3DLoader`

```cpp
// Dia/DiaAnimation3D/AnimationClip3DLoader.h
namespace Dia { namespace Animation3D {

class AnimationClip3DLoader
{
public:
    // Parse glTF animation by index.
    // skeleton: target Skeleton3D — used to resolve bone names to indices.
    // animationIndex: 0 = first animation in the file.
    static bool LoadFromGltf(const Dia::Core::Containers::String512& path,
                             const Dia::Rig3D::Skeleton3D&            skeleton,
                             unsigned int                             animationIndex,
                             AnimationClip3D&                         outClip);
};

} }
```

### `Dia::Animation3D::AnimationComponent3D`

```cpp
// Dia/DiaAnimation3D/AnimationComponent3D.h
namespace Dia { namespace Animation3D {

class AnimationComponent3D : public Dia::Core::IComponent
{
public:
    static constexpr Dia::Core::StringCRC kUniqueId{"AnimationComponent3D"};

    AnimationComponent3D();

    void Bind(Dia::Rig3D::SkeletonComponent3D* skeleton, const AnimationClip3D* clip);
    void Play(PlayMode mode = PlayMode::Loop);
    void Update(float dt);   // called by sim tick

    ClipPlayer3D&       GetPlayer();
    const ClipPlayer3D& GetPlayer() const;

private:
    Dia::Rig3D::SkeletonComponent3D* mSkeleton;
    const AnimationClip3D*           mClip;
    ClipPlayer3D                     mPlayer;
};

} }
```

## Implementation

### Files introduced

```
Dia/DiaAnimation3D/                            NEW MODULE
├── DiaAnimation3D.vcxproj                     NEW
├── DiaAnimation3D.vcxproj.filters             NEW
├── dia.animation3d.architecture.module.md     NEW
├── KeyframeCurve.h                            NEW (template, header-only)
├── AnimationClip3D.h                          NEW
├── AnimationClip3D.cpp                        NEW
├── ClipPlayer3D.h                             NEW
├── ClipPlayer3D.cpp                           NEW
├── AnimationClip3DLoader.h                    NEW
├── AnimationClip3DLoader.cpp                  NEW (cgltf-driven)
├── AnimationComponent3D.h                     NEW
├── AnimationComponent3D.cpp                   NEW
└── Testing/
    ├── MockAnimationClip3D.h                  NEW
    └── Fixtures/
        └── three_bone_walk.gltf               NEW (fixture: 3-bone skeleton + 1s walk cycle)
```

### Sampling algorithm (per curve)

```
T Sample(float time) {
  if (keyframes.Size() == 0) return T{};
  if (keyframes.Size() == 1) return keyframes[0].value;

  // Binary search for the upper bound — first keyframe with time >= input.
  int upper = upperBound(time);
  if (upper == 0) return keyframes[0].value;            // before first
  if (upper == keyframes.Size()) return keyframes.Back().value; // past last

  const Keyframe& k0 = keyframes[upper - 1];
  const Keyframe& k1 = keyframes[upper];
  float t01 = (time - k0.time) / (k1.time - k0.time);

  switch (interpolation) {
    case STEP:        return k0.value;
    case LINEAR:      return Lerp(k0.value, k1.value, t01);
    case CUBICSPLINE: // Hermite with k0.outTangent / k1.inTangent
                      // (for glTF: tangents are interleaved with values per spec)
                      return HermiteSpline(k0.value, k0.outTangent, k1.inTangent, k1.value, t01);
  }
}
```

For Quaternion rotations, `Lerp` is `Quaternion::Slerp` (DiaMaths). For positions/scales, linear lerp.

### glTF channel → bone mapping

```
1. Parse glTF animation[animationIndex].
2. For each animation.channel:
     channel.target_node = glTF node index.
     Find the Skeleton3D bone whose name (StringCRC) matches glTF node name.
     If not found, log a warning and skip the channel.
     channel.target_path = TRANSLATION | ROTATION | SCALE
     Append samples to BoneCurves[boneIndex].{position|rotation|scale}Curve.
3. duration = max(channel.sampler.input.max) across all channels.
```

## Files Introduced / Modified

| File | Change |
|------|--------|
| `Dia/DiaAnimation3D/` (entire module) | NEW |
| `Cluiche/Cluiche.sln` | Add DiaAnimation3D.vcxproj |

## Dependencies

| Dependency | Type | Notes |
|------------|------|-------|
| **DiaMaths `quaternion`, `vector3d-cross`** | Hard | Quaternion::Slerp; Vector3D arithmetic |
| **DiaRig3D `skeleton-and-pose`** | Hard | ClipPlayer3D::SampleInto writes to Pose3D |
| **cgltf** (via diamesh3d) | Hard | glTF animation parsing |
| `diaanimation2d` (Done) | Soft | Pattern reference |
| `diaskinning3d` | Reverse | Triggered by AnimationComponent3D's pose updates |

## Acceptance Criteria

1. `Dia/DiaAnimation3D/DiaAnimation3D.vcxproj` builds clean
2. `KeyframeCurve<T>` template instantiates for `Vector3D` and `Quaternion`; sample(t) returns expected value at boundary (t=0 → keyframe[0], t=duration → last keyframe), midpoint, before-start, past-end
3. STEP, LINEAR, CUBICSPLINE interpolation modes correctly sample the test fixture (verify against hand-computed reference values for a 3-keyframe curve)
4. `Quaternion::Slerp` is used for rotation curves (no naive lerp + normalise)
5. `AnimationClip3D` exposes duration and per-bone curves
6. `ClipPlayer3D` Play/Stop/SetTime/SetSpeed/Update behave correctly; PlayMode Once stops at duration; Loop wraps; PingPong reverses
7. `ClipPlayer3D::SampleInto(pose)` writes interpolated transforms only for bones present in the clip; absent bones leave pose values untouched
8. glTF loader correctly maps animation channels to skeleton bones by name; logs warnings for unmapped channels
9. `AnimationComponent3D::Update(dt)` advances player + samples into bound skeleton's CurrentPose
10. Tests cover: fixture three_bone_walk.gltf load, sample at t=0/0.5/1.0, loop wrap correctness, ping-pong reverse correctness, missing-bone warning
11. PD-004 audit: no STL in headers
12. `dia.animation3d.architecture.module.md` validates
13. `dia run googletest` is green

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia/dia.md |
| System (primary) | RenderBackend | @docs/specs/applications/dia/systems/render-backend/render-backend.md |

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for IDs | Compliant — clip asset id, component id |
| PD-004 | Platform | No STL in public APIs | Compliant — `DynamicArrayC` everywhere |
| PD-005 | Platform | x64 only | Compliant |
| PD-006 | Platform | VS project files | Compliant |
| PD-007 | Platform | C++20 | Compliant |
| AD-001 | Dia App | Module YAML | Compliant |
| AD-003 | Dia App | Namespace Dia::<Module>:: | Compliant — `Dia::Animation3D::` |
| AD-005 | Dia App | Component-based entities | Compliant — AnimationComponent3D is IComponent |
| **DiaMaths SD-005** | DiaMaths | Row-major matrix | N/A — animation works on Quaternion + Vector3D, not matrices directly |
| RB-002 | RenderBackend | Two-phase delivery | Phase 2 |
| RB-003 | RenderBackend | Phase 2 specs Approved, not in progress | Compliant |
| RB-013 | RenderBackend | Module decomposition mirrors 2D | Compliant — DiaAnimation3D mirrors DiaAnimation2D |
| RB-015 | RenderBackend | glTF 2.0 runtime parse | Compliant |

## AI Review Questions

| # | Question | Answer |
|---|----------|--------|
| 1 | Why a separate clip+player module instead of folding into DiaRig3D? | Same boundary as 2D: rig data is a stable "what". Animation is a time-domain "when". Splitting keeps each module's surface narrow. |
| 2 | CUBICSPLINE interpolation is rare. Worth implementing in the first round? | Yes — glTF authoring tools (Blender, Maya) export CUBICSPLINE by default for smooth curves. Skipping would mean every clip sampled at LINEAR loses smoothness. Cost is ~50 LOC. |
| 3 | What if a clip has more bones than the target skeleton? | Loader logs warning per unmapped channel; clip retains only mapped curves. Skeleton determines the universe. |
| 4 | Multiple clips on one component (locomotion blend)? | Out of scope — single clip per component. Pose blending is a future Phase 2.5 feature mirroring DiaAnimation2D's blend stack. |
| 5 | Animation events (sound triggers, frame events)? | Out of scope. |
| 6 | Root motion extraction? | Out of scope. Locomotion is in-place for Phase 2. |
| 7 | Bone curve storage cost | Per bone: 3 curves × 1024 keyframes × ~24 bytes/keyframe ≈ 72 KB worst case. 256 bones × 72 KB ≈ 18 MB. Acceptable for asset-side; in practice clips have far fewer keyframes per bone (~30 for a 1s clip at 30 fps). |
| 8 | Update on which thread? | Sim thread. Same as 2D. |
| 9 | Hot-reload | Out of scope. |
| 10 | Time accuracy | float seconds. Adequate for clips up to ~16 hours before float precision degrades. Far beyond any practical clip. |

---
