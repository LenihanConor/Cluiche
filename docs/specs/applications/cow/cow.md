# Application Spec: CoW (Crucible of Wings)

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose
CoW is a game built on top of Dia and CluicheGameBaseline, developed as its own application sibling to CluicheTest and CluicheEditor. This is a starting stub, not an interviewed spec — Purpose, Systems, and Personas should be filled in properly via `/spec-app` once there's a first vertical slice to describe.

## Systems
<!-- List all systems within this application. -->
| System | Description | Spec |
|--------|-------------|------|
| | | |

## Application-Specific Architecture

CoW work follows a four-tier dependency model, checked for every capability CoW needs:

1. **Dia** — generic engine capability, any game could use it (rendering, physics, ECS, math).
2. **CluicheGameBaseline** — generic gameplay infrastructure any Cluiche game needs (e.g. `Scene2DModule`), but not engine-level.
3. **CoW-Shared** (`Cluiche/CoW/Shared/`) — specific to CoW's own content/mechanics, reused across more than one CoW system.
4. **Feature-local** — specific to a single CoW feature; stays where it's used.

Tiers 1 and 2 are outside CoW's edit zone (see Decisions below); tiers 3 and 4 are built directly as part of CoW work.

## Platform Dependencies
- [x] Dia (engine)
- [x] CluicheGameBaseline (shared gameplay infra)
- [ ] Other:

## Out of Scope
- Direct edits to Dia, CluicheGameBaseline, CluicheTest, or CluicheEditor from CoW work — these are proposed, not implemented, from within CoW (see AD-001).

## Key Users / Personas
<!-- To be filled in via /spec-app once CoW has a first vertical slice. -->

## Decisions
<!-- Decisions specific to this application. Binding decisions cascade to all systems and features within it.
     AI: Always check parent platform decisions (Cluiche.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | CoW develops on a dedicated `cow` branch/worktree carrying a tracked `.claude/settings.json` `ask` permission rule for `Edit`/`Write`/`NotebookEdit` on `Dia/**`, `Cluiche/CluicheGameBaseline/**`, `Cluiche/CluicheTest/**`, `Cluiche/CluicheEditor/**`, their spec trees, and shared build/platform files. | Guarantees no engine or shared-layer change lands without explicit human sign-off, enforced by the permission layer rather than relying on convention alone. | This app | Accepted | Yes |
| AD-002 | Every cross-cutting need CoW hits is classified against the four-tier model (Dia / CluicheGameBaseline / CoW-Shared / Feature-local) before implementation. Blocking needs are flagged immediately for a decision; non-blocking needs are logged to `dia-needs.md` for batch review. | Prevents CoW-only mechanics leaking into shared layers, and prevents genuinely reusable code getting stranded inside CoW; matches how urgent a need actually is instead of stopping on every ask. | This app | Accepted | Yes |

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
| 1 | Systems | What is CoW's first system / vertical slice? | TBD |
| 2 | Platform Dependencies | Does CoW need new CluicheGameBaseline capability day one, or can it start purely on existing Dia + baseline APIs? | TBD |

## Status
`Draft`
