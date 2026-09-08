# Plan: Plugin Browser Scaling

**Spec:** docs/specs/features/cluicheeditor/pluginbrowser/plugin-browser-scaling.md
**Status:** In Progress

## Session Notes

Spec decisions summary (Platform → App → System → Feature):
- PD-001: All plugin IDs are StringCRC — `TagPluginManifest`, `SetActiveManifests`, palette command names all use StringCRC.
- PD-004: `SetActiveManifests` takes `DynamicArrayC<StringCRC, kMaxManifests>`, not `std::vector`.
- AED-002/AED-006: Filter boundary is the `.diaapp` manifest list from `.cluicheproj`.
- SCPB-001/SCPB-002: Filter is display-only (C++ side only); does not unload plugins.
- AC1/AC2: Empty active manifest list = no filter (cold-start); non-empty = filter active. Built-ins (empty `manifestId`) always pass.
- T2 location: `PluginLoaderModule::DoStart` — not `EditorModel`. Only `PluginLoaderModule` has the manifest→typeId mapping.
- Spec Q5: double-load guard already exists in `PluginLoaderModule::LoadPlugin` — demote WARNING → INFO per spec.
- fuse.min.js: copy from npm `node_modules` into `Dia/DiaEditor/Plugin/Assets/pluginbrowser/`.

## Implementation Patterns

### Manifest scope filter (T1a/T1b/T2)

`EditorPluginRegistry::PluginEntry` gains `manifestId` (StringCRC, default empty = built-in).

```cpp
// New methods on EditorPluginRegistry:
void TagPluginManifest(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& manifestId);
void SetActiveManifests(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests>&);
void ClearActiveManifests();
bool IsInScopeFilter(const Dia::Core::StringCRC& typeId) const;
// Rule: empty active list → pass all; empty manifestId (built-in) → pass; else manifestId in active set
```

```cpp
// PluginLoaderModule::LoadManifest lambda — tag after load:
EditorPluginRegistry::Instance().TagPluginManifest(
    Dia::Core::StringCRC(entry.typeId), Dia::Core::StringCRC(manifestPath));

// PluginLoaderModule::DoStart — after manifest loop:
Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 16> active;
for (unsigned int i = 0; i < manifestCount; ++i)
    active.Add(Dia::Core::StringCRC(model.GetManifestPath(i)));
EditorPluginRegistry::Instance().SetActiveManifests(active);

// PluginBrowserEditorPlugin::get_available — skip out-of-scope:
if (!registry.IsInScopeFilter(typeId)) continue;
```

### Double-load guard (T7)

Replace `DIA_LOG_WARNING` with `DIA_LOG_INFO` in `PluginLoaderModule::LoadPlugin` guard:
```cpp
DIA_LOG_INFO("Application", "PluginLoaderModule::LoadPlugin: '%s' already loaded, skipping", typeId.AsChar());
```

### Command registration (T6)

In `PluginBrowserEditorPlugin::OnLoad`, after bridge handlers, loop registry and register JSON commands:
```cpp
Dia::API::CommandInfoJson cmd;
cmd.name  = Dia::Core::StringCRC((std::string("plugin.load.") + typeId.AsChar()).c_str());
cmd.category = Dia::Core::StringCRC("plugin");
cmd.owner = "PluginBrowser";
cmd.callback = [loader, typeId](const Json::Value&) -> Json::Value {
    loader->LoadPlugin(typeId, Dia::Core::StringCRC((std::string(typeId.AsChar()) + "_palette").c_str()));
    return Json::Value(Json::objectValue);
};
Dia::API::CommandRegistry::RegisterCommandJson(cmd);
// mirror for plugin.unload.<id>
```

### HTML layout (T3/T4/T5)

```
┌──────────────────────────────────┐
│ [search input                  ] │  T4
│ [All] [Loaded] [Available]       │  T5
├──────────────────────────────────┤
│ Plugin Name             [Unload] │  T3 compact rows ≤32px (scrollable)
├──────────────────────────────────┤
│ Plugin Name  v1.0  ● Loaded      │  T3 detail pane 120px fixed
│ Description text                 │
└──────────────────────────────────┘
```
fuse.js: `<script src="./fuse.min.js"></script>` — keys: `["name","description"]`, threshold: 0.4

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T1a | Add `manifestId` to `PluginEntry`; add `TagPluginManifest`, `SetActiveManifests`, `ClearActiveManifests`, `IsInScopeFilter` | Compile clean | Done | sonnet | |
| T1b | Apply `IsInScopeFilter` in `get_available` handler | All plugins at cold-start; filtered after project load | Todo | sonnet | Depends on T1a |
| T2 | Tag plugins in `LoadManifest` lambda; call `SetActiveManifests` in `DoStart` after manifest loop | Load project → list narrows | Todo | sonnet | Depends on T1a |
| T3 | Rewrite `pluginbrowser/index.html`: compact rows + 120px detail pane | Rows ≤32px; detail always visible | Todo | sonnet | |
| T4 | Add fuse.js search input + copy `fuse.min.js` | Type → list narrows; clear → all shown | Todo | sonnet | Depends on T3 |
| T5 | Add `All / Loaded / Available` chips | Chips compose with search | Todo | sonnet | Depends on T3/T4 |
| T6 | Register `plugin.load.<id>` / `plugin.unload.<id>` commands in `OnLoad` | Commands appear in palette | Todo | sonnet | Depends on T2 |
| T7 | Demote double-load guard WARNING → INFO in `PluginLoaderModule::LoadPlugin` | Info log emitted; no double-load | Done | haiku | |
| T8 | Git commit | — | Todo | haiku | |
