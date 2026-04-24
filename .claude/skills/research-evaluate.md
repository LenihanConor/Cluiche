---
name: research-evaluate
description: Run the evaluate stage of a research session — score and rank candidates by cost, impact, risk, and Cluiche fit
tags: [research, evaluate, planning]
user_invocable: true
agent_invocable: true
---

# Research: Evaluate Stage

Score and rank candidates from an existing `ideate.md` using a weighted rubric.

## Usage

```
/research-evaluate <topic_slug>
```

## Instructions for Claude

### 1. Resolve the Session

If no slug is provided:
- List all subfolders in `docs/research/`
- Ask: "Which research session do you want to evaluate?"

Check prerequisites:
- `docs/research/<slug>/explore.md` — must exist
- `docs/research/<slug>/ideate.md` — must exist

If either is missing, report which is missing and say to run the earlier stage first. Stop.

If `docs/research/<slug>/evaluate.md` already exists:
- Warn: "evaluate.md already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read:
- `docs/research/<slug>/explore.md`
- `docs/research/<slug>/ideate.md`

### 3. Score Each Candidate

Score on 5 axes, each 1–5 (higher is always better):

| Axis | Weight | Meaning |
|------|--------|---------|
| Engine Value | 0.25 | Improves Dia module reusability or capability |
| Game Value | 0.20 | Improves CluicheTest as a demo or testbed |
| Implementation Cost | 0.25 | Inverse of effort: 5 = very cheap, 1 = very expensive |
| Risk | 0.15 | Inverse of uncertainty: 5 = well-understood, 1 = highly uncertain |
| Cluiche Fit | 0.15 | Alignment with module structure and PD-001 through PD-007 |

Weighted total = (Engine×0.25) + (Game×0.20) + (Cost×0.25) + (Risk×0.15) + (Fit×0.15)

For each axis score, briefly note in the rationale which Dia module or PD decision informed it.

### 4. Write evaluate.md

Create `docs/research/<slug>/evaluate.md`:

```markdown
# Research: Evaluate — <Topic>

**Input:** docs/research/<slug>/ideate.md

## Scoring Criteria

| Axis | Weight | What it measures |
|------|--------|-----------------|
| Engine Value | 0.25 | Improves Dia module reusability or capability |
| Game Value | 0.20 | Improves CluicheTest as demo or testbed |
| Implementation Cost | 0.25 | Inverse of effort (5 = cheap, 1 = expensive) |
| Risk | 0.15 | Inverse of uncertainty (5 = known, 1 = uncertain) |
| Cluiche Fit | 0.15 | Alignment with module structure and PD decisions |

## Scores

| Candidate | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|-----------|---------------|-------------|-------------|-------------|------------|-------|
| ...       | ...           | ...         | ...         | ...         | ...        | ...   |

## Top 3 Candidates

### Rank 1: <Name> (score: X.XX)
**Why:** [2–3 sentences — strengths, which modules benefit, Cluiche relevance]
**Watch out for:** [1–2 sentences — risks or open questions]

### Rank 2: <Name> (score: X.XX)
[same]

### Rank 3: <Name> (score: X.XX)
[same]

## Recommendation
[One paragraph naming the top candidate, why it beat the alternatives,
referencing at least one specific PD decision and one Dia module by name]
```

### 5. Report

Print the full scores table inline for the user.
Print:
```
evaluate.md written to docs/research/<slug>/
Next step: /research-choose <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20
