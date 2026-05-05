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

### 0. Check Model

Check which model is active. If it is not `claude-opus-4-7`, print:

```
Research sessions should run on Opus for best results.
Switch with: /model claude-opus-4-7
Then re-run this command.
```

And stop. Do not proceed until the user confirms they are on Opus or explicitly says to continue anyway.

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

### 3. Present a Draft Survey and Open a Conversation

Build the survey and present it **inline in the conversation** — do not write explore.md yet. The goal is to make sure you and the user are seeing the problem the same way before committing anything.

Cover:

- **Existing approaches**: Patterns and techniques from the industry or literature relevant to this topic
- **Design axes**: The key dimensions of variation (e.g., discrete vs continuous, push vs pull, data-driven vs code-driven)
- **Known tradeoffs**: What you gain and lose with each approach
- **Known pitfalls**: Mistakes that come up repeatedly in C++ game engine implementations of this topic
- **Cluiche-specific opportunities**: How the topic maps to existing Dia modules, what gaps exist, what PD constraints apply

End the presentation with:
```
Does this feel like the right problem? Any axes I've missed, approaches that seem wrong,
or Cluiche constraints I haven't accounted for?
Type 'done' when you're happy and I'll write explore.md.
```

### 4. Run the Conversation Loop

Stay in this loop until the user types 'done':

**If the user asks a question or pushes back:**
- Engage directly — this is a conversation about whether we're framing the problem correctly
- If the answer changes the survey, update the relevant section inline and say what changed
- If the user identifies a missing axis or approach, add it and re-present that section

**If the user types 'done':**
- Proceed to Step 5.

### 5. Write explore.md

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

## Open Questions for Mapping
[Bulleted list — unresolved questions that should shape the map]
```

### 6. Report

Print:
```
explore.md written to docs/research/<slug>/
Next step: /research-map <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20
