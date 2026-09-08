# Feature Spec: Connection Status Indicator

## Parent System
@docs/specs/applications/dia/systems/diaapplicationfloweditor/diaapplicationfloweditor.md

## Purpose

A minimal, read-only visual indicator in the Editor header showing whether a game is currently connected. Replaces the full 3-state "Connect Live" button which moved to DiaApplicationFlowInspector. The Editor no longer owns connection lifecycle — it only observes the result.

## Acceptance Criteria

1. **Read-only indicator** — Editor header shows a single `.tl` traffic-light dot. No button, no click action, no tooltip with connection details.
2. **Two states only:**
   - Grey dot: offline (no game connected)
   - Green dot + subtle pulse: live (game connected via Inspector)
3. **Data source** — Reads `live.connectionStatus` topic pushed by the Inspector plugin via the shared WebUIBridge topic bus.
4. **Default offline** — Grey dot when no `live.connectionStatus` topic has been received (Inspector not loaded, Inspector not connected, or no topic yet). Never shows an error state.
5. **Immediate update** — Transitions to grey within the same render frame when `live.connectionStatus.connected` becomes `false`.
6. **No Inspector coupling** — Editor does not import, reference, or depend on any Inspector code. The topic bus is the only interface.

## Design

### React Component

```typescript
// ConnectionStatusDot.tsx
// Reads connectionState from the read-only useLiveStoreV2 (Editor's trimmed version)
// Renders TrafficLightDot: grey when disconnected, green+pulse when connected
```

Placed in the Editor header bar, right-aligned, before the Toolbar controls.

### Data Flow

```
Inspector plugin → GameConnectionManager::Connect() → success
Inspector plugin → publishes topic: live.connectionStatus {connected: true, host, port}
Editor plugin   → receives topic via WebUIBridge bus subscription
Editor UI       → useLiveStoreV2.connectionState updated → ConnectionStatusDot re-renders green
```

## Tasks

| # | Task | Test | Status | Notes |
|---|------|------|--------|-------|
| 1 | `ConnectionStatusDot.tsx` component (reads store, renders dot) | `ConnectionStatusDot.test.tsx`: grey when disconnected, green+pulse when connected | Todo | |
| 2 | Place in Editor header (AppV2.tsx) | Manual: dot visible in header at all times | Todo | |
| 3 | Verify default state on fresh load (no Inspector) | Manual: dot is grey, no console errors | Todo | |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| ED-003 | Live mode is overlay on static view | Dot is a passive indicator; no mode switch. |
| ED-007 | React + CEF frontend | React component. |
| ED-008 | Single .tl traffic-light dot primitive | Uses existing TrafficLightDot. |

## Status

`Approved` — 2026-06-04
