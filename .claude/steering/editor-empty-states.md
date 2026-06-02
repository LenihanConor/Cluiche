# Editor Empty-State Overlay Standard

Every CluicheEditor plugin that requires an external dependency **must** render a
blocking overlay when that dependency is absent. The overlay must disappear the moment
the dependency becomes available.

## Two overlay variants

| Variant | When to show | Message | Hint |
|---------|-------------|---------|------|
| `no-diagame` | No `.diagame` project is loaded | `No project loaded` | `Open a .diagame project to use this panel` |
| `no-connection` | No live game connection | `No game connected` | `Use the Game Connection panel to connect to a running game` |

## Required HTML structure

Place immediately after `<body>`, before the toolbar, so `z-index` stacking works.

```html
<div class="no-project-overlay" id="no-project-overlay">
  <div class="overlay-message">No project loaded</div>
  <div class="overlay-hint">Open a .diagame project to use this panel</div>
</div>
```

## Required CSS

```css
.no-project-overlay {
  position: absolute;
  top: 0; left: 0; right: 0; bottom: 0;
  background: rgba(30, 30, 30, 0.92);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-direction: column;
  gap: 8px;
  z-index: 100;
}
.no-project-overlay .overlay-message {
  font-size: 14px;
  color: #f48771;
}
.no-project-overlay .overlay-hint {
  font-size: 11px;
  color: #888;
}
```

`body` must have `position: relative` for the overlay to cover the full panel.

## Required JS wiring

### On init (startup race-condition safe)

Call the plugin's `get_project_state` handler with a retry loop (up to ~8 attempts,
300 ms apart) until a response arrives. Show overlay if `isValid` is false; hide if true.

```js
var _initAttempts = 0;
function tryInit() {
  diaRequest('myplugin.get_project_state', {}).then(function(r) {
    if (r && r.isValid !== undefined) {
      updateProjectOverlay(r.isValid);
    } else if (_initAttempts++ < 8) {
      setTimeout(tryInit, 300);
    }
  });
}
setTimeout(tryInit, 200);
```

### On C++ push

Every plugin that needs `.diagame` already subscribes to a `project_changed` topic.
Wire the overlay to that topic:

```js
if (msg.topic === 'myplugin.project_changed') {
  updateProjectOverlay(msg.data && msg.data.isValid);
}
```

### Helper

```js
function updateProjectOverlay(isValid) {
  var el = document.getElementById('no-project-overlay');
  if (el) el.style.display = isValid ? 'none' : 'flex';
}
```

## Required C++ handler

Each plugin must expose a `<plugin>.get_project_state` request handler:

```cpp
mBridge->RegisterRequestHandler(
  Dia::Core::StringCRC("myplugin.get_project_state"),
  [this](const Json::Value&) -> Json::Value {
    Json::Value r;
    r["isValid"]     = mDiagamePath[0] != '\0';
    r["diagamePath"] = mDiagamePath;
    return r;
  });
```

The plugin must store the last-known `.diagame` path (e.g. `mDiagamePath`) and update
it in `OnProjectChangedStatic`.

## Which plugins are exempt

- `home`, `hello`, `stub` — static/informational, no dependency
- `outputconsole` — always usable
- `pluginbrowser` — always usable
- `gameconnection` — is the connection manager; uses a different 3-state UI
- `DiaApplicationEditor` — has its own project-state model
- `DiaPipelineEditor` — React build; handles its own state internally
- `debug-layers` — no project dependency

## Reference implementation

`Dia/DiaAssetRuntimeEditor/UI/index.html` — `no-connection` variant.
Class names there are `.disconnect-overlay`, `.message`, `.hint`. New plugins should
use `.no-project-overlay`, `.overlay-message`, `.overlay-hint` for clarity.
