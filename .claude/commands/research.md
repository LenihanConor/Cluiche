Take the user through a structured research funnel: explore → ideate → evaluate → choose → findings. Produces markdown artifacts in docs/research/<topic_slug>/ that summarise the exploration and end with a chosen, ready-to-spec work item.

---

## Step 1 — Orient

Check docs/research/ for existing session folders.

If the user's message mentions a topic name that matches (or closely resembles) an existing folder:
- Read whichever of explore.md / ideate.md / evaluate.md / choose.md / summary.md already exist
- Report which stages are complete, e.g. "I found docs/research/2d_rigid_body/ — explore and ideate are done."
- Ask: "Continue from evaluate, or re-run an earlier stage?"
- Jump to the appropriate step below once confirmed.

If this is a new session:
1. Ask: "What topic or problem area do you want to research?"
2. Ask: "Any constraints, hunches, or directions to include or exclude?"
3. Derive the topic slug:
   - Lowercase the topic phrase
   - Remove stop words: a, an, the, and, or, for, in, of, to, with, is, are, that
   - Take up to 4 significant tokens; truncate each to 8 characters; join with underscores
   - Example: "2D rigid body physics sleeping" → 2d_rigid_body_physics
4. Present the proposed slug: "Proposed folder: docs/research/<slug>/. Confirm, or type a custom slug."
5. Wait for confirmation before creating any files. Then continue to Step 2.

---

## Step 2 — Explore

Goal: map the problem space before generating solutions.

Before writing anything, read silently:
- @.claude/steering/tech.md
- @.claude/steering/structure.md
- @docs/specs/platform/Cluiche.md

Survey the problem space and write docs/research/<slug>/explore.md using this structure:

```
# Research: Explore — <Topic>

**Session date:** YYYY-MM-DD
**Folder:** docs/research/<slug>/

## Problem Space Overview
[2–3 paragraph survey of what this domain is and why it matters]

## Existing Approaches
[Bulleted list of patterns or techniques that exist in the industry or literature]

## Design Axes
| Axis | Options | Notes |
|------|---------|-------|
| ... | ... | ... |

## Known Tradeoffs
[Bulleted list]

## Known Pitfalls (C++ / game engine context)
[Bulleted list]

## Cluiche-Specific Opportunities

### Relevant Existing Modules
| Module | Relevance |
|--------|-----------|
| DiaCore | ... |
| ... | ... |

### Platform Decision Constraints
| Decision | Implication for this topic |
|----------|---------------------------|
| PD-001 StringCRC | ... |
| PD-004 No STL in public APIs | ... |
| ... | ... |

## Open Questions for Ideation
[Bulleted list of unresolved questions that should shape the candidate list]
```

After writing, print: "Explore complete. Proceed to ideation? (yes / no / re-run explore)"
Wait for confirmation before continuing.

---

## Step 3 — Ideate

Goal: generate a bounded list of concrete candidate features or experiments.

Read docs/research/<slug>/explore.md.

Generate 5–10 candidates. Each must:
- Be plausibly buildable within the Cluiche/Dia module hierarchy
- Have a clear home module or a named new module
- Be sized as S (≤1 week), M (1–3 weeks), L (1–2 months), or XL (>2 months)

Write docs/research/<slug>/ideate.md using this structure:

```
# Research: Ideate — <Topic>

**Input:** docs/research/<slug>/explore.md

## Candidates

### Candidate 1: <Name>
**Home module/system:** <e.g., DiaRigidBody2D / new DiaSomething>
**Size:** S / M / L / XL
**Description:** [1–2 paragraphs]
**Primary value:** [One sentence: what changes for the player or the engine]

[Repeat for all candidates]

## Coverage Map
[Brief note on how the candidate list spans the design axes and scope range from explore.md]
```

After writing, print: "Ideation complete — <N> candidates generated. Proceed to evaluation? (yes / no / re-run ideate)"
Wait for confirmation.

---

## Step 4 — Evaluate

Goal: score and rank candidates objectively.

Read docs/research/<slug>/explore.md and docs/research/<slug>/ideate.md.

Score each candidate on 5 axes (1–5, higher is always better):
- **Engine Value** (weight 0.25): Improves Dia module reusability or capability
- **Game Value** (weight 0.20): Improves CluicheTest as a demo or testbed
- **Implementation Cost** (weight 0.25): Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk** (weight 0.15): Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit** (weight 0.15): Aligns with module structure and PD-001 through PD-007

Weighted total = (Engine×0.25) + (Game×0.20) + (Cost×0.25) + (Risk×0.15) + (Fit×0.15)

Write docs/research/<slug>/evaluate.md using this structure:

```
# Research: Evaluate — <Topic>

**Input:** docs/research/<slug>/ideate.md

## Scoring Criteria
[One-line description of each axis and its weight]

## Scores
| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| ...       | ...           | ...         | ...         | ...         | ...        | ...   |

## Top 3 Candidates

### Rank 1: <Name> (score: X.XX)
**Why:** [2–3 sentences on strengths and Cluiche relevance]
**Watch out for:** [1–2 sentences on risks or open questions]

### Rank 2: <Name> (score: X.XX)
[same]

### Rank 3: <Name> (score: X.XX)
[same]

## Recommendation
[One paragraph naming the top candidate and why it beat the alternatives, referencing at least one PD decision]
```

Print the ranked scores table to the user after writing.
Print: "Evaluation complete. Ready to choose? (yes / no / re-run evaluate)"
Wait for confirmation.

---

## Step 5 — Choose (human-gated)

Goal: commit to one candidate. Claude must NOT write choose.md until the user explicitly confirms.

Read docs/research/<slug>/evaluate.md.

Present the following decision prompt:

```
== HUMAN DECISION REQUIRED ==

Recommended:  <Top Candidate Name>
Reason:       <one line from recommendation paragraph>

Other options:
  2. <Rank 2 name>
  3. <Rank 3 name>

Type a candidate name or number to confirm your choice.
Type 'review' to re-examine the full evaluation before deciding.
```

Wait for explicit user response.

- If user types "review": re-read evaluate.md, print the full Top 3 section, then re-present the decision prompt above.
- If user names or numbers a candidate: record the choice and ask any clarifying questions needed for Pre-Spec Commitments (e.g. "Any constraints you want carried into the spec?").
- Do NOT write choose.md for any other response — ask the user to confirm explicitly.

Once confirmed, write docs/research/<slug>/choose.md:

```
# Research: Choice — <Topic>

**Date:** YYYY-MM-DD
**Chosen candidate:** <Name>

## Rationale
[Why this candidate was chosen — user's stated reason plus top evaluation factors]

## What Was Ruled Out
| Candidate | Reason not chosen |
|-----------|------------------|
| ... | ... |

## Pre-Spec Commitments
[Any constraints or directions the user stated at choice time, to carry into the spec]

## Next Step
Run /spec-feature or /spec-system with this candidate as input.
Suggested parent system: <likely home module>
```

Print: "Choice recorded. Generate summary findings? (yes / no)"
Wait for confirmation.

---

## Step 6 — Findings

Goal: produce a clean one-page summary that can be attached to a future spec.

Read all four prior files: explore.md, ideate.md, evaluate.md, choose.md.

Write docs/research/<slug>/summary.md:

```
# Research Summary — <Topic>

**Session folder:** docs/research/<slug>/
**Date:** YYYY-MM-DD

## One-Line Answer
[The chosen candidate and its core value in one sentence]

## Journey
1. **Explored:** [2-sentence summary of what the survey found]
2. **Ideated:** [X candidates generated; range of scope covered]
3. **Evaluated:** [Top scorer and key differentiator]
4. **Chose:** [Confirmed candidate and user's stated reason]

## Chosen Work Item
**Name:** <Candidate Name>
**Home module:** <e.g., DiaRigidBody2D>
**Suggested spec type:** Feature / System / Application
**Estimated size:** S / M / L / XL

## Key Insights from Exploration
[3–6 bullet points — tradeoffs to watch for, constraints already decided, surprises]

## Discarded Candidates
| Candidate | Why discarded |
|-----------|--------------|
| ... | ... |

## References
- docs/research/<slug>/explore.md
- docs/research/<slug>/ideate.md
- docs/research/<slug>/evaluate.md
- docs/research/<slug>/choose.md
```

Print: "Summary written to docs/research/<slug>/summary.md"
Print: "Next step: /spec-feature or /spec-system — attach summary.md as context."

---

## Cluiche Context (read before generating any content)

Engine modules available:
- DiaCore — containers, type system, CRC, memory, logging, JSON
- DiaMaths — vectors, matrices, shapes (pure linear algebra, no deps)
- DiaGeometry2D — 2D shapes, intersection, Grid/Quadtree/BVH
- DiaGraphics — ICanvas, Frame, rendering abstraction
- DiaWindow — Win32 window management
- DiaInput — keyboard, mouse, event queue
- DiaUI / DiaUIAwesomium / DiaUICEF / DiaUIUltralight — UI systems
- DiaApplicationFlow — ProcessingUnit, Phase, Module lifecycle
- DiaAPI — command/plugin framework (DiaAPI plugins)
- DiaSFML — SFML integration layer
- DiaLogger — logging channels and sinks
- DiaRigidBody2D — 2D rigid-body physics
- DiaSoftBody2D — 2D soft-body physics (PBD)
- DiaWebSocket — WebSocket server/client
- DiaEditor — editor plugin framework
- DiaPython — embedded Python scripting

Applications:
- CluicheTest — demo game + engine testbed (3 ProcessingUnits: Main, Render, Sim)
- CluicheEditor — plugin-based editor (DiaEditor + CEF UI)
- GoogleTests — unit test suite

Binding platform constraints (always check against new ideas):
- PD-001: Use StringCRC for identifiers, not raw strings
- PD-002: Use ProcessingUnit/Phase/Module pattern for application structure
- PD-003: Use Component system (IComponent/IComponentObject) for entities
- PD-004: No STL containers in public APIs — use DiaCore containers
- PD-005: x64 Windows only
- PD-007: C++20 required (/std:c++20)

For every candidate ask: "Which Dia module is its natural home?" and "Does it comply with PD-001 through PD-007?"
