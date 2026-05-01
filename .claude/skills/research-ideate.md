---
name: research-ideate
description: Run the ideate stage of a research session — generate 5–10 candidate features from explore findings
tags: [research, ideate, planning]
user_invocable: true
agent_invocable: true
---

# Research: Ideate Stage

Generate a bounded list of concrete candidate features or experiments from an existing `explore.md`.

## Usage

```
/research-ideate <topic_slug>
```

## Instructions for Claude

### 1. Resolve the Session

If no slug is provided:
- List all subfolders in `docs/research/`
- Ask: "Which research session do you want to ideate for?"

Check that `docs/research/<slug>/explore.md` exists. If it does not:
- Print: "explore.md not found for docs/research/<slug>/. Run /research-explore <slug> first."
- Stop.

If `docs/research/<slug>/ideate.md` already exists:
- Warn: "ideate.md already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read:
- `docs/research/<slug>/explore.md`
- @.claude/steering/structure.md (to verify module landscape)

### 3. Generate Candidates

Produce 5–10 concrete candidates. For each:

- It must have a clear home in the Dia module hierarchy (existing module or a named new one)
- It must be buildable as a Cluiche feature
- It must comply with PD-001 through PD-007
- Size it as S (≤1 week), M (1–3 weeks), L (1–2 months), or XL (>2 months)
- The set should span at least 3 different sizes — avoid all-small or all-large lists
- Primary value must clearly state what changes for the player or for the engine

### 4. Write ideate.md

Create `docs/research/<slug>/ideate.md` using this exact structure:

```markdown
# Research: Ideate — <Topic>

**Input:** docs/research/<slug>/explore.md

## Candidates

### Candidate 1: <Name>
**Home module/system:** <e.g., DiaRigidBody2D / new DiaSomething>
**Size:** S / M / L / XL
**Description:** [1–2 paragraphs]
**Primary value:** [One sentence]

[Repeat for each candidate]

## Coverage Map
[Brief note on how the candidates span the design axes and scope range from explore.md]
```

### 5. Report

Print:
```
ideate.md written to docs/research/<slug>/ — <N> candidates generated.
Next step: /research-evaluate <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20

For every candidate: "Which Dia module is its natural home?" and "Does it comply with PD-001 through PD-007?"
