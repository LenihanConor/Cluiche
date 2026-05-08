# Editor UI Authoring Guide

**Scope:** CluicheEditor panels, dialogs, toolbars, and all editor-facing UI.  
**Not for:** Game debug panels or HUDs — see [game-ui-authoring.md](game-ui-authoring.md).  
**Spec:** @docs/specs/features/dia/diauicef/editor-ui-framework-convention.md

---

## The Stack (Fixed)

| Layer | Choice | Notes |
|-------|--------|-------|
| Framework | React 18 | Functional components + hooks only |
| Language | TypeScript 5 | `.tsx` components, `.ts` utilities — no `.js`/`.jsx` |
| Build | Vite 4 | `tsc && vite build` |
| Layout | react-mosaic-component | Dockable panel layout |
| Components | shadcn/ui + Tailwind CSS | Buttons, tables, dialogs, forms |
| Styling | Tailwind CSS | Utility classes; no plain CSS files unless necessary |
| Tests | vitest + @testing-library/react | All new components must have tests |
| Bridge | `window.dia.callCpp` via `EditorBridge` | Never use `window.app` (that's the game bridge) |

**This stack is fixed.** Introducing any other framework (Vue, Alpine, Svelte, Webix, Preact, Angular) requires a new spec decision superseding the Editor UI Framework Convention spec.

---

## Project Layout

```
Cluiche/CluicheEditor/UI/
  package.json
  vite.config.ts         ← root: src/, outDir: ../dist/
  tsconfig.json
  src/
    index.html
    main.tsx             ← entry point
    bridge/
      EditorBridge.ts    ← all C++↔JS communication (do not bypass)
    components/          ← reusable UI primitives
      CommandPalette.tsx
      SplashScreen.tsx
    layout/
      DockingManager.tsx ← react-mosaic panel layout
      Toolbar.tsx
    test/
      setup.ts
```

Build: `npm run build` from `Cluiche/CluicheEditor/UI/`  
Output: `Cluiche/CluicheEditor/UI/dist/` → deployed to `Cluiche/bin/{Config}/x64/diaapplicationeditor/`

---

## C++ Bridge Pattern

All C++ communication goes through `EditorBridge`. Never call `window.dia.callCpp` directly.

```typescript
import { EditorBridge } from "../bridge/EditorBridge";

// Fire-and-forget event to C++
EditorBridge.executeCommand("myCommand", { arg: "value" });

// Request with response
const result = await EditorBridge.getPanels();

// Subscribe to C++ data push (topic-based)
useEffect(() => {
  return EditorBridge.subscribe("my_topic", (data) => {
    setMyState(data as MyType);
  });
}, []);
```

**Never use:**
- `window.app` — that is the Ultralight game bridge
- `window.dia.callCpp` directly — always go through `EditorBridge`
- `fetch` or `XMLHttpRequest` for C++ communication — use `EditorBridge`

---

## Component Pattern

```tsx
// components/MyPanel.tsx
import { useEffect, useState } from "react";
import { EditorBridge } from "../bridge/EditorBridge";

type MyData = { id: string; label: string };

export function MyPanel() {
  const [items, setItems] = useState<MyData[]>([]);

  useEffect(() => {
    return EditorBridge.subscribe("my_data_topic", (raw) => {
      setItems(raw as MyData[]);
    });
  }, []);

  return (
    <div className="flex flex-col gap-2 p-4">
      {items.map((item) => (
        <div key={item.id} className="rounded bg-neutral p-2 text-sm">
          {item.label}
        </div>
      ))}
    </div>
  );
}
```

Rules:
- Functional components only — no class components
- One component per file; filename matches component name (`MyPanel.tsx`)
- All state via hooks (`useState`, `useReducer`, `useContext`)
- Tailwind utility classes for all styling
- Use shadcn/ui primitives for buttons, dialogs, tables, forms — do not reinvent them

---

## Test Pattern

Every new component needs a test file alongside it:

```tsx
// components/MyPanel.test.tsx
import { render, screen } from "@testing-library/react";
import { describe, it, expect, vi } from "vitest";
import { MyPanel } from "./MyPanel";

// Mock EditorBridge so tests don't need a real C++ process
vi.mock("../bridge/EditorBridge", () => ({
  EditorBridge: {
    subscribe: vi.fn(() => () => {}),  // returns unsubscribe no-op
    executeCommand: vi.fn(),
  },
}));

describe("MyPanel", () => {
  it("renders without crashing", () => {
    render(<MyPanel />);
    // assert something meaningful
  });
});
```

Run tests: `npm test` from `Cluiche/CluicheEditor/UI/`

---

## Adding a New Panel to the Docking Layout

1. Create `src/components/MyPanel.tsx`
2. Create `src/components/MyPanel.test.tsx`
3. Register the panel in `DockingManager.tsx` — add to the panel map
4. Wire up `EditorBridge` topics as needed
5. Run `npm test` and `npm run build` — both must pass

---

## AI Prompt Template

```
Generate a React TypeScript component for the CluicheEditor.

Requirements:
- TypeScript (.tsx), functional component, React hooks only
- Import EditorBridge from "../bridge/EditorBridge"
- Use EditorBridge.subscribe(topic, callback) to receive C++ data
  (return the unsubscribe fn as the useEffect cleanup)
- Use EditorBridge.executeCommand(id, args) to send commands to C++
- Use Tailwind CSS utility classes for all styling
- Use shadcn/ui primitives where applicable (Button, Dialog, Table, Input, etc.)
- No inline styles, no plain CSS files
- Export a named component matching the filename

Component description: [DESCRIBE THE PANEL]
C++ data topics it subscribes to: [topic name + JSON shape]
Commands it sends to C++: [command IDs + args]
```

---

## What NOT to do

| Don't | Do instead |
|-------|-----------|
| Use Vue, Alpine, Svelte, Preact, or any other framework | React + TypeScript only |
| Use `window.app` | Use `EditorBridge` — `window.app` is the game bridge |
| Call `window.dia.callCpp` directly | Always go through `EditorBridge` |
| Write `.js` or `.jsx` files | Always `.ts` / `.tsx` |
| Use class components | Functional components + hooks |
| Skip writing a test | Every component gets a test |
| Use plain CSS files for layout | Tailwind utility classes |
| Add a new UI framework dependency without a spec decision | Raise a spec change first |
