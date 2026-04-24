---
name: research-choose
description: Human-gated choice stage — commit to a candidate and write choose.md and summary.md
tags: [research, choose, findings, planning]
user_invocable: true
agent_invocable: false
---

# Research: Choose + Findings Stage

Commit to a candidate (human-gated) and generate the final summary.

`agent_invocable: false` — this step requires a human in the loop. Claude must not auto-commit.

## Usage

```
/research-choose <topic_slug>
/research-choose <topic_slug> <candidate_name_or_number>
```

## Instructions for Claude

### 1. Resolve the Session

If no slug is provided:
- List all subfolders in `docs/research/`
- Ask: "Which research session are you choosing for?"

Check that `docs/research/<slug>/evaluate.md` exists. If not:
- Print: "evaluate.md not found. Run /research-evaluate <slug> first."
- Stop.

If `docs/research/<slug>/choose.md` already exists:
- Warn: "A choice has already been recorded for this session. Override? (yes/no)"
- Wait for confirmation before continuing.

### 2. Read the Evaluation

Read `docs/research/<slug>/evaluate.md`.

### 3. Present the Human Decision Prompt

If the user already named or numbered a candidate in their invocation, confirm it:
```
You selected: <Candidate Name>
Is that correct? (yes / no / review)
```

Otherwise, present:
```
== HUMAN DECISION REQUIRED ==

Recommended:  <Top Candidate Name from evaluate.md>
Reason:       <One line from Recommendation paragraph>

Other options:
  2. <Rank 2 name>
  3. <Rank 3 name>

Type a candidate name or number to confirm your choice.
Type 'review' to re-examine the full evaluation before deciding.
```

**Do NOT write choose.md for any response other than explicit confirmation.**

- If user types "review": re-read evaluate.md, print the full Top 3 section verbatim, then re-present the decision prompt.
- If user types a number or name: accept it as the choice. Ask: "Any constraints or directions to carry into the spec?" Record the response as Pre-Spec Commitments.
- For any other response: ask the user to confirm explicitly.

### 4. Write choose.md

Only after explicit user confirmation, write `docs/research/<slug>/choose.md`:

```markdown
# Research: Choice — <Topic>

**Date:** YYYY-MM-DD
**Chosen candidate:** <Name>

## Rationale
[Why this candidate was chosen — user's stated reason plus the top evaluation factors that supported it]

## What Was Ruled Out
| Candidate | Reason not chosen |
|-----------|------------------|
| ... | ... |

## Pre-Spec Commitments
[Any constraints or directions the user stated at choice time, to carry into the spec.
If none stated, write "None stated."]

## Next Step
Run /spec-feature or /spec-system with this candidate as input.
Suggested parent system: <likely home module from evaluate.md>
```

Print: "Choice recorded. Generate summary findings? (yes / no)"
Wait for confirmation.

### 5. Generate summary.md

Only if the user confirms, read all four prior files and write `docs/research/<slug>/summary.md`:

```markdown
# Research Summary — <Topic>

**Session folder:** docs/research/<slug>/
**Date:** YYYY-MM-DD

## One-Line Answer
[The chosen candidate and its core value in one sentence]

## Journey
1. **Explored:** [2-sentence summary of what the survey found]
2. **Ideated:** [X candidates generated; range of scope covered]
3. **Evaluated:** [Top scorer and the key differentiator that won]
4. **Chose:** [Confirmed candidate and user's stated reason]

## Chosen Work Item
**Name:** <Candidate Name>
**Home module:** <e.g., DiaRigidBody2D>
**Suggested spec type:** Feature / System / Application
**Estimated size:** S / M / L / XL

## Key Insights from Exploration
[3–6 bullet points — tradeoffs to watch for, constraints already decided, surprises from the survey]

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

Print:
```
Summary written to docs/research/<slug>/summary.md
Next step: /spec-feature or /spec-system — attach summary.md as context.
```
