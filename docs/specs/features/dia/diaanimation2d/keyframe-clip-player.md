# Feature Spec: Keyframe Clip Player

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaanimation2d.md | **keyframe-clip-player** |

**Status:** `Approved`

---

## Problem Statement

Procedural animation covers most cases, but stylized signature moments (takeoff, roar, landing) need authored keyframe data. The engine has no clip data representation or playback mechanism. Without one, game code must manually interpolate bone transforms per frame -- error-prone and duplicated across every animation use case.

---

## Solution Overview

Four types in `Dia::Animation2D`:

### Keyframe

Single sample point:

```cpp
namespace Dia::Animation2D {
    struct Keyframe {
        float time;                         // Seconds from clip start
        Dia::Rig2D::BoneTransform transform;
    };
}
```

### KeyframeTrack

Per-bone track:

```cpp
namespace Dia::Animation2D {
    struct KeyframeTrack {
        Dia::Core::StringCRC                                boneId;
        Dia::Core::Containers::DynamicArrayC<Keyframe>      keyframes;  // Sorted ascending by time
    };
}
```

### AnimClipDef

Clip definition:

```cpp
namespace Dia::Animation2D {
    struct AnimClipDef {
        Dia::Core::StringCRC                                    id;
        float                                                   duration;   // Seconds; last keyframe time
        Dia::Core::Containers::DynamicArrayC<KeyframeTrack>     tracks;
    };
}
```

### AnimClip

Immutable runtime clip:

```cpp
namespace Dia::Animation2D {
    class AnimClip {
    public:
        explicit AnimClip(const AnimClipDef& def);

        const Dia::Core::StringCRC& GetId() const;
        float GetDuration() const;
        int   GetTrackCount() const;

        // Sample clip at time t, write results into outPose for tracked bones.
        // Bones not in any track are left unchanged. Partial keyframes (e.g. rotation
        // only) use bind pose for omitted fields -- Sample is a pure write, no read-modify.
        // Time is clamped to [0, duration].
        void Sample(float time,
                    const Dia::Rig2D::Skeleton& skeleton,
                    Dia::Rig2D::Pose& outPose) const;
    };
}
```

### AnimClipPlayer

Playback controller:

```cpp
namespace Dia::Animation2D {
    enum class PlaybackMode { kOneShot, kLooping };

    class AnimClipPlayer {
    public:
        // Begin playback of a clip. Does not own the clip -- caller manages lifetime.
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

### Key Behaviours

1. AnimClip construction validates: DIA_ASSERT on zero-duration, unsorted keyframes, duplicate bone tracks. Empty tracks silently skipped. (AND-030)
2. Sample() is pure write -- partial keyframes use bind pose for omitted fields (AND-022). No read-modify-write.
3. Linear interpolation only: lerp position/scale, shortest-arc angle lerp for rotation (AND-002).
4. Bones not found in skeleton are skipped with DIA_LOG_WARNING at AnimClip construction (AND-017). Resolution is bone ID to index at construction.
5. Play() while already playing restarts from time 0 (AND-023).
6. One-shot: time clamps to [0, duration], IsPlaying() returns false after reaching end.
7. Looping: time wraps via fmod(time, duration).
8. Negative speed supported -- one-shot clamps to 0, looping wraps backward.
9. AnimClipPlayer does not own the AnimClip -- non-owning pointer (AND-006).
10. Non-uniform track lengths supported -- each track interpolates independently.
11. Time is clamped to [0, duration] during Sample.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaAnimation2D/Keyframe.h` | Keyframe struct |
| `Dia/DiaAnimation2D/KeyframeTrack.h` | KeyframeTrack struct |
| `Dia/DiaAnimation2D/AnimClipDef.h` | AnimClipDef struct |
| `Dia/DiaAnimation2D/AnimClip.h` | AnimClip class declaration |
| `Dia/DiaAnimation2D/AnimClip.cpp` | AnimClip implementation (validation, bone resolution, sampling) |
| `Dia/DiaAnimation2D/AnimClipPlayer.h` | AnimClipPlayer class declaration |
| `Dia/DiaAnimation2D/AnimClipPlayer.cpp` | AnimClipPlayer implementation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `Keyframe` struct stores `time` (float) and `transform` (BoneTransform) | Code review |
| 2 | `KeyframeTrack` stores `boneId` (StringCRC) and `keyframes` (DynamicArrayC\<Keyframe\>) | Code review |
| 3 | `AnimClipDef` stores `id` (StringCRC), `duration` (float), and `tracks` (DynamicArrayC\<KeyframeTrack\>) | Code review |
| 4 | `AnimClip` constructor DIA_ASSERTs on zero-duration clip | Unit test: construct with duration 0, assert fires |
| 5 | `AnimClip` constructor DIA_ASSERTs on unsorted keyframes within a track | Unit test: construct with keyframes out of time order, assert fires |
| 6 | `AnimClip` constructor DIA_ASSERTs on duplicate bone tracks (two tracks with same boneId) | Unit test: construct with duplicate bone IDs, assert fires |
| 7 | `AnimClip` constructor silently skips empty tracks (no assert) | Unit test: construct with an empty track, no assert; GetTrackCount reflects only non-empty tracks |
| 8 | Sampling at exact keyframe times returns the exact keyframe transform values | Unit test: sample at t=0.0 and t=duration, verify exact match |
| 9 | Sampling between keyframes produces correct linear interpolation for position and scale | Unit test: two keyframes at t=0 and t=1, sample at t=0.5, verify lerped position/scale |
| 10 | Sampling between keyframes produces correct shortest-arc angle lerp for rotation | Unit test: rotation keyframes at 170 degrees and -170 degrees, verify interpolation goes through 180 not through 0 |
| 11 | Partial keyframes use bind pose for omitted fields (pure write, no read-modify-write) | Unit test: track with rotation-only keyframes, verify position/scale come from bind pose not previous pose state |
| 12 | Bones referenced in clip but not found in skeleton are skipped with DIA_LOG_WARNING at construction | Unit test: clip references bone "missing_bone" not in skeleton, no crash, warning logged |
| 13 | Play() while already playing restarts playback from time 0 | Unit test: Play, Update halfway, Play again, verify GetCurrentTime() == 0 |
| 14 | One-shot mode: time clamps to [0, duration], IsPlaying() returns false after reaching end | Unit test: Play one-shot, Update past duration, verify IsPlaying() == false and GetCurrentTime() == duration |
| 15 | Looping mode: time wraps via fmod(time, duration) | Unit test: Play looping, Update past duration, verify time has wrapped and IsPlaying() == true |
| 16 | Negative speed: one-shot clamps to 0, looping wraps backward | Unit test: SetSpeed(-1), Update, verify correct time behavior for both modes |
| 17 | Sample is a no-op when not playing (no clip set or after Stop) | Unit test: call Sample without Play, verify outPose unchanged |
| 18 | Non-uniform track lengths work correctly (tracks with different keyframe counts/times) | Unit test: clip with 2-keyframe track and 5-keyframe track, sample at various times, verify correct per-track interpolation |
| 19 | All public API uses Dia containers (DynamicArrayC), no STL | Code review |
| 20 | All code in `Dia::Animation2D::` namespace | Code review |
| 21 | AnimClipPlayer does not own the AnimClip (non-owning pointer) | Code review: verify raw pointer or reference, no unique_ptr/shared_ptr |
| 22 | GetNormalizedTime() returns value in [0, 1] | Unit test: verify at start, middle, and end of playback |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Keyframe.h`, `KeyframeTrack.h`, `AnimClipDef.h` -- data structs | - | Plain data types with default initialization |
| 2 | Implement `AnimClip.h` / `AnimClip.cpp` -- construction validation, bone resolution, Sample with linear interpolation | 1 | Bone ID to index resolution at construction; DIA_ASSERT on bad data; shortest-arc angle lerp |
| 3 | Implement `AnimClipPlayer.h` / `AnimClipPlayer.cpp` -- Play/Stop/Update/Sample, one-shot/looping, speed control | 2 | Non-owning clip pointer; negative speed support |
| 4 | Unit tests in `Cluiche/Tests/GoogleTests/Animation2D/` | 3 | Cover all 22 acceptance criteria |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all entity/component IDs | Compliant -- AnimClipDef.id, KeyframeTrack.boneId are StringCRC |
| PD-004 | Platform | No STL containers in public APIs | Compliant -- DynamicArrayC for keyframes, tracks; no std::vector, std::string |
| PD-007 | Platform | C++20 required | Compliant -- compiled under /std:c++20 |
| AD-002 | Dia App | No STL in public APIs | Compliant -- reinforces PD-004 |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant -- all code in Dia::Animation2D:: |
| AND-002 | DiaAnimation2D | Linear interpolation only (lerp position/scale, shortest-arc angle lerp) | Compliant -- Sample uses lerp for position/scale, shortest-arc angle lerp for rotation |
| AND-006 | DiaAnimation2D | AnimClipPlayer does not own the AnimClip | Compliant -- non-owning pointer; caller manages clip lifetime |
| AND-010 | DiaAnimation2D | No STL in public APIs | Compliant -- reinforces PD-004/AD-002 |
| AND-017 | DiaAnimation2D | Clip bones not found in skeleton: DIA_LOG_WARNING, skip | Compliant -- bone resolution at AnimClip construction logs warning and skips missing bones |
| AND-022 | DiaAnimation2D | AnimClip::Sample is a pure write; partial keyframes use bind pose | Compliant -- omitted fields filled from bind pose, no read-modify-write |
| AND-023 | DiaAnimation2D | Play() while playing restarts from time 0 | Compliant -- re-calling Play resets playback time to zero |
| AND-030 | DiaAnimation2D | AnimClipDef validates at construction: zero-duration, unsorted keyframes, duplicate bone tracks | Compliant -- DIA_ASSERT on all three conditions; empty tracks silently skipped |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Sampling | What if a track has only 1 keyframe? | The single keyframe's transform is returned for all sample times. No interpolation is needed -- the bone holds that exact transform for the entire clip duration. This is valid and useful (e.g., a bone that should be locked to a specific pose throughout the clip). |
| 2 | Sampling | What if Sample is called with time > duration? | Time is clamped to [0, duration] inside Sample (Key Behaviour 11). The caller never sees out-of-range sampling. AnimClipPlayer also clamps/wraps time before calling Sample, so double-clamping is safe but redundant. |
| 3 | Architecture | Should AnimClipPlayer expose an OnComplete callback for one-shot clips? | Not in v1. Game code can poll IsPlaying() each frame, which is simple and sufficient. An event/notify system is explicitly deferred in the DiaAnimation2D system spec. If callback-driven completion is needed later, it would be a separate feature spec. |
| 4 | Lifetime | What if AnimClip is destroyed while AnimClipPlayer still references it? | Undefined behavior -- this is the caller's responsibility per AND-006 (non-owning pointer). AnimClipPlayer stores a raw pointer; if the clip is destroyed, the pointer dangles. This matches the DiaIK2D pattern (IKSolver non-owning Skeleton reference). Documentation must clearly state the lifetime contract. |
| 5 | Playback | Should speed=0 be supported? | Yes -- speed=0 effectively pauses playback. Update(dt) advances time by dt*speed = 0, so time stays constant. IsPlaying() remains true. This is a useful and intuitive way to freeze animation without calling Stop (which would reset state). No special-case code needed. |
| 6 | Concurrency | Can two AnimClipPlayers reference the same AnimClip simultaneously? | Yes -- AnimClip is immutable after construction. Multiple players can safely read from the same clip concurrently. Each player maintains its own playback time, speed, and mode. No synchronization needed since AnimClip has no mutable state. |
| 7 | Sampling | What if the skeleton has changed (bones added/removed) since AnimClip was constructed? | AnimClip resolves bone IDs to indices at construction time. If the skeleton changes afterward, the cached indices may be invalid. However, Skeleton is immutable (DiaRig2D design), so this cannot happen in practice. If a different skeleton is passed to Sample than was used at construction, bone indices may not match -- this is caller error. |
| 8 | Edge Cases | What happens with very small duration clips (e.g., 0.001s)? | Valid as long as duration > 0. The clip plays and completes very quickly. In looping mode, fmod wrapping works correctly. For one-shot, a single Update call with a normal dt will likely overshoot, clamping to duration. No special handling needed -- the math works at any positive duration. |

---

## Open Questions

None -- all resolved above.
