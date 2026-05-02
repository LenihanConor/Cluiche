# Feature Spec: Skeleton Debug Renderer

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diarig2d.md | **skeleton-debug-renderer** |

**Status:** `Approved`

---

## Problem Statement

During development, it's impossible to visually verify that skeletons, poses, and forward kinematics are working correctly without rendering the bone hierarchy. Debugging transform issues by inspecting numbers is slow and error-prone. The engine needs a debug visualization that draws bones, joints, and hierarchy relationships.

---

## Solution Overview

Provide `DiaRig2DVisualDebugger` — a lightweight debug renderer in a **separate .vcxproj** (`DiaRig2DVisualDebugger`) that bridges DiaRig2D and DiaGraphics. This follows the same pattern as `DiaRigidBody2DVisualDebugger` and the planned DiaSoftBody2D visual debugger (RD-007).

### DiaRig2DVisualDebugger

```cpp
namespace Dia::Rig2D {
    class VisualDebugger {
    public:
        explicit VisualDebugger(bool enabled = true);

        void SetEnabled(bool enabled);
        bool IsEnabled() const;

        void Draw(
            const Skeleton& skeleton,
            const Dia::Core::Containers::DynamicArrayC<BoneTransform>& worldTransforms,
            Dia::Graphics::FrameData& frameData
        );
    };
}
```

### What Gets Drawn

| Element | Visual | Colour |
|---------|--------|--------|
| Bone | Line from parent joint to child joint | White |
| Joint | Small circle at bone world position | Yellow |
| Root joint | Slightly larger circle | Green |
| Bone name | Text label near joint (optional, toggleable) | Grey |
| Bone direction | Small arrowhead at bone tip | White |

### Design

- `Draw()` takes pre-computed world transforms (the output of `Pose::ComputeWorldTransforms()`). The debugger does NOT run FK itself — it renders whatever transforms the caller provides.
- Rendering uses `Dia::Graphics::FrameData` — the same frame-data pattern used by existing visual debuggers.
- On/off toggle via `SetEnabled()`. When disabled, `Draw()` is a no-op.
- Bone names are an optional second toggle (off by default for performance when many skeletons are on screen).

### Separate Project

`DiaRig2DVisualDebugger.vcxproj` — static library that depends on both DiaRig2D and DiaGraphics. This prevents DiaRig2D from depending on DiaGraphics.

### Files

| File | Purpose |
|------|---------|
| `Dia/DiaRig2DVisualDebugger/VisualDebugger.h` | Class declaration |
| `Dia/DiaRig2DVisualDebugger/VisualDebugger.cpp` | Rendering implementation |
| `Dia/DiaRig2DVisualDebugger/DiaRig2DVisualDebugger.vcxproj` | VS project |
| `Dia/DiaRig2DVisualDebugger/DiaRig2DVisualDebugger.vcxproj.filters` | VS project filters |
| `Dia/DiaRig2DVisualDebugger/dia.rig2dvisualdebugger.architecture.module.md` | Module documentation |

---

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| 1 | `DiaRig2DVisualDebugger.vcxproj` exists as separate static library in Cluiche.sln | Build verification |
| 2 | DiaRig2DVisualDebugger depends on DiaRig2D and DiaGraphics; DiaRig2D does NOT depend on DiaGraphics | Dependency review |
| 3 | `Draw()` renders bones as lines between parent-child joint positions | Visual verification in CluicheTest |
| 4 | `Draw()` renders joints as circles at bone world positions | Visual verification |
| 5 | Root joint rendered with distinct colour/size | Visual verification |
| 6 | `SetEnabled(false)` causes `Draw()` to be a no-op | Unit test: call Draw with enabled=false, verify no frame data written |
| 7 | Bone name labels toggleable (off by default) | Visual verification |
| 8 | Draw takes pre-computed world transforms, does not run FK | Code review |
| 9 | No STL in public API | Code review |
| 10 | All code in `Dia::Rig2D::` namespace | Code review |
| 11 | `dia.rig2dvisualdebugger.architecture.module.md` exists with correct YAML frontmatter | File review |

---

## Tasks

| # | Task | Depends On | Notes |
|---|------|------------|-------|
| 1 | Create `Dia/DiaRig2DVisualDebugger/` directory, .vcxproj, .vcxproj.filters, register in Cluiche.sln | - | Separate project; PD-008 compliant |
| 2 | Create `dia.rig2dvisualdebugger.architecture.module.md` | 1 | AD-001 compliance |
| 3 | Implement `VisualDebugger.h` / `VisualDebugger.cpp` | Flat Skeleton, Pose & Pose Blending features | |
| 4 | Visual test in CluicheTest: construct a skeleton, compute world transforms, draw with debugger | 3 | Manual visual verification |

---

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC for world transforms |
| PD-005 | Platform | x64 only | Compliant — DiaRig2DVisualDebugger.vcxproj targets x64 |
| PD-006 | Platform | VS project files source of truth | Compliant — separate .vcxproj manually maintained |
| PD-007 | Platform | C++20 required | Compliant |
| PD-008 | Platform | Directory.Build.props owns build paths | Compliant — no overrides |
| AD-001 | Dia App | Module YAML documentation | Compliant — architecture module doc created |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::Rig2D:: |
| RD-007 | DiaRig2D | Debug renderer is separate .vcxproj | Compliant — this feature implements that decision |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Performance | Drawing bone names as text for many skeletons could be slow. How is this handled? | Bone name labels are off by default. Toggled per-debugger instance. Typical debug use is 1-3 skeletons on screen. Performance is not a concern for debug rendering. |
| 2 | Namespace | Should the debug renderer be in `Dia::Rig2D::` or `Dia::Rig2DVisualDebugger::`? | `Dia::Rig2D::` — it's a thin utility, not a separate domain. The separate .vcxproj is for dependency isolation, not namespace isolation. Consistent with how the codebase would likely place it. |
| 3 | FrameData | Does DiaGraphics::FrameData support all needed drawing primitives (lines, circles, text)? | Needs verification during implementation. If text rendering isn't available, bone name labels will be deferred. Lines and circles are expected to be available based on existing visual debugger patterns. |

---

## Open Questions

None — all resolved above.
