# Feature Spec: Editor UI Framework Convention

## Status
`Approved`

## Traceability

| Level | Spec |
|-------|------|
| Platform | @docs/specs/platform/Cluiche.md |
| Application | @docs/specs/applications/dia.md |
| System | @docs/specs/systems/dia/diauicef.md |
| Related | @docs/specs/features/dia/diauiultralight/game-ui-framework-convention.md |

## Problem Statement

There is no documented decision locking in React + TypeScript as the canonical framework for CluicheEditor UI, leaving the door open for future developers or AI to introduce conflicting frameworks (Vue, Alpine, Svelte, Webix, etc.) into the editor stack.

## Summary

This is a **decision record** — not an implementation task. The CluicheEditor UI stack already exists and works. This spec documents it as the canonical, fixed convention so it is visible to all developers and AI agents working in this codebase, and establishes that changing the stack requires an explicit spec decision.

**The editor UI stack is fixed as:**
- **React 18** + **TypeScript 5** + **Vite 4**
- **react-mosaic-component** for multi-panel dockable layout
- **fuse.js** for fuzzy search
- **@testing-library/react** + **vitest** for component tests
- Output: compiled to `Cluiche/CluicheEditor/UI/dist/`, deployed to `Cluiche/bin/{Config}/x64/diaapplicationeditor/`
- Bridge: `window.dia.sendMessage` / `window.dia.onMessage` (DiaUICEF UCEF-004)

No alternative framework may be introduced into the editor without a new spec decision superseding this one.

## Acceptance Criteria

1. React 18 + TypeScript + Vite documented as the canonical and only permitted framework for all CluicheEditor UI panels
2. Any alternative framework (Vue, Alpine, Svelte, Webix, or any other) explicitly prohibited in the editor stack without a superseding spec decision
3. Existing CluicheEditor Vite project structure documented — location, build command, output directory, entry points
4. AI authoring convention documented — prompt conventions and patterns for generating editor UI with React + TypeScript
5. Decision recorded that this stack is fixed; changing it requires an explicit spec decision

## Existing Project Structure

| Item | Value |
|------|-------|
| Project root | `Cluiche/CluicheEditor/UI/` |
| Entry point | `Cluiche/CluicheEditor/UI/src/` |
| Build command | `tsc && vite build` |
| Dev server | `vite` (not usable inside CEF; for browser-based development only) |
| Test command | `vitest run` |
| Output directory | `Cluiche/CluicheEditor/UI/dist/` |
| Deployed to | `Cluiche/bin/{Config}/x64/diaapplicationeditor/` |
| Config file | `Cluiche/CluicheEditor/UI/vite.config.ts` |
| Package file | `Cluiche/CluicheEditor/UI/package.json` |

### Key Dependencies

| Package | Version | Purpose |
|---------|---------|---------|
| react | ^18.2.0 | UI framework |
| react-dom | ^18.2.0 | DOM rendering |
| typescript | ^5.0.0 | Type safety |
| vite | ^4.4.0 | Build tool |
| react-mosaic-component | ^6.1.0 | Dockable multi-panel layout |
| fuse.js | ^7.0.0 | Fuzzy search |
| shadcn/ui | latest | Component primitives (buttons, forms, tables, dialogs) — recommended for new editor UI |
| tailwindcss | ^3 | Utility CSS framework — required by shadcn/ui; consistent with game tier |
| vitest | ^2.1.9 | Test runner |
| @testing-library/react | ^16.3.2 | Component testing |

## AI Authoring Conventions

When generating or modifying CluicheEditor UI code:

1. **Always use TypeScript** — `.tsx` for components, `.ts` for utilities. No `.js` or `.jsx` files.
2. **Functional components only** — no class components.
3. **React hooks** — `useState`, `useEffect`, `useContext`, `useCallback`, `useMemo`. No legacy lifecycle methods.
4. **C++ bridge** — use `window.dia.sendMessage(name, data)` and `window.dia.onMessage(name, callback)` for all C++ communication. Never access `window.app` (that is the Ultralight game bridge).
5. **No new frameworks** — do not introduce Vue, Alpine, Svelte, Preact, or any other UI framework. Raise a spec decision if a new dependency is needed.
6. **Component tests** — use `@testing-library/react` + vitest for all new components.
7. **Build before deploy** — run `npm run build` from `Cluiche/CluicheEditor/UI/`; output lands in `dist/` and is copied to the binary directory.

## Prohibited Frameworks

The following are explicitly prohibited in the editor stack without a superseding spec decision:

- Vue (any version)
- Alpine.js
- Svelte / SvelteKit
- Preact / HTM
- Webix (also GPL — double prohibition)
- Angular
- Any other non-React UI framework

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Write `docs/guides/editor-ui-authoring.md` with React + TypeScript conventions and AI prompt templates | Not Started | Document bridge pattern, component structure, test pattern |
| 2 | Register feature in DiaUICEF system spec features table | Not Started | |

## Files This Feature Touches

| Path | Change |
|------|--------|
| `docs/specs/systems/dia/diauicef.md` | Feature registered in features table |
| `docs/guides/editor-ui-authoring.md` | New — authoring guide with conventions and AI prompt templates |

## Known Open Questions

None — this is a documentation and decision record. No implementation work required.

## Binding Decisions Compliance

| Source | ID | Decision | Compliance |
|--------|----|----------|------------|
| Platform | PD-001 | Use StringCRC for all entity/component IDs | Compliant — StringCRC used on C++ side of bridge; TypeScript layer uses plain strings for display only |
| Platform | PD-002 | ProcessingUnit/Phase/Module pattern | Compliant — editor UI panels are assets loaded by existing DiaEditor framework; no new processing units |
| Platform | PD-003 | Component-based entities | Compliant — this spec is JS/TS convention only; no entity architecture changes |
| Platform | PD-004 | No STL in public APIs | Compliant — C++ bridge code uses DiaCore types; TypeScript layer is unaffected |
| Platform | PD-005 | x64 Windows only | Compliant — CEF x64 Windows build; React/TypeScript runs inside CEF's Chromium renderer |
| Platform | PD-006 | VS project files as source of truth | Compliant — HTML/JS/CSS build output is deployed as assets, not compiled sources; `.vcxproj` not modified |
| Platform | PD-007 | C++20 required | Compliant — no new C++ code; TypeScript layer is independent of C++ standard |
| System | UCEF-001 | Windowed rendering for editors | Compliant — this spec documents the JS framework only; rendering mode is a C++ concern |
| System | UCEF-004 | Message passing via `window.dia` object | Compliant — all C++ bridge calls use `window.dia.sendMessage` / `window.dia.onMessage`; this is mandated in AI authoring conventions above |
| System | UCEF-006 | DevTools enabled in Debug builds only | Compliant — React dev tools and Vite sourcemaps available in Debug; no impact on Release |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Stack version | Should the spec pin exact versions (React 18.2.0) or allow minor bumps (React ^18)? | Resolved — spec pins major versions only (React 18, TypeScript 5, Vite 4). Exact versions are `package-lock.json`'s responsibility. |
| 2 | Shadcn/ui | The game UI spec uses shadcn/ui for the React tier — should shadcn/ui also be adopted in the editor, or is react-mosaic-component sufficient? | Resolved — shadcn/ui is the recommended component library for new editor UI primitives (buttons, forms, tables, dialogs). react-mosaic-component stays for dockable panel layout. The two coexist. |
| 3 | Tailwind | The game tier uses Tailwind for styling — does the editor use Tailwind, or a different styling approach? | Resolved — Tailwind CSS adopted in the editor. Consistent with game tier; required by shadcn/ui. |
| 4 | Authoring guide | Should the editor authoring guide and game authoring guide be one document or two? | Resolved — two separate documents: `docs/guides/editor-ui-authoring.md` and `docs/guides/game-ui-authoring.md`. Different audiences, different frameworks, different C++ bridges. |
