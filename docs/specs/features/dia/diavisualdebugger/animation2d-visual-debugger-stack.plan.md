# Plan: animation2d-visual-debugger-stack

**Spec:** @docs/specs/features/dia/diavisualdebugger/animation2d-visual-debugger-stack.md
**Status:** Done
**Started:** 2026-05-04
**Last Updated:** 2026-05-04

## Tasks

| # | Task | Status | Model | Notes |
|---|------|--------|-------|-------|
| 1 | Add 4 accessor declarations + implementations to `AnimationEvaluator` | Done | sonnet | `GetSourceCount`, `GetSourceId`, `GetClipPlayer`, `GetSpringChain`; `SourceType` enum already existed |
| 2 | Add `GetLayerId(int)` to `PoseBlendStack` | Done | sonnet | One-liner implementation reading `mLayers[index].id` |
| 3 | Add 6 accessor declarations + implementations to `SpringChain` | Done | sonnet | Added `boneId` field to `NodeState` struct; populated from `def.boneIds[i]` in constructor |
| 4 | Create `DiaAnimation2DVisualDebugger/` directory and new vcxproj | Done | sonnet | GUID `{E5F6A7B8-C9D0-1234-EF01-345678901234}`; static library; modelled on `DiaIK2DVisualDebugger` |
| 5 | Write `AnimClipCursorDrawer.h/.cpp` | Done | sonnet | Text labels; normalised time; playing/stopped colour |
| 6 | Write `AnimBlendWeightsDrawer.h/.cpp` | Done | sonnet | Text labels; weight + priority per layer |
| 7 | Write `AnimSpringDrawer.h/.cpp` | Done | sonnet | Coloured circles by angular velocity; gravity ray indicator; `Skeleton::FindBoneIndex` already existed |
| 8 | Add project to `Cluiche.sln` | Done | sonnet | Added under Dia visual debugger solution folder |
| 9 | Build solution — verify zero errors | Done | sonnet | 0 errors, 9 warnings (all pre-existing) |
| 10 | Write tests | Done | sonnet | 22 tests across 6 suites; spring test uses 2 nodes (3-bone skeleton = 2 spring nodes) |
| 11 | Run tests | Done | sonnet | 22/22 pass; also 34 debug-budget + debug-text-primitive tests all pass |

## Session Notes

### 2026-05-04
- Implemented prerequisites `debug-budget` and `debug-text-primitive` (both `Approved` but unimplemented)
  - `DebugPrimitive.h`: added `DebugPrimitiveText2D` struct, `Text2D = 7` enum value, `uint32_t entityId` field
  - `DebugFrameData.h/.cpp`: added `kCapacity = 1024u`, `DroppedCount()`, `IsOverCapacity()`, `RequestDrawText()`, private `CanAdd()` guard on all 7 existing draw methods
  - `MockVisitors.h`: expanded `primitiveCount[7]` → `[8]` to accommodate `Text2D = 7`
- `AnimationEvaluator::SourceType` enum already existed — no structural change needed
- `SpringChain::NodeState` lacked `boneId` (StringCRC); added alongside existing `boneIndex`
- `Skeleton::FindBoneIndex(StringCRC)` already existed — no addition needed
- Build succeeded: 0 errors, 9 warnings (all pre-existing, none in new code)
- All 56 new tests pass (22 animation2d visual debugger + 8 debug budget + 7 debug-budget draw types + 4 entity ID + 15 debug text primitive)
