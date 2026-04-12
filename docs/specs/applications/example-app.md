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

