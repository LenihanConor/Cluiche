# Game UI Authoring Guide

**Scope:** CluicheTest and future Cluiche games — debug panels, HUDs, tool overlays.  
**Not for:** CluicheEditor UI — see [editor-ui-authoring.md](editor-ui-authoring.md).  
**Spec:** @docs/specs/features/dia/diauiultralight/game-ui-framework-convention.md

---

## The Boundary Rule

> **If a panel needs to read or react to state owned by another panel → use React.**  
> **If it is self-contained → use Alpine.**

When in doubt, start with Alpine. If you find yourself fighting `x-data` scoping to share state between sections, port it to React. Delete the Alpine version when you do.

---

## Tier 1 — Alpine Panels

### When to use
- FPS counter, frame time graph, log stream
- Variable inspector (single source of data)
- Health/resource bars
- Toast notifications
- Any panel that displays or reacts to one stream of C++ data

### File layout
```
Cluiche/CluicheTest/AlpinePanels/
  template.html          ← starter template (copy this)
  fps-counter.html
  log-stream.html
  ...
```

Deploy `.html` files to `Cluiche/bin/{Config}/x64/ui/alpine/` alongside the game binary.

### Starter template
Copy `AlpinePanels/template.html` and replace the `x-data` block. The template wires up:
- `window.GameBridge_dispatch(topic, dataJson)` — C++ pushes data in
- `window.app.methodName(argsJson)` — JS sends data to C++
- DaisyUI `data-theme` attribute on `<html>` for per-game theming
- Vendored Alpine/Tailwind/DaisyUI from `External/` (no CDN)

### C++ bridge pattern (Alpine)
```javascript
// Receive data from C++ — C++ calls: page->CallJavaScriptFunction("GameBridge_dispatch", ...)
window.GameBridge_dispatch = (topic, dataJson) => {
  if (topic !== 'my_topic') return;
  const data = JSON.parse(dataJson);
  this.myValue = data.value;   // 'this' is the Alpine x-data component
};

// Send data to C++ — C++ registers: page->SetCallback("onMyEvent", &myMethod)
window.app.onMyEvent(JSON.stringify({ key: value }));
```

### AI prompt template (Alpine)
```
Generate a single-file Alpine.js debug panel for Cluiche game UI.

Requirements:
- Single .html file, no build step
- Load Alpine from: ../../External/Alpine/alpine.min.js (defer)
- Load Tailwind from: ../../External/Tailwind/tailwind.min.js
- Load DaisyUI from: ../../External/DaisyUI/daisyui.min.css
- Load theme from: themes/cluichetest.css
- Set <html data-theme="cluichetest">
- Use window.GameBridge_dispatch(topic, dataJson) to receive data from C++
- Use window.app.methodName(argsJson) to send data to C++
- Use DaisyUI component classes (btn, table, stat, badge, progress, toast, etc.)
- Keep all state in x-data — no cross-panel state
- Use Alpine directives: x-data, x-text, x-bind, x-for, x-show, @click

Panel description: [DESCRIBE YOUR PANEL HERE]
Data shape from C++: [DESCRIBE THE JSON SHAPE]
```

---

## Tier 2 — React Panels

### When to use
- Entity inspector linked to a scene list
- Production chain viewer (selection drives multiple sections)
- Pathfinding debug overlay (selection drives map + stats + path list)
- Any panel where two or more components react to shared selection state

### Project layout
```
Cluiche/CluicheTest/UI/
  package.json
  vite.config.ts
  tsconfig.json
  src/
    index.html
    main.tsx               ← entry point; render your top-level panel here
    bridge/
      GameBridge.ts        ← C++↔JS bridge (do not modify the bridge contract)
      GameBridge.test.ts   ← bridge contract tests
    panels/
      ExamplePanel.tsx     ← cross-panel state demo — entity list + detail
      YourPanel.tsx        ← add new panels here
    themes/
      cluichetest.css      ← per-game DaisyUI theme
```

Build: `npm run build` from `Cluiche/CluicheTest/UI/`  
Output: `Cluiche/bin/Debug/x64/ui/` (provisional — will change when deployment is formalised)

### C++ bridge pattern (React)
```typescript
import { useEffect, useState } from "react";
import { GameBridge } from "../bridge/GameBridge";

function MyPanel() {
  const [data, setData] = useState<MyData | null>(null);

  useEffect(() => {
    // Subscribe to C++ data push
    return GameBridge.subscribe("my_topic", (raw) => {
      setData(raw as MyData);
    });
  }, []);

  const handleClick = () => {
    // Send to C++
    GameBridge.send("onMyEvent", { key: "value" });
  };

  return <div onClick={handleClick}>{data?.value}</div>;
}
```

### Cross-panel state pattern (React context)
```typescript
// 1. Define context
const SelectionCtx = createContext<{ id: string | null; setId: (id: string) => void }>({
  id: null, setId: () => {},
});

// 2. Provide at root
function App() {
  const [id, setId] = useState<string | null>(null);
  return (
    <SelectionCtx.Provider value={{ id, setId }}>
      <EntityList />   {/* sets id on click */}
      <DetailPanel />  {/* reads id */}
      <ResourcePanel />{/* reads id */}
    </SelectionCtx.Provider>
  );
}

// 3. Consume in any panel
function DetailPanel() {
  const { id } = useContext(SelectionCtx);
  // ...
}
```

### AI prompt template (React)
```
Generate a React TypeScript component for Cluiche game UI.

Requirements:
- TypeScript (.tsx), functional component, React hooks only
- Import GameBridge from "../bridge/GameBridge"
- Use GameBridge.subscribe(topic, callback) to receive C++ data (return the unsubscribe fn from useEffect)
- Use GameBridge.send(methodName, data) to send data to C++
- Use useContext(SelectionCtx) if this panel shares state with other panels
- No inline styles — use DaisyUI classes (table, stat, badge, btn, progress, modal, tabs, etc.)
- The panel will be rendered inside a single Ultralight View — no iframes, no routing

Panel description: [DESCRIBE YOUR PANEL HERE]
Data shape from C++ (topic name + JSON shape): [DESCRIBE]
Cross-panel state needed: [yes/no — if yes, describe what state is shared]
```

---

## Per-Game Theming

Both tiers read their theme from DaisyUI's `data-theme` attribute on `<html>`.

1. Copy `Cluiche/CluicheTest/UI/src/themes/cluichetest.css` to `themes/<your-game>.css`
2. Edit the CSS custom properties to match your game's colour palette
3. Set `<html data-theme="your-game">` in Alpine panels and `index.html` for React panels
4. Load the theme file via `<link rel="stylesheet" href="themes/your-game.css">`

Only override what you need — DaisyUI's dark theme fills everything else.

---

## Upgrade Path: Alpine → React

When an Alpine panel outgrows the boundary rule:

1. Create a new `.tsx` file in `CluicheTest/UI/src/panels/`
2. Re-implement the panel in React using `GameBridge.subscribe` and context as needed
3. **Delete the Alpine `.html` file** — do not keep both versions
4. Update `main.tsx` to render the new component

---

## Running Tests

```bash
cd Cluiche/CluicheTest/UI
npm test          # run bridge contract tests (6 tests)
npm run build     # verify TypeScript compiles and Vite bundles cleanly
```

The bridge contract tests are the only automated tests. They verify:
- C++ can push data to Alpine-equivalent subscribers via `window.GameBridge_dispatch`
- C++ can push data to React components via `GameBridge.subscribe`
- React can send data to C++ via `window.app`
- Unsubscribe works correctly
- Bridge degrades gracefully when `window.app` is unavailable

---

## What NOT to do

| Don't | Do instead |
|-------|-----------|
| Use Vue, Svelte, Preact, or any other framework | Alpine (simple) or React (complex) |
| Load frameworks from CDN URLs | Use vendored files from `External/` |
| Use `window.dia` (that's the editor bridge) | Use `window.app` / `GameBridge` |
| Keep an Alpine panel after porting to React | Delete the Alpine version |
| Put cross-panel state in an Alpine panel | Port to React |
| Import from `node_modules` in Alpine panels | Alpine panels have no build step |
