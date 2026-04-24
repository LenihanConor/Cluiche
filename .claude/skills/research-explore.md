---
name: research-explore
description: Run the explore stage of a research session — survey the problem space and write explore.md
tags: [research, explore, planning]
user_invocable: true
agent_invocable: true
---

# Research: Explore Stage

Survey the problem space for an existing or new research session and write `explore.md`.

## Usage

```
/research-explore <topic_slug>
/research-explore <topic_slug> "<topic description override>"
```

## Instructions for Claude

### 1. Resolve the Session

If a topic slug is provided, check whether `docs/research/<slug>/` exists.

If no slug is provided:
- List all subfolders in `docs/research/`
- Ask: "Which research session do you want to run explore for? (or type a new topic)"

If `docs/research/<slug>/explore.md` already exists:
- Warn: "explore.md already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before overwriting.

### 2. Read Context

Read silently before generating anything:
- @.claude/steering/tech.md
- @.claude/steering/structure.md
- @docs/specs/platform/Cluiche.md

### 3. Survey the Problem Space

Produce a structured survey — not a brainstorm. Cover:

- **Existing approaches**: Patterns and techniques from the industry or literature relevant to this topic
- **Design axes**: The key dimensions of variation (e.g., discrete vs continuous, push vs pull, data-driven vs code-driven)
- **Known tradeoffs**: What you gain and lose with each approach
- **Known pitfalls**: Mistakes that come up repeatedly in C++ game engine implementations of this topic
- **Cluiche-specific opportunities**: How the topic maps to existing Dia modules, what gaps exist, what PD constraints apply

### 4. Write explore.md

Create `docs/research/<slug>/explore.md` using this exact structure:

```markdown
# Research: Explore — <Topic>

**Session date:** YYYY-MM-DD
**Folder:** docs/research/<slug>/

## Problem Space Overview
[2–3 paragraph survey]

## Existing Approaches
[Bulleted list]

## Design Axes
| Axis | Options | Notes |
|------|---------|-------|

## Known Tradeoffs
[Bulleted list]

## Known Pitfalls (C++ / game engine context)
[Bulleted list]

## Cluiche-Specific Opportunities

### Relevant Existing Modules
| Module | Relevance |
|--------|-----------|

### Platform Decision Constraints
| Decision | Implication for this topic |
|----------|---------------------------|

## Open Questions for Ideation
[Bulleted list — unresolved questions that should shape candidates]
```

### 5. Report

Print:
```
explore.md written to docs/research/<slug>/
Next step: /research-ideate <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20
