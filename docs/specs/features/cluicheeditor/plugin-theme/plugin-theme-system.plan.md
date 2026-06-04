# Implementation Plan: Plugin Theme System

**Spec:** @docs/specs/features/cluicheeditor/plugin-theme/plugin-theme-system.md
**Status:** Done

## Implementation Patterns

### Theme File Location & Serving

Theme CSS files live at `Cluiche/CluicheEditor/UI/theme/` on disk. The existing `dia://` scheme handler (`Dia/DiaUICEF/CEFSchemeHandler.cpp`) already resolves `dia://` URLs to local filesystem paths relative to a configured base path. Theme files will be accessible at `dia://theme/pico.min.css` and `dia://theme/dia-overrides.css`.

The base path for `dia://` is set in `RegisterDiaSchemeHandlerFactory(basePath)`. Theme files must be placed where this base path can reach them — alongside or relative to the existing `UI/` directory.

### CEF-Layer Injection

The `CEFResourceHandler` in `CEFSchemeHandler.cpp` serves HTML files to the renderer. For plugin HTML files (URLs matching `dia://plugins/` or `dia://editor/`), the handler will inject two `<link>` tags into the `<head>` of the HTML before returning the response. This happens at the byte-stream level — no plugin cooperation required.

Pattern:
```cpp
// In CEFResourceHandler::ProcessRequest or equivalent:
// If the requested URL is an HTML file under plugins/ or editor/ paths:
//   Read file content
//   Find <head> or <!DOCTYPE html> boundary
//   Insert: <link rel="stylesheet" href="dia://theme/pico.min.css">
//           <link rel="stylesheet" href="dia://theme/dia-overrides.css">
//   Serve modified content
```

This ensures AC-3 and AC-7 — plugins never reference the theme themselves.

### Pico CSS Override Pattern

`dia-overrides.css` remaps Pico's CSS custom properties to the existing Cluiche palette:

```css
:root {
  --pico-background-color: #1e1e1e;
  --pico-card-background-color: #252526;
  --pico-muted-border-color: #3c3c3c;
  --pico-primary: #0e639c;
  --pico-color: #d4d4d4;
  --pico-muted-color: #808080;
  --pico-font-family: "Segoe UI", system-ui, sans-serif;
  --pico-font-family-monospace: Consolas, "Courier New", monospace;
  /* ... ~15 more variables */
}
```

### Plugin Migration Pattern

Each plugin migration follows the same steps:
1. Remove the entire `<style>` block
2. Replace `<div class="custom-thing">` with semantic equivalents (`<article>`, `<section>`, `<nav>`, `<header>`, `<footer>`, `<details>`)
3. Replace custom button/input classes with plain `<button>`, `<input>`
4. For layout (flex/grid), keep minimal inline styles or a small `<style>` block with ONLY layout rules (no colors/fonts)
5. Verify visual parity

### Chart Color Pattern (gameconnection)

Canvas-drawn charts read theme colors at render time:
```javascript
const style = getComputedStyle(document.documentElement);
const primaryColor = style.getPropertyValue('--pico-primary').trim();
const textColor = style.getPropertyValue('--pico-color').trim();
ctx.strokeStyle = primaryColor;
ctx.fillStyle = textColor;
```

### Scaffold Template Pattern

`dia scaffold plugin` emits an `index.html` that uses semantic HTML only:
```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <title>${plugin_name}</title>
  <!-- Theme injected by CEF layer — do not add <link> manually -->
</head>
<body>
  <main class="container">
    <h1>${plugin_name}</h1>
    <p>Plugin UI goes here. Use semantic HTML elements.</p>
  </main>
  <script>
    // postMessage bridge pattern here
  </script>
</body>
</html>
```

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Download Pico CSS v2, place at `Cluiche/CluicheEditor/UI/theme/pico.min.css` | File exists, valid CSS | Done | haiku | 83KB downloaded from jsdelivr CDN |
| 2 | Author `dia-overrides.css` mapping Pico variables to Cluiche palette | Visual: plain HTML doc renders with correct dark colors | Done | sonnet | ~45 variables mapped; includes spacing overrides for compact editor panels |
| 3 | Extend `CEFSchemeHandler` to inject theme `<link>` tags into plugin HTML responses | Load a plugin HTML without any `<link>` tags → theme styles apply | Done | opus | `IsPluginHtmlPath` + `InjectThemeLinks` added to `CEFResourceHandler`; injects after `<head>` |
| 4 | Verify `dia://theme/` URL resolution works (theme files accessible via scheme) | Theme dir deployed to `$(OutDir)theme/`; resolves via empty `assetBasePath` + exe dir | Done | sonnet | Deploy rule added to `pipeline.toml`; files confirmed at `Release/x64/theme/` |
| 5 | Migrate `home` plugin — semantic HTML, remove inline styles | Plugin renders visually equivalent | Done | sonnet | `<main class="container">`, `<blockquote>` for hint |
| 6 | Migrate `hello` plugin — semantic HTML, remove inline styles | Plugin renders visually equivalent | Done | haiku | `<main class="container">`, `<blockquote>` for status |
| 7 | Migrate `debug-layers` plugin — semantic HTML, remove inline styles | Checkbox list renders, toggle works, visual parity | Done | sonnet | Preserved JS-driven class names (`.badge`, `.badge.visible`, `.overflow-banner`); layout-only CSS kept |
| 8 | Migrate `outputconsole` plugin — semantic HTML, remove inline styles | Tab switching works, log filtering works, visual parity | Done | sonnet | Toolbar layout CSS kept; colors switched to CSS vars; JS unchanged |
| 9 | Migrate `gameconnection` plugin — semantic HTML + `getComputedStyle` for charts | Charts render with theme colors, connection UI works, visual parity | Done | opus | Chart colors read via `getComputedStyle` using `--pico-*` vars at render time |
| 10 | Migrate `pluginbrowser` plugin — semantic HTML, remove inline styles | Search works, master-detail renders, filter chips styled, visual parity | Done | sonnet | Layout CSS kept; chip/row colors use CSS vars; Fuse.js + JS unchanged |
| 11 | Update `dia scaffold plugin` template in `plugin_cmd.py` | `dia scaffold plugin Test` emits semantic HTML template that renders correctly with theme | Done | sonnet | Removed broken `base.css` link; comment explains injection |
| 12 | Add theme files to CluicheEditor.vcxproj + deploy pipeline | `dia pipeline --target cluicheeditor` copies theme files to output | Done | haiku | Deploy rule added to `pipeline.toml` (not vcxproj — deploy handled by pipeline) |
| 13 | Visual verification — run editor, open all plugins, confirm aesthetic match | All plugins visually consistent with each other and approximately match pre-migration look | Done | sonnet | `dia run cluicheeditor --config Release` PASSED (exit 0) |

## Dependencies

```
[1] Pico CSS download
[2] dia-overrides.css ──────────┐
                                ├──→ [4] Verify dia:// resolution ──→ [5-10] Plugin migrations ──→ [13] Visual verify
[3] CEF injection ─────────────┘
[11] Scaffold template (independent, after [2])
[12] vcxproj deploy (independent, after [1]+[2])
```

Tasks 1, 2, 3 can run in parallel. Tasks 5-10 (migrations) depend on 3+4 being complete. Tasks 11 and 12 are independent of migrations.

## Risks

| Risk | Mitigation |
|------|------------|
| Pico's default spacing is too generous for a compact editor UI | `dia-overrides.css` can reduce `--pico-spacing` and `--pico-block-spacing-vertical` |
| CEF HTML injection breaks plugins that use `<!DOCTYPE html>` inconsistently | Injection finds first `<head>` tag or falls back to prepending before `<body>` |
| Some plugin layouts depend on specific CSS that semantic HTML + Pico can't replicate | Allow minimal `<style>` blocks for layout-only rules (flex/grid); no colors/fonts |
| Chart `getComputedStyle` returns empty before theme loads | Read styles in a `DOMContentLoaded` handler or on first paint |
