---
name: research-map
description: Run the map stage of a research session — interactive loop to understand, question, and refine before committing to map.md
tags: [research, map, planning]
user_invocable: true
agent_invocable: false
---

# Research: Map Stage

Build understanding of the idea space through conversation, then commit to `map.md` when the user is satisfied.

`agent_invocable: false` — this stage is an interactive loop; it cannot be driven by an agent.

## Usage

```
/research-map <topic_slug>
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

If no slug is provided:
- List all subfolders in `docs/research/`
- Ask: "Which research session do you want to map?"

Check that `docs/research/<slug>/explore.md` exists. If it does not:
- Print: "explore.md not found for docs/research/<slug>/. Run /research-explore <slug> first."
- Stop.

If `docs/research/<slug>/map.md` already exists:
- Warn: "map.md already exists for this session. Regenerate? (yes/no)"
- Wait for confirmation before continuing.

### 2. Read Context

Read silently before generating anything:
- `docs/research/<slug>/explore.md`
- @.claude/steering/structure.md (to verify module landscape)

### 3. Present a Draft Map

Build and present the map **inline in the conversation** — do not write map.md yet. Use plain conversational prose with the tree structure embedded. Keep it readable: short descriptions, clear labels, no unnecessary detail.

Identify and structure the idea space into three node types:

- **Baseline** — must come first; nothing else makes sense without it
- **Fork** — two or more approaches that are incompatible or redundant; pick exactly one
- **Stack** — an optional extension that builds on a prior node; both can coexist

Rules:
- Every node must have a named Dia module home
- Every node must comply with PD-001 through PD-007
- Size each node: S (≤1 week), M (1–3 weeks), L (1–2 months), XL (>2 months)
- A fork's options must be genuinely incompatible — if two ideas can coexist, they're a stack not a fork
- Stacks only appear under the node they depend on

After the tree, add exactly one **Wildcard** — a far-out idea that contrasts with the sensible options. Different in kind: different architecture, paradigm, or scope. Pros/cons only — no module placement, no PD check. Make clear that if the user wants to pursue it, they can start a new research session using the wildcard as the topic and `summary.md` from this session as seed context.

End the presentation with:
```
Does this map make sense? Ask me anything — about any node, why it's a fork vs stack,
what's been left out, or anything from explore.md you want to revisit.
Type 'done' when you're happy and I'll write map.md.
Type 'back to explore' if we need to go back and re-survey first.
```

### 4. Run the Conversation Loop

Stay in this loop until the user types 'done' or 'back to explore':

**If the user asks a question:**
- Answer it directly and concisely
- If the answer changes the map, update the relevant part of the map inline and say what changed

**If the user suggests a change:**
- Apply it, re-present the affected part of the map, and confirm the change

**If the user types 'back to explore':**
- Print: "Going back to explore. Run /research-explore <slug> to re-survey, then return here."
- Stop.

**If the user types 'done':**
- Proceed to Step 5.

### 5. Write map.md

Write `docs/research/<slug>/map.md` using the final agreed map:

```markdown
# Research: Map — <Topic>

**Input:** docs/research/<slug>/explore.md
**Session date:** YYYY-MM-DD

## Decision Map

[Node name] [BASELINE] — Size: S/M/L/XL
**Module:** <Dia module>
**Description:** [1–2 sentences]

  ├─ [Option A] [FORK] — Size: S/M/L/XL
  │  **Module:** <Dia module>
  │  **Description:** [1–2 sentences]
  │  **Why fork vs Option B:** [1 sentence]
  │
  │    └─ [Extension X] [STACK] — Size: S/M/L/XL
  │       **Module:** <Dia module>
  │       **Description:** [1–2 sentences]
  │       **Depends on:** Option A
  │
  └─ [Option B] [FORK] — Size: S/M/L/XL
     **Module:** <Dia module>
     **Description:** [1–2 sentences]
     **Why fork vs Option A:** [1 sentence]

       └─ [Extension Y] [STACK] — Size: S/M/L/XL
          **Module:** <Dia module>
          **Description:** [1–2 sentences]
          **Depends on:** Option B

## Map Notes
[1–3 bullet points — non-obvious dependencies, constraints from explore.md that shaped the structure, decisions the user should pay attention to in choose]

---

## Wildcard: <Name>
*A far-out idea included for contrast — not part of the main map.*

**Pros:** [2–3 bullets]
**Cons:** [2–3 bullets]
**To pursue:** Start a new research session — `/research-explore <new-slug>` — and attach `docs/research/<slug>/summary.md` as seed context.
```

Print:
```
map.md written to docs/research/<slug>/
Next step: /research-choose <slug>
```

---

## Cluiche Context

Engine modules: DiaCore, DiaMaths, DiaGeometry2D, DiaGraphics, DiaWindow, DiaInput, DiaUI/Awesomium/CEF/Ultralight, DiaApplication, DiaAPI, DiaSFML, DiaLogger, DiaRigidBody2D, DiaSoftBody2D, DiaWebSocket, DiaEditor, DiaPython

Applications: CluicheTest (demo + testbed), CluicheEditor (plugin editor), GoogleTests

PD constraints: PD-001 StringCRC, PD-002 ProcessingUnit/Phase/Module, PD-003 Component system, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20
