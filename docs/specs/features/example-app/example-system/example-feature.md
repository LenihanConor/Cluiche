# Feature Spec: [Feature Name]

## Traceability
| Level | Name | Link |
|-------|------|------|
| Platform | [Platform Name] | @docs/specs/platform/Cluiche.md |
| Application | [App Name] | @docs/specs/applications/example-app.md |
| System | [System Name] | @docs/specs/systems/example-app/example-system.md |
| Feature | **This file** | — |

## Purpose
<!-- One sentence: what does this feature do? -->

## Requirements
<!-- Acceptance criteria. Each item should be testable. -->
- [ ] 
- [ ] 
- [ ] 

## Design

### Data Models


### API / Interface


### UI Flow (if applicable)


### File Boundary Plan
<!-- Which files/directories does this feature touch? Keeps subagent scope clear. -->
| File / Directory | Change Type |
|-----------------|-------------|
| src/... | Create / Modify |

## Tasks
<!-- Implementation steps. Each task = one subagent scope. -->
- [ ] `task-01` — _Boundary: src/..._ 
- [ ] `task-02` — _Boundary: src/..._ _Depends: task-01_
- [ ] `task-03` — Write tests _Depends: task-02_

## Binding Decisions Compliance
<!-- AI: Populate this by reading ALL parent specs in the chain (Platform → App → System).
     For every Binding=Yes decision found, add a row explaining how this feature
     complies with it. If the feature cannot comply, flag it as a CONFLICT.
     This section must be complete before status can move to Approved. -->

| Decision ID | Source | Decision Summary | How this feature complies | Status |
|-------------|--------|-----------------|--------------------------|--------|
| PD-001 | Platform | | | ✅ Compliant / ⚠️ Conflict / ⬜ TBD |
| AD-001 | App | | | ✅ Compliant / ⚠️ Conflict / ⬜ TBD |
| SD-001 | System | | | ✅ Compliant / ⚠️ Conflict / ⬜ TBD |

## Open Questions
<!-- Unresolved decisions the human must answer before implementation starts. -->
- 

## AI Review Questions
<!--
  AI: After reading the full spec chain (Platform → App → System → this feature),
  list every question that must be answered before tasks can be implemented.
  Consider:
  - Requirements that are ambiguous or untestable
  - Design gaps (missing model fields, unclear API contracts)
  - File boundary conflicts with other features
  - Task dependencies that are unclear or circular
  - Anything that contradicts a parent spec
  - Edge cases not covered by the requirements
  - Any Binding Decisions not yet marked Compliant above

  For each question, suggest a default answer if you have enough context.
  Format: | # | Section | Question | Suggested Default | Answer |
-->

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | | | | |

## Status
`Draft` | `Approved` | `In Progress` | `Done`

