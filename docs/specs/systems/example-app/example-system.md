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

