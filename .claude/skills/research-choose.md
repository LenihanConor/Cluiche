---
name: research-choose
description: Human-gated choice stage — resolve forks, pick outcome type, write choose.md + summary.md + backlog entry
tags: [research, choose, findings, planning]
user_invocable: true
agent_invocable: false
---

# Research: Choose + Findings Stage

Commit to a path (human-gated), assign an outcome type, and write the backlog entry.

`agent_invocable: false` — this step requires a human in the loop. Claude must not auto-commit.

## Usage

```
/research-choose <topic_slug>
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
- Ask: "Which research session are you choosing for?"

Check that `docs/research/<slug>/map.md` exists. If not:
- Print: "map.md not found. Run /research-map <slug> first."
- Stop.

If `docs/research/<slug>/choose.md` already exists:
- Warn: "A choice has already been recorded for this session. Override? (yes/no)"
- Wait for confirmation before continuing.

### 2. Read the Map

Read `docs/research/<slug>/map.md`.

### 3. Resolve the Path

Walk the map with the user. They need to resolve every FORK and decide which STACKs to include. Present:

```
== PATH DECISION ==

Walk the map and confirm your choices:
- For each FORK: which option do you choose?
- For each STACK on your chosen path: include or skip?

Type 'review' to see the full map first.
Type 'nothing' if the research concluded with no action.
```

**Do NOT write anything until the path is explicitly confirmed.**

- If user types "review": print map.md verbatim, then re-present the prompt.
- If user types "nothing": skip to Step 3b (Nothing outcome).
- If user types "wildcard": skip to Step 3c (Wildcard outcome).
- If user states their path: confirm it back to them explicitly. Ask: "Any constraints or directions to carry forward?" Record as Pre-Spec Commitments.
- For any other response: ask the user to confirm explicitly.

### 3b. Nothing Outcome

If the user chose nothing:
- Ask: "What's the reason for stopping?" Record the answer.
- Skip Steps 4–5. Go straight to Step 6 (backlog entry) using the Nothing outcome type.

### 3c. Wildcard Outcome

If the user chose the wildcard:
- Read the Wildcard section from `map.md`.
- Print:
  ```
  To pursue the wildcard, start a new research session with it as the topic.
  The current session's summary will seed it with context.

  Suggested next step:
    /research-explore <wildcard-slug>
  Attach docs/research/<slug>/summary.md as context when you run it.
  ```
- Write a choose.md recording the wildcard decision (use the wildcard name as the chosen path, outcome type = More research).
- Write the backlog entry under Spec Work Needed: "Follow-on research session — wildcard from <slug>. Seed with docs/research/<slug>/summary.md."
- Stop. Do not generate summary.md.

### 4. Assign Outcome Type

Once the path is confirmed, ask:

```
What's the outcome of this research? Pick one:

  1. More research  — another research session on a deeper sub-topic
  2. Spike          — small throwaway prototype to resolve a technical unknown
  3. New spec       — /spec-feature or /spec-system for net-new work
  4. Feature add    — extend scope of an existing approved spec
  5. Refactor       — structural code change, no new behaviour
```

Wait for confirmation. If the user picks "Feature add", ask which existing spec it targets.

### 5. Write choose.md

Write `docs/research/<slug>/choose.md`:

```markdown
# Research: Choice — <Topic>

**Date:** YYYY-MM-DD
**Outcome type:** More research / Spike / New spec / Feature add / Refactor / Nothing

**Chosen path:**
- Fork decisions: [list each fork and which option was chosen]
- Stacks included: [list which stacks are in scope]
- Stacks deferred: [list which stacks were skipped and why]

## Rationale
[Why this path was chosen — user's stated reason plus key map factors]

## What Was Ruled Out
| Node | Reason not chosen |
|------|------------------|
| ... | ... |

## Pre-Spec Commitments
[Constraints or directions to carry forward. If none, write "None stated."]

## Next Step
[Specific next action based on outcome type — see below]
```

**Next Step text by outcome type:**
- More research: `Run /research-explore <new-slug> to start a deeper session on <sub-topic>.`
- Spike: `Create a throwaway spike for <specific unknown>. Timebox: <S/M/L>.`
- New spec: `Run /spec-feature or /spec-system. Attach summary.md as context. Suggested parent: <module>.`
- Feature add: `Amend [spec name](path/to/spec.md) to include <scope extension>.`
- Refactor: `Refactor <target> — no new behaviour. Consider /spec-feature if scope grows.`
- Nothing: `Research concluded. No action. Reason: <user's stated reason>.`

Print: "Choice recorded. Generate summary? (yes / no)"
Wait for confirmation.

### 6. Write Backlog Entry

Write the backlog entry **before** generating the summary.

**For all outcome types except Nothing** — append to `docs/BACKLOG.md` under the appropriate section:

- More research → **Spec Work Needed**, noting it's a follow-on research session
- Spike → **Loose Ends**
- New spec → **Spec Work Needed**
- Feature add → **Loose Ends**, referencing the target spec
- Refactor → **Loose Ends**

Entry format (match the table style of the existing section):

```
| <Title> | Research: docs/research/<slug>/summary.md | <one-line description of what to do next> |
```

**For Nothing** — append to `docs/BACKLOG-HISTORY.md` under **Superseded Items**:

```
| <Topic> | Research concluded, no action — <user's stated reason>. Session: docs/research/<slug>/ |
```

### 7. Generate summary.md

Only if the user confirmed in Step 5, read all prior files and write `docs/research/<slug>/summary.md`:

```markdown
# Research Summary — <Topic>

**Session folder:** docs/research/<slug>/
**Date:** YYYY-MM-DD
**Outcome:** <outcome type>

## One-Line Answer
[The chosen path and its core value in one sentence. If Nothing: why the research concluded without action.]

## Journey
1. **Explored:** [2-sentence summary of what the survey found]
2. **Mapped:** [key forks identified, number of nodes, scope range covered]
3. **Chose:** [confirmed path, outcome type, and user's stated reason]

## Chosen Work Item
*(Omit this section for Nothing and More Research outcomes)*
**Name:** <name>
**Home module:** <e.g., DiaRigidBody2D>
**Suggested spec type:** Feature / System / Application
**Estimated size:** S / M / L / XL (baseline) + [stacks if included]

## Key Insights from Exploration
[3–6 bullet points — tradeoffs to watch for, constraints already decided, surprises]

## Deferred Work
| Node | Why deferred |
|------|-------------|
| ... | ... |

## References
- docs/research/<slug>/explore.md
- docs/research/<slug>/map.md
- docs/research/<slug>/choose.md
```

Print:
```
Summary written to docs/research/<slug>/summary.md
Backlog updated: docs/BACKLOG.md  (or BACKLOG-HISTORY.md for Nothing)
```
