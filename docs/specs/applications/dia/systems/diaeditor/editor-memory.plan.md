**Spec:** @docs/specs/applications/dia/systems/diaeditor/editor-memory.md
**Status:** Done

## Implementation Patterns

### EditorMemory class (DiaEditor library)

Plain C++ class in `Dia/DiaEditor/Memory/EditorMemory.h/.cpp`. No Module/Phase inheritance (SED-015). Owns serialization logic for `.memory.json`:

```cpp
namespace Dia::Editor {
    struct MemoryPluginEntry {
        char typeId[128];
        char instanceId[128];
    };

    class EditorMemory {
    public:
        bool Load(const char* path);      // Returns false if file missing/corrupt
        bool Save(const char* path) const;

        // Layout tree (opaque JSON blob — same shape DockingLayout already serializes)
        const Json::Value& GetLayoutTree() const;
        void SetLayoutTree(const Json::Value& tree);

        // Plugin list (non-built-in plugins that were loaded)
        unsigned int GetPluginCount() const;
        const MemoryPluginEntry& GetPlugin(unsigned int index) const;
        void AddPlugin(const char* typeId, const char* instanceId);
        void ClearPlugins();

        // Last project path
        const char* GetLastProject() const;
        void SetLastProject(const char* path);

    private:
        Json::Value mLayoutTree;
        static const unsigned int kMaxMemoryPlugins = 16;
        DynamicArrayC<MemoryPluginEntry, kMaxMemoryPlugins> mPlugins;
        char mLastProject[512];
    };
}
```

### Save integration (PluginLoaderModule::DoStop)

After existing unload loop, before `mLoadedPlugins.RemoveAll()`:
1. Create `EditorMemory` instance
2. Iterate `mLoadedPlugins` — skip built-in types (compare against the 4 hardcoded typeIds), add rest to memory
3. Serialize current layout via `mView->GetDockingLayout()->Serialize()`
4. Set last project from `EditorModel::GetProjectPath()`
5. Call `memory.Save("../../out/CluicheEditor/.memory.json")` (relative to editor working dir)

### Restore integration (PluginLoaderModule::DoStart)

After `RestoreLayoutPlugins()` (which handles layout-file panels), add a new step:
1. Try `memory.Load("../../out/CluicheEditor/.memory.json")`
2. If successful: for each plugin in memory, check `EditorPluginRegistry::Instance().IsPluginRegistered(typeId)` — if yes and not already loaded, `LoadPlugin(typeId, instanceId)`
3. Apply layout tree: `mView->GetDockingLayout()->Deserialize(memory.GetLayoutTree())`
4. If `memory.GetLastProject()` is non-empty and no project was specified on command line, call `model.LoadProject(memory.GetLastProject())`

### Graceful skip for missing plugins

`EditorPluginRegistry::IsPluginRegistered()` already exists. On restore, if a typeId from `.memory.json` isn't registered, log `DIA_LOG_WARNING` and skip. No assert, no error dialog.

### EditorPluginContext project path

Add `const char* mProjectPath` to `EditorPluginContext`. Set by `PluginLoaderModule` from `EditorModel::GetProjectPath()` on project load. Plugins can read this to scope their `.context.json` state.

### .context.json project key

Per SED-021, plugins already write `.context.json` in `out/CluicheEditor/<PluginName>/`. Extend the convention: add a `"project"` field. Plugins that want project-scoped state check if `context.mProjectPath` matches their saved `.context.json` `"project"` field — if not, archive the old context per existing SED-021 archive flow.

### File paths

- Memory file: `Cluiche/out/CluicheEditor/.memory.json` (PD-009)
- The `out/` directory is gitignored
- Relative path from editor binary working directory (`Cluiche/bin/CluicheEditor/Debug/x64/`) to `out/` = `../../../../out/CluicheEditor/.memory.json` — OR use `EditorModel::GetOutputPath()` if available, otherwise compute relative to project root

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Create `Dia/DiaEditor/Memory/EditorMemory.h/.cpp` — Load/Save, plugin list, layout tree, last project path | Unit test: round-trip serialize/deserialize; corrupt file returns false; empty file returns false | Done | sonnet | Add to DiaEditor.vcxproj + filters |
| 2 | Add `mProjectPath` to `EditorPluginContext`; set from EditorModel in PluginLoaderModule | Compiles; existing plugins unaffected | Done | haiku | Backward compatible — null by default |
| 3 | Extend `PluginLoaderModule::DoStop` — collect non-built-in plugins + layout + project → save `.memory.json` | Manual: close editor, verify `.memory.json` written to out/ | Done | sonnet | Skip the 4 built-in typeIds |
| 4 | Extend `PluginLoaderModule::DoStart` — load `.memory.json`, restore plugins + layout + project | Manual: open editor, verify plugins/layout restored from previous session | Done | sonnet | After RestoreLayoutPlugins; skip unregistered plugins with warning |
| 5 | Add graceful skip: unregistered plugin in `.memory.json` logs warning, doesn't crash | Unit test: EditorMemory with fake typeId → skip path exercised | Done | haiku | Covered by task 4 code path but needs explicit test |
| 6 | Document `"project"` field convention for per-plugin `.context.json` | Update SED-021 notes in diaeditor.md; no code change | Done | haiku | Convention only — plugin adoption is per-plugin |
