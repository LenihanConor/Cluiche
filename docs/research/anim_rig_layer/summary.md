# Research Summary — Animation & Rig Layer

**Session folder:** docs/research/anim_rig_layer/
**Date:** 2026-04-26

## One-Line Answer

Build three focused Dia modules (DiaRig, DiaIK, DiaAnimation) in four waves, delivering a hybrid procedural/clip animation layer that scales across genetically-varied dragons without requiring a traditional animator.

## Journey

1. **Explored:** The COW dragon game's genetic variance and AI-driven autonomy make traditional keyframe pipelines unsustainable; the game's own animation spec (Option C) recommends a hybrid procedural rig with runtime solvers and a small authored pose library. The Cluiche engine has no animation infrastructure yet — DiaRigidBody2D and DiaSoftBody2D cover physics/secondary motion but nothing owns a skeleton hierarchy or IK.
2. **Ideated:** 9 candidates generated — all complementary building blocks spanning S and M scope; one M candidate (locomotion oscillator) deferred due to gait-feel iteration risk.
3. **Evaluated:** Minimal Flat Skeleton scored 5.00 (perfect) as the unavoidable foundation; Look-At Constraint scored highest value-per-cost (4.75); all candidates scored ≥3.90 confirming no weak links in the set.
4. **Chose:** Full 9-candidate set as a layered system — research confirmed this was an architecture decomposition, not a competition between alternatives.

## Chosen Work Item

**Name:** Dia Animation Layer  
**Home modules:** DiaRig (new), DiaIK (new), DiaAnimation (new)  
**Suggested spec type:** System (parent), with Feature specs per candidate  
**Estimated size:** S×8 + M×1 — Wave 1 alone (4×S) is achievable in under 2 weeks

## Key Insights from Exploration

- **DR-014 is a blocker:** `GetWorldMatrix()` is uncached in DiaMaths Transform2D — must be fixed as part of building the skeleton or every bone traversal will be slow. Building Candidate 1 (Flat Skeleton) with dirty-flagged caching resolves this as a side effect.
- **Secondary motion has two tools, not one:** DiaSoftBody2D Rope handles tail collision with terrain (physics layer); Damped Spring Chain handles frill/sail/neck visual follow-through (animation layer). They are complementary — do not conflate them.
- **IK isolation is the right call for testing:** Separating DiaIK as a pure-math module means every solver is a stateless free function testable with exact geometric assertions — no game state, no rendering, no time dependency.
- **Spine2D JSON is the AI authoring bridge:** No new parser dependency needed — DiaCore/Json (jsoncpp) can load it. This gives an immediate path from AI generation tools to runtime without building a custom editor.
- **Oscillator needs its own research before speccing:** Gait feel requires iteration that is hard to pre-specify. Do not spec Candidate 6 until Wave 2 is stable and you have a working skeleton + IK to experiment against.
- **Debug renderer is a testing tool, not a luxury:** Without SkeletonDebugRenderer, the only way to validate IK solver output and spring chain behaviour is visual inspection. With it, integration tests can assert bone positions programmatically.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Full keyframe-only pipeline | Content cost explodes with genetic variance; no animator on team |
| Fully procedural (no clips) | Too risky for stylised signature moments (takeoff, roar) |
| Monolithic animation module | Cannot unit-test IK solvers in isolation; violates testing preference |
| DiaSoftBody2D as animation secondary motion | Wrong layer — PBD Rope is for physics/collision, not bone-rotation follow-through |

## Build Wave Summary

| Wave | Candidates | Status |
|------|-----------|--------|
| 1 — Foundations | Flat Skeleton, Two-Algorithm IK, Look-At, Debug Renderer | Ready to spec now |
| 2 — Data + Motion Basics | Spine2D Loader, Spring Chain, Clip Player | After Wave 1 passes tests |
| 3 — Locomotion | Procedural Oscillator | Needs own research session first |
| 4 — Integration | Pose Blend Stack | After Wave 3 |

## References

- docs/research/anim_rig_layer/explore.md
- docs/research/anim_rig_layer/ideate.md
- docs/research/anim_rig_layer/evaluate.md
- docs/research/anim_rig_layer/choose.md
- C:/Users/clenihan/Desktop/ChatGPT Outputs/COW/_animation/procedural_dragon_animation_system.md
- C:/Users/clenihan/Desktop/ChatGPT Outputs/COW/_animation/dragon_animation_rig_concept_v3.docx
- C:/Users/clenihan/Desktop/ChatGPT Outputs/COW/_animation/dragon_animation_production_spec.docx
