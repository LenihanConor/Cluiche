#!/usr/bin/env bash
# =============================================================================
# Spec-Driven Development Scaffold
# Platform → Application → System → Feature
# =============================================================================
# Usage:
#   chmod +x setup-specs.sh
#   ./setup-specs.sh
#
# Optional: pass a platform name as argument
#   ./setup-specs.sh "MyPlatform"
# =============================================================================

set -e

PLATFORM_NAME="${1:-Platform}"
PLATFORM_SLUG=$(echo "$PLATFORM_NAME" | tr '[:upper:]' '[:lower:]' | tr ' ' '-')

echo "🏗  Scaffolding spec structure for platform: $PLATFORM_NAME"
echo ""

# =============================================================================
# DIRECTORY STRUCTURE
# =============================================================================

mkdir -p docs/specs/platform
mkdir -p docs/specs/applications
mkdir -p docs/specs/systems
mkdir -p docs/specs/features
mkdir -p .claude/commands
mkdir -p .claude/steering

echo "✅ Directories created"

# =============================================================================
# LEVEL 1: PLATFORM SPEC
# =============================================================================

cat > docs/specs/platform/PLATFORM.md << 'EOF'
# Platform Spec

## Overview
<!-- One paragraph describing the platform: what it is, why it exists, who it serves. -->

## Applications
<!-- List each application built on this platform. -->
| Application | Description | Spec |
|-------------|-------------|------|
| example-app | Description here | [example-app.md](../applications/example-app.md) |

## Shared Codebase
<!-- Describe what the shared codebase provides. -->

### Shared Libraries / Modules
- 

### Shared Infrastructure
- Auth:
- Database:
- Deployment:
- Observability:

### Shared Design System
- 

## Architecture Principles
<!-- Decisions that apply across ALL applications. -->
1. 
2. 
3. 

## Non-Functional Requirements
| Concern | Requirement |
|---------|-------------|
| Performance | |
| Security | |
| Scalability | |
| Availability | |

## Conventions
<!-- Coding standards, naming, branching, commit style etc. that apply platform-wide. -->
See: @.claude/steering/tech.md

## Change Policy
<!-- How changes to shared code are proposed, reviewed, and communicated to app teams. -->

## Decisions
<!-- Architectural and design decisions that apply to ALL applications on this platform.
     Binding decisions cascade down and must be respected by every app, system, and feature.
     AI: When reviewing child specs, check every Binding=Yes decision is honoured. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| PD-001 | | | Platform-wide | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all children · `No` = guidance only

## AI Review Questions
<!--
  AI: Before this spec can be marked Approved, review all sections above and list
  any questions that must be answered to make this spec complete and unambiguous.
  Format each question as:
    - [ ] **[Section]** Question text
  Leave blank if no questions remain. When all boxes are checked, this spec is ready.
-->

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | | | |

EOF

echo "✅ Platform spec created"

# =============================================================================
# LEVEL 2: APPLICATION SPEC (example)
# =============================================================================

cat > docs/specs/applications/example-app.md << 'EOF'
# Application Spec: [App Name]

## Parent Platform
@docs/specs/platform/PLATFORM.md

## Purpose
<!-- What does this application do? Who is it for? -->

## Systems
<!-- List all systems within this application. -->
| System | Description | Spec |
|--------|-------------|------|
| example-system | Description here | [example-system.md](../systems/example-app/example-system.md) |

## Application-Specific Architecture
<!-- Any architectural decisions specific to this app, beyond platform defaults. -->

## Platform Dependencies
<!-- Which shared platform modules does this app use? -->
- [ ] Auth
- [ ] Database
- [ ] Design System
- [ ] Other:

## Out of Scope
<!-- What this application deliberately does NOT do. -->

## Key Users / Personas
<!-- Who uses this application and how. -->

## Decisions
<!-- Decisions specific to this application. Binding decisions cascade to all systems and features within it.
     AI: Always check parent platform decisions (PLATFORM.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | | | This app + all systems | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems and features · `No` = guidance only

## AI Review Questions
<!--
  AI: Review this application spec and list any questions that must be answered
  before systems can be designed or features built. Consider:
  - Ambiguous purpose or scope
  - Missing persona detail that would affect system design
  - Unclear platform dependency boundaries
  - Anything that contradicts the platform spec or its Binding decisions
  Format: | # | Section | Question | Answer |
-->

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | | | |

## Status
`Draft` | `Active` | `Deprecated`

EOF

mkdir -p docs/specs/systems/example-app
mkdir -p docs/specs/features/example-app/example-system

echo "✅ Application spec (example) created"

# =============================================================================
# LEVEL 3: SYSTEM SPEC (example)
# =============================================================================

cat > docs/specs/systems/example-app/example-system.md << 'EOF'
# System Spec: [System Name]

## Parent Application
@docs/specs/applications/example-app.md

## Purpose
<!-- One paragraph: what does this system own? -->

## Responsibilities
- 
- 

## Public Interfaces
<!-- APIs, events, or data contracts that other systems depend on. -->

### Endpoints / APIs


### Events Emitted


### Data Contracts


## Features
<!-- All features within this system. -->
| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| example-feature | Description | [example-feature.md](../../features/example-app/example-system/example-feature.md) | Draft |

## Platform Primitives Used
<!-- Which shared platform modules does this system consume? -->
- 

## Dependencies on Other Systems
<!-- Other systems this one calls or consumes. -->
- 

## Out of Scope
<!-- What this system deliberately does NOT handle. -->

## Decisions
<!-- Decisions specific to this system. Binding decisions cascade to all features within it.
     AI: Before adding a decision here, check parent app (AD-) and platform (PD-) decisions
     for conflicts. Use SD- prefix for system-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | | | This system + all features | Proposed | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions
<!-- AI: Populate this automatically by reading parent specs. List every Binding=Yes
     decision from Platform and Application levels that applies to this system.
     Keep in sync whenever parent specs change. -->

| ID | Source | Decision | Implication for this system |
|----|--------|----------|-----------------------------|
| PD-001 | Platform | | |
| AD-001 | App | | |

## AI Review Questions
<!--
  AI: Review this system spec and list any questions that must be answered
  before features can be designed or implemented. Consider:
  - Unclear ownership boundaries with other systems
  - Ambiguous public interfaces
  - Missing data contract detail
  - Dependency conflicts or circular dependencies
  - Anything that contradicts the parent application or platform spec
  - Any Binding decisions from parent specs not yet reflected in this system's design
  Format: | # | Section | Question | Answer |
-->

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | | | |

## Status
`Draft` | `Active` | `Deprecated`

EOF

echo "✅ System spec (example) created"

# =============================================================================
# LEVEL 4: FEATURE SPEC (example)
# =============================================================================

cat > docs/specs/features/example-app/example-system/example-feature.md << 'EOF'
# Feature Spec: [Feature Name]

## Traceability
| Level | Name | Link |
|-------|------|------|
| Platform | [Platform Name] | @docs/specs/platform/PLATFORM.md |
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

EOF

echo "✅ Feature spec (example) created"

# =============================================================================
# CLAUDE.md — ROOT (loaded every session)
# =============================================================================

cat > CLAUDE.md << EOF
# ${PLATFORM_NAME} — Claude Code Instructions

## Spec Hierarchy
This project uses four-level spec-driven development:

\`\`\`
Platform → Application → System → Feature
\`\`\`

| Level | Location | Purpose |
|-------|----------|---------|
| Platform | @docs/specs/platform/PLATFORM.md | Shared codebase, cross-app decisions |
| Application | @docs/specs/applications/ | Per-app purpose, systems list |
| System | @docs/specs/systems/<app>/ | Bounded domain within an app |
| Feature | @docs/specs/features/<app>/<system>/ | Implementation unit |

**Always read the full spec chain before implementing.** Every feature spec has a Traceability table — follow it upward to understand constraints before writing code.

## Steering Docs
- Tech standards: @.claude/steering/tech.md
- Codebase structure: @.claude/steering/structure.md

## Workflow
1. Spec must exist and be \`Approved\` before implementation starts
2. Implement features by pointing at the feature spec:
   \`implement @docs/specs/features/<app>/<system>/<feature>.md\`
3. Each task in the feature spec should be delegated to a subagent
4. Commit after each task before continuing
5. Update feature spec status as work progresses

## Custom Commands
- \`/spec-platform\` — create or update the platform spec
- \`/spec-app\` — create a new application spec
- \`/spec-system\` — create a new system spec
- \`/spec-feature\` — create a new feature spec (interview + AI review questions)
- \`/spec-review\` — review any spec and populate AI review questions
- \`/spec-trace\` — trace a feature's full lineage up to platform

EOF

echo "✅ CLAUDE.md created"

# =============================================================================
# STEERING DOCS
# =============================================================================

cat > .claude/steering/tech.md << 'EOF'
# Tech Standards

## Language & Runtime


## Frameworks


## Testing
- Framework:
- Coverage requirement:
- Pattern (TDD / test-after):

## Code Style
- Formatter:
- Linter:
- Max line length:

## Naming Conventions
| Thing | Convention | Example |
|-------|-----------|---------|
| Files | | |
| Classes | | |
| Functions | | |
| Constants | | |
| DB Tables | | |

## Git Workflow
- Branch format:
- Commit style:
- PR requirements:

EOF

cat > .claude/steering/structure.md << 'EOF'
# Codebase Structure

## Top-Level Layout
```
/
├── src/
├── docs/
├── tests/
└── ...
```

## Module / Package Structure


## Shared vs App-Specific Code
- Shared code lives in:
- App-specific code lives in:

## Key Patterns
<!-- Architecture patterns used across the codebase. -->

EOF

echo "✅ Steering docs created"

# =============================================================================
# CUSTOM SLASH COMMANDS
# =============================================================================

cat > .claude/commands/spec-platform.md << 'EOF'
Update or create the platform spec at @docs/specs/platform/PLATFORM.md.

Ask the user:
1. Has anything changed about the shared codebase or architecture?
2. Are there new applications to register?
3. Are there new non-functional requirements or conventions?

Update the spec based on their answers. Do not change sections the user did not mention.
EOF

cat > .claude/commands/spec-app.md << 'EOF'
Create a new application spec.

Ask the user:
1. What is the application name?
2. What does it do, and who is it for?
3. What are its main systems (rough list is fine)?
4. Which platform shared modules does it use?

Then:
- Create docs/specs/applications/<app-name>.md using the application spec template
- Create docs/specs/systems/<app-name>/ directory
- Create docs/specs/features/<app-name>/ directory
- Register the new application in @docs/specs/platform/PLATFORM.md applications table
EOF

cat > .claude/commands/spec-system.md << 'EOF'
Create a new system spec.

Ask the user:
1. Which application does this system belong to?
2. What is the system name?
3. What does it own / what is its responsibility?
4. What are its public interfaces (APIs, events)?
5. What other systems does it depend on?

Then:
- Create docs/specs/systems/<app-name>/<system-name>.md using the system spec template
- Register the system in the parent application spec's systems table
- Read parent Platform and App specs and populate the Inherited Binding Decisions table
  with every Binding=Yes decision that applies to this system
EOF

cat > .claude/commands/spec-feature.md << 'EOF'
Create a new feature spec. Run an interview before writing anything.

Step 1 — Interview the user:
1. Which application and system does this feature belong to?
2. What is the feature name?
3. What problem does it solve? (one sentence)
4. What are the acceptance criteria? (list until they say done)
5. Any data models or API shapes already decided?
6. Any files or modules you know it will touch?
7. Any known open questions or blockers?

Step 2 — Write the spec:
- Create docs/specs/features/<app>/<s>/<feature-name>.md
- Fill in the Traceability table with correct parent links
- Set status to Draft
- Register the feature in the parent system spec's features table

Step 3 — Decision cascade:
Read every Binding=Yes decision from all parent specs (Platform PD-, App AD-, System SD-).
For each one, populate a row in the Binding Decisions Compliance table:
  - Summarise the decision in plain language
  - Explain how this feature's design complies with it
  - If compliance is impossible or unclear, mark as CONFLICT and add to Open Questions

If any conflicts exist, surface them to the user immediately before proceeding.

Step 4 — AI Review Questions:
Read the full spec chain and populate the AI Review Questions table. For every gap,
ambiguity, conflict, or unchecked binding decision found, add a row:
  - Section: which section it relates to
  - Question: clearly stated
  - Suggested Default: your best guess if you have enough context

Present unanswered questions to the user one at a time.
Fill in the Answer column as they respond and update the spec after each answer.

Step 5 — Approval:
Once all Binding Decisions are marked Compliant, all AI Review Questions have answers,
and all Open Questions are resolved, ask:
"All decisions compliant and questions resolved. Shall I mark this spec Approved?"
EOF

cat > .claude/commands/spec-review.md << 'EOF'
Review an existing spec and populate or refresh its decisions and AI Review Questions.

The user will provide a spec path or name at any level (Platform / App / System / Feature).

1. Read the spec
2. Read all parent specs in the chain

For Platform / App / System specs:
3. Check the Decisions table for completeness — are rationale and scope filled in?
4. For each Binding=Yes decision, check whether child specs (if readable) honour it
5. Populate or refresh the AI Review Questions table with any gaps found

For Feature specs:
3. Collect ALL Binding=Yes decisions from the full parent chain
4. For each one, check or populate the Binding Decisions Compliance table
5. Flag any CONFLICT rows and add them to Open Questions
6. Populate or refresh the AI Review Questions table

Present unanswered questions to the user one at a time.
Fill in answers as they respond and update the spec file after each answer.

When all questions are answered and all binding decisions are compliant, summarise
what changed and ask if the status should be updated.
EOF

cat > .claude/commands/spec-trace.md << 'EOF'
Trace the full lineage of a feature from Platform down to implementation.

The user will provide a feature name or path.

1. Find the feature spec in docs/specs/features/
2. Read its Traceability table
3. Read the parent system spec
4. Read the parent application spec
5. Read the platform spec

Output a summary:
- Platform: [name + one-line summary]
  - Application: [name + purpose]
    - System: [name + responsibility]
      - Feature: [name + purpose + status]
        - Tasks: list with checkboxes
        - Open Questions: list if any
        - AI Review Questions: X answered / Y total

Flag any AI Review Questions across the chain that are still unanswered.

Also surface all Binding=Yes decisions from the chain and their compliance status:
  ✅ Compliant · ⚠️ Conflict · ⬜ TBD

Then answer: "Is there anything in the platform or application spec that constrains
this feature's implementation — and is the feature currently compliant with all
binding decisions?"
EOF
echo "✅ Custom slash commands created"

# =============================================================================
# DONE
# =============================================================================

echo ""
echo "=============================================="
echo "✅ Spec scaffold complete!"
echo "=============================================="
echo ""
echo "Structure created:"
echo ""
echo "  CLAUDE.md                          ← loaded every session"
echo "  docs/specs/"
echo "    platform/PLATFORM.md             ← Level 1: Platform"
echo "    applications/example-app.md      ← Level 2: Application"
echo "    systems/example-app/             ← Level 3: Systems"
echo "    features/example-app/            ← Level 4: Features"
echo "  .claude/"
echo "    steering/tech.md"
echo "    steering/structure.md"
echo "    commands/spec-platform.md"
echo "    commands/spec-app.md"
echo "    commands/spec-system.md"
echo "    commands/spec-feature.md"
echo "    commands/spec-trace.md"
echo ""
echo "Next steps:"
echo "  1. Fill in docs/specs/platform/PLATFORM.md"
echo "  2. Fill in .claude/steering/tech.md and structure.md"
echo "  3. Run /spec-app in Claude Code to add Dia, Cluiche etc."
echo "  4. Run /spec-system to add diacore and other systems"
echo "  5. Run /spec-feature to spec individual features"
echo ""
echo "  Rename 'example-app' and 'example-system' files as needed."
echo "  Delete example files once you have real ones."
echo ""
