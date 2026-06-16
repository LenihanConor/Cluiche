# System Spec: DiaEditorUI

## Parent Application
@docs/specs/applications/dia/dia.md

## Purpose

DiaEditorUI is the shared React component library for all editor plugin UIs in the Cluiche platform. It provides a single source of truth for the VS Code-style dark theme, connection status patterns, common layout primitives, and the `useBridge` hook that wires each plugin's UI to the C++ `WebUIBridge`. All five existing React+Vite editor UIs contain duplicated versions of these primitives; DiaEditorUI eliminates that duplication and enforces visual and behavioural consistency across every editor panel.

**Location:** `Dia/DiaEditorUI/` — consumed as a local npm workspace package by every editor UI.

## Responsibilities

- **Theme** — single dark-theme colour palette and style-factory helpers; `font-family: 'Segoe UI', system-ui, sans-serif` at `12px` baseline
- **TrafficLightDot** — state indicator dot (grey / amber / green / red)
- **TabBar** — horizontal tab row with active underline, keyboard navigation
- **EmptyState** — centred placeholder with icon, message, and hint text
- **ConnectionStatus** — unified connection status indicator (dot + label + optional dropdown) replacing three divergent implementations
- **useBridge** — React hook that wraps `EditorBridge.subscribe()` and `EditorBridge.request()` with React lifecycle; replaces manual `useEffect` wiring in all four editors
- **Toast / notifications** — zustand-backed toast queue with auto-dismiss; extracted from CluicheEditor and made available to all plugins

## Non-Responsibilities

- **Layout** — docking, panels, and split panes remain per-editor concerns (react-mosaic, react-resizable-panels)
- **Domain components** — flow graphs, pipeline timelines, entity tables — each editor owns these
- **C++ plugin code** — no impact on `IEditorPlugin`, `WebUIBridge`, or `GameConnectionManager`
- **Plain HTML/JS editors** — DiaEntityInspector, DiaSceneEditor, DiaAssetCatalogueEditor etc. are not migrated by this spec; they may adopt the theme variables over time

## Public Interfaces

### Package identity

```json
{
  "name": "@dia/editor-ui",
  "version": "1.0.0",
  "main": "src/index.ts"
}
```

Consumed in each editor's `package.json`:
```json
{
  "dependencies": {
    "@dia/editor-ui": "workspace:*"
  }
}
```

### Theme

```typescript
// @dia/editor-ui/src/theme.ts
export const theme = {
  bg:          '#1e1e1e',
  bgPanel:     '#252526',
  bgInput:     '#2d2d2d',
  border:      '#3c3c3c',
  borderMuted: '#555',
  text:        '#d4d4d4',
  textMuted:   '#888',
  textDim:     '#ccc',
  accent:      '#0e639c',
  accentHover: '#007acc',
  success:     '#89d185',
  warning:     '#cca700',
  error:       '#f48771',
} as const

export type Theme = typeof theme

// CSS variable injection (call once at app root)
export function injectThemeVars(): void

// Style factories
export function inputStyle(overrides?: CSSProperties): CSSProperties
export function buttonStyle(variant?: 'default' | 'primary' | 'ghost', overrides?: CSSProperties): CSSProperties
```

### TrafficLightDot

```typescript
// @dia/editor-ui/src/TrafficLightDot.tsx
export type DotState = 'grey' | 'amber' | 'green' | 'red'

export interface TrafficLightDotProps {
  state: DotState
  size?: number          // default 8
  label?: string         // optional text beside dot
}

export const TrafficLightDot: React.FC<TrafficLightDotProps>
```

### TabBar

```typescript
// @dia/editor-ui/src/TabBar.tsx
export interface Tab {
  id: string
  label: string
  count?: number         // optional badge (e.g. "Errors (3)")
}

export interface TabBarProps {
  tabs: Tab[]
  activeTab: string
  onTabChange: (id: string) => void
}

export const TabBar: React.FC<TabBarProps>
```

### EmptyState

```typescript
// @dia/editor-ui/src/EmptyState.tsx
export interface EmptyStateProps {
  message: string
  hint?: string
  icon?: React.ReactNode  // defaults to a simple placeholder glyph
}

export const EmptyState: React.FC<EmptyStateProps>
```

### ConnectionStatus

```typescript
// @dia/editor-ui/src/ConnectionStatus.tsx
export type ConnectionState = 'disconnected' | 'connecting' | 'connected' | 'error'

export interface ConnectionStatusProps {
  state: ConnectionState
  label?: string            // e.g. "localhost:7000"
  onConnect?: () => void    // show connect affordance when disconnected
  onDisconnect?: () => void
  compact?: boolean         // dot only, no label (for toolbars)
}

export const ConnectionStatus: React.FC<ConnectionStatusProps>
```

### useBridge

```typescript
// @dia/editor-ui/src/useBridge.ts
export interface BridgeSubscription {
  topic: string
  handler: (data: unknown) => void
}

// Subscribe to one or more bridge topics; auto-unsubscribes on unmount
export function useBridgeSubscribe(subscriptions: BridgeSubscription[]): void

// Send a request to the C++ bridge and await a typed response
export function useBridgeRequest<T = unknown>(): (topic: string, payload?: unknown) => Promise<T>
```

### Toast / Notifications

```typescript
// @dia/editor-ui/src/notifications/useToast.ts
export type ToastSeverity = 'info' | 'success' | 'warning' | 'error'

export interface Toast {
  id: string
  message: string
  severity: ToastSeverity
  durationMs?: number   // default 4000; 0 = sticky
}

export function useToast(): {
  push: (message: string, severity?: ToastSeverity, durationMs?: number) => void
  dismiss: (id: string) => void
}

// Render this once at app root
export const ToastRenderer: React.FC
```

### Barrel export

```typescript
// @dia/editor-ui/src/index.ts
export { theme, injectThemeVars, inputStyle, buttonStyle } from './theme'
export { TrafficLightDot } from './TrafficLightDot'
export { TabBar } from './TabBar'
export { EmptyState } from './EmptyState'
export { ConnectionStatus } from './ConnectionStatus'
export { useBridgeSubscribe, useBridgeRequest } from './useBridge'
export { useToast, ToastRenderer } from './notifications/useToast'
```

## Dependencies

- **DiaEditor** — `WebUIBridge` is the C++ target that `useBridge` wraps; no new C++ surface required
- **React 18** — peer dependency; each consuming editor supplies it
- **zustand 4** — used internally by the toast store; peer dependency
- **No other Dia C++ systems** — this is a pure TypeScript library

## Inherited Binding Decisions

| Source | ID | Decision | Impact on DiaEditorUI |
|--------|----|----------|-----------------------|
| Platform | PD-004 | No STL in public APIs | N/A — TypeScript library only |
| Platform | PD-005 | x64 only | No build artefact; npm workspace package |
| Platform | PD-006 | VS project files are source of truth | No `.vcxproj` needed; purely a UI package |
| Dia | — | React + Vite for all editor UIs | DiaEditorUI is a React library; sets the standard |

## System-Specific Decisions

| ID | Decision | Rationale | Status | Binding |
|----|----------|-----------|--------|---------|
| DEUI-001 | Delivered as a local npm workspace package, not a published registry package | No external registry needed; editors are co-located in the same repo | Accepted | Yes |
| DEUI-002 | No bundled CSS — styles via inline `CSSProperties` and the `theme` object | Avoids CSS import ordering issues in Vite builds; keeps components self-contained | Accepted | Yes |
| DEUI-003 | `useBridgeSubscribe` auto-unsubscribes on unmount | Prevents stale listener leaks when editor panels are hidden/unmounted | Accepted | Yes |
| DEUI-004 | Toast store is module-level zustand; `ToastRenderer` must be mounted once at app root | Single queue per plugin; avoids prop-drilling | Accepted | Yes |
| DEUI-005 | Existing `TrafficLightDot.tsx` in DiaApplicationFlowEditor and DiaApplicationFlowInspector are deleted and replaced with the shared import | Prevents the duplicate from drifting again | Accepted | Yes |
| DEUI-006 | `ConnectionStatus` replaces `ConnectionStatusDot`, `LiveConnectionButton`, and `ConnectionButton` across all editors — migration required at adoption | Canonical component; three divergent designs are retired | Accepted | Yes |
| DEUI-007 | Each editor references `@dia/editor-ui` via a relative `file:` path — no npm workspace root | Vite bundles the shared package at build time; deployed asset is self-contained; workspace overhead not justified for one shared package | Accepted | Yes |
| DEUI-008 | `ConnectionStatus` has no knowledge of connection configuration (host/port); CluicheEditor wraps it with its own host/port panel | Connection config is application-specific behaviour; shared component handles state display only | Accepted | Yes |
| DEUI-009 | `useBridgeRequest` is generic: `useBridgeRequest<T>()` | Compile-time shape verification at each call site; casting with `as T` silences the compiler without verifying the shape | Accepted | Yes |

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Package scaffold | Create `Dia/DiaEditorUI/` npm workspace package with Vite lib mode build | — | Draft |
| Theme constants | `theme` object + `injectThemeVars()` + style factories; update all 5 React editors | — | Draft |
| TrafficLightDot | Extract from DiaApplicationFlowEditor; delete duplicate in DiaApplicationFlowInspector | — | Draft |
| TabBar | New component; replace inline tab implementations in AppFlowEditor, AppFlowInspector, PipelineEditor | — | Draft |
| EmptyState | Extract from DiaPipelineEditor (`EmptyState.tsx`); generalise for other editors | — | Draft |
| ConnectionStatus | Unified component; retire `ConnectionStatusDot`, `LiveConnectionButton`, `ConnectionButton` | — | Draft |
| useBridge hook | Extract `useEffect` subscribe/request pattern present in all 4 editors | — | Draft |
| Toast system | Move `useNotifications` + `ToastRenderer` from CluicheEditor into shared package | — | Draft |

## Status

`Done`

**Plan:** @docs/specs/applications/dia/systems/diaeditorui/diaeditorui.plan.md
