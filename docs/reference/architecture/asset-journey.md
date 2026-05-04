# Asset Journey: End-to-End Walkthrough

How a single asset — `texture.player_ship` — travels from authored file to loaded-in-game, touching every system along the way. Read the [asset-system-overview](../../specs/systems/dia/asset-system-overview.md) for the architectural view; this document is the concrete trace.

---

## Cast

| System | Role in this journey |
|--------|---------------------|
| DiaAssetCatalogue | Data model — identity, relationships, scope, rules |
| DiaAssetCatalogueEditor | UI — how the developer interacts with the catalogue |
| DiaAssetPipeline | Build-time — validates, transforms, deploys files, generates runtime manifest |
| DiaPipeline | Orchestrator — calls DiaAssetPipeline during `build-assets` stage |
| DiaAssetRuntime | In-game — path resolution, Stage lifecycle, state machine |
| DiaAssetRuntimeEditor | Live debugging — shows runtime state over WebSocket |

---

## 1. Authoring — the file exists on disk

An artist saves a texture:

```
Assets/CluicheTest/Characters/player_ship.texture.png
```

The file pattern `*.texture.png` matches the `texture` type descriptor registered in `AssetTypeRegistry` (DiaAssetCatalogue Feature 2).

At this point the file is just a file. No system knows about it yet.

---

## 2. Discovery — the editor finds the file

Developer opens `DiaAssetCatalogueEditor` and clicks **File Discoverer**. The discoverer scans `Assets/CluicheTest/` from the configured root, pattern-matches files against `AssetTypeRegistry`, and presents unregistered files as suggestions.

The developer sees:

```
  player_ship.texture.png    [texture]    Add?
```

They click **Add**. The editor:
1. Creates an `AssetRecord` with ID `texture.player_ship` (composite `type.name`, SD-CAT-001)
2. Calls `ContentHasher::ComputeHash("Assets/CluicheTest/Characters/player_ship.texture.png")` → CRC32 `2918374651`
3. Registers the record in the `AssetRegistry`
4. Wraps the whole operation as an `IEditorCommand` for undo/redo (SD-ACE-004)

The record at this point:

```cpp
AssetRecord {
    mId:             StringCRC("texture.player_ship"),
    mAssetTypeId:    StringCRC("texture"),
    mSourcePath:     FilePath("Assets/CluicheTest/Characters/player_ship.texture.png"),
    mContentHash:    2918374651,
    mStatus:         AssetStatus::Active,
    mScope:          AssetScope::kGlobal,     // default — not yet computed
    mScopeStageName: StringCRC(""),
    mTags:           [],
    mReferences:     []
};
```

Scope is `kGlobal` by default. Tags and relationships are empty. The record exists, but it is not connected to anything.

---

## 3. Rules — automation fills in the gaps

Developer clicks **Dry Run** in the Catalogue Rules UI. The editor calls `CatalogueRulesEngine::EvaluateDryRun()`, which loads `Assets/CluicheTest/assets.rules.json`:

```json
{
    "rules": [
        {
            "name": "characters-folder-tag",
            "match": { "source_path_glob": "Assets/*/Characters/**" },
            "action": "assign_tag",
            "tag": "characters"
        },
        {
            "name": "entity-texture-refs",
            "match": { "type": "entity" },
            "action": "infer_references",
            "source": "typed_fields"
        },
        {
            "name": "player-stage-membership",
            "match": { "source_path_glob": "Assets/*/Characters/player_*" },
            "action": "assign_stage",
            "stage_id": "stage.gameplay"
        },
        {
            "name": "scope-auto",
            "match": { "all": true },
            "action": "compute_scope"
        }
    ]
}
```

Rules evaluated in array order against `texture.player_ship` (SD-CAT-009):

| # | Rule | Match? | Effect |
|---|------|--------|--------|
| 1 | `characters-folder-tag` | Yes — path matches `Assets/*/Characters/**` | Assigns tag `characters` |
| 2 | `entity-texture-refs` | No — type is `texture`, not `entity` | Skip |
| 3 | `player-stage-membership` | Yes — path matches `Assets/*/Characters/player_*` | Adds `contains` edge from `stage.gameplay` to `texture.player_ship` in RelationshipIndex |
| 4 | `scope-auto` | Yes — matches all | `ScopeComputer` traverses: `stage.gameplay` → `texture.player_ship`. Only one Stage references it → `scope = kStage`, `stage_name = "Gameplay"` |

Meanwhile, `entity.player_ship` also exists. Rule 2 fires on it: `RelationshipInferrer` walks `player_ship.entity.json`'s TypeDefinition, finds a field marked `TypeVariableAttributeAssetReference` pointing to `texture.player_ship` → adds a `uses` edge from `entity.player_ship` to `texture.player_ship`.

The dry-run returns a `RuleChangeset`:

```
RuleChangeset {
    mChanges: [
        { texture.player_ship, "tag",          "",        "characters",  "characters-folder-tag", conflict=false },
        { texture.player_ship, "relationship", "",        "stage.gameplay→contains", "player-stage-membership", conflict=false },
        { texture.player_ship, "scope",        "kGlobal", "kStage",      "scope-auto", conflict=false },
        { texture.player_ship, "stage_name",   "",        "Gameplay",    "scope-auto", conflict=false },
        ...
    ],
    mConflictCount: 0
};
```

No conflicts. Developer reviews the preview and clicks **Apply Rules**. The registry is now mutated.

After rules, the record is:

```cpp
AssetRecord {
    mId:             StringCRC("texture.player_ship"),
    mAssetTypeId:    StringCRC("texture"),
    mSourcePath:     FilePath("Assets/CluicheTest/Characters/player_ship.texture.png"),
    mContentHash:    2918374651,
    mStatus:         AssetStatus::Active,
    mScope:          AssetScope::kStage,
    mScopeStageName: StringCRC("Gameplay"),
    mTags:           [ StringCRC("characters") ],
    mReferences:     []   // texture has no forward refs
};
// RelationshipIndex reverse refs:
//   entity.player_ship  --uses-->      texture.player_ship
//   stage.gameplay      --contains-->  texture.player_ship
```

---

## 4. Save — the catalogue manifest is written

Developer clicks **Save**. `CatalogueManifestSerializer::SaveManifest()` writes the full registry to:

```
Assets/CluicheTest/assets.catalogue.json
```

The texture record in the manifest:

```json
{
    "id": "texture.player_ship",
    "type": "texture",
    "source_path": "Assets/CluicheTest/Characters/player_ship.texture.png",
    "content_hash": 2918374651,
    "status": "active",
    "scope": "stage",
    "stage_name": "Gameplay",
    "tags": ["characters"],
    "references": []
}
```

The Stage record `stage.gameplay` exists as its own entry with a `contains` relationship edge to each member asset. Relationship data (forward refs) is serialized per-record; reverse refs are computed on load.

This file is the **source of truth** for the asset catalogue. It is version-controlled.

---

## 5. Build — the pipeline processes and deploys

Developer runs:

```bash
dia pipeline --target cluichetest --stage build-assets
```

DiaPipeline reads `pipeline.toml`, finds the CluicheTest target:

```toml
[targets.cluichetest]
catalogue_manifest = "Assets/CluicheTest/assets.catalogue.json"
asset_stages = ["stage.main_menu", "stage.gameplay"]
```

DiaPipeline invokes DiaAssetPipeline. The pipeline:

### 5a. Read catalogue

```python
with open("Assets/CluicheTest/assets.catalogue.json") as f:
    catalogue = json.load(f)
```

Python — not C++ `CatalogueManifestSerializer`. No DiaAssetCatalogue dependency at build time (SD-APIPE-001).

### 5b. Filter by target stages

`asset_stages = ["stage.main_menu", "stage.gameplay"]`. The pipeline deploys:
- All assets scoped to those Stages
- All global-scoped assets

Assets scoped to Stages not in the target's list are skipped.

### 5c. Per-asset processing

For `texture.player_ship`:

1. **Validate** — texture handler checks source file exists, is a valid PNG
2. **Transform** — no transform registered for texture → copy-as-is (SD-APIPE-003)
3. **Deploy** — scope is `stage`, stage name is `Gameplay`, tag is `characters` →

```
Source: Assets/CluicheTest/Characters/player_ship.texture.png
Deploy: bin/CluicheTest/Debug/x64/assets/stages/Gameplay/characters/player_ship.texture.png
```

For a Folder asset like `folder.main_ui`:

```
Source: Assets/CluicheTest/UI/main_ui.folder/   (entire directory)
Deploy: bin/CluicheTest/Debug/x64/assets/global/Presentation/UI/main_ui.folder/
```

The directory tree is copied recursively, preserving internal structure (SD-CAT-013).

### 5d. Generate runtime manifest

After all assets are deployed, the pipeline generates `assets.runtime.json` — a lean lookup table with no source paths, no content hashes, no tags, no type metadata:

```json
{
    "assets": [
        {
            "id": "texture.player_ship",
            "scope": "stage",
            "deploy_path": "stages/Gameplay/characters/player_ship.texture.png"
        },
        {
            "id": "entity.player_ship",
            "scope": "stage",
            "deploy_path": "stages/Gameplay/characters/player_ship.entity.json"
        },
        {
            "id": "config.ship_stats",
            "scope": "global",
            "deploy_path": "global/gameplay/ship_stats.config.json"
        },
        {
            "id": "folder.main_ui",
            "scope": "global",
            "deploy_path": "global/Presentation/UI/main_ui.folder/"
        }
    ],
    "stages": [
        {
            "id": "stage.gameplay",
            "assets": [
                "texture.player_ship",
                "entity.player_ship",
                "config.ship_stats",
                "folder.main_ui"
            ]
        },
        {
            "id": "stage.main_menu",
            "assets": [
                "config.ship_stats",
                "folder.main_ui"
            ]
        }
    ]
}
```

Deployed to:

```
bin/CluicheTest/Debug/x64/assets/assets.runtime.json
```

The `stages` array is derived from `contains` relationships in the catalogue (SD-CAT-012). Global assets that appear in multiple Stages are listed in each Stage that uses them — this is how the runtime knows to ref-count them.

**The pipeline is the bridge.** It reads the catalogue format (Python `json.load`) and writes the runtime format. Neither C++ system knows the other's format (SD-CAT-004).

---

## 6. Startup — the game loads the manifest

Game starts. The `AssetRuntime` module wrapper calls:

```cpp
Dia::AssetRuntime::AssetRuntime runtime;
runtime.LoadManifest(FilePath("bin/CluicheTest/Debug/x64/assets/assets.runtime.json"));
runtime.RegisterListener(&myGraphicsSystem);
```

`RuntimeManifestLoader` deserializes the manifest into:
- **Asset table:** `HashTableC<StringCRC, RuntimeAssetEntry>` — one entry per asset
- **Stage table:** `HashTableC<StringCRC, RuntimeStageEntry>` — one entry per stage

Each `RuntimeAssetEntry` stores:
- `mId` — StringCRC (`"texture.player_ship"`)
- `mScope` — `AssetScope::kStage`
- `mDeployPath` — absolute path: `C:/GitHub/Cluiche/bin/CluicheTest/Debug/x64/assets/stages/Gameplay/characters/player_ship.texture.png`

Deploy paths are resolved to absolute by prepending the manifest's parent directory.

All assets start in `Registered` state. Path resolution works immediately — `ResolveAssetPath(StringCRC("texture.player_ship"))` returns the absolute path even before the asset is loaded.

**Late-joining listeners:** All listeners should register before any `RequestStageLoad` call. If a system registers after a Stage has already been loaded, it will NOT receive replayed `OnAssetReady` events. Instead, the late joiner must call `GetStagedAssets()` to discover assets already in `Staged` or `Loaded` state and self-serve (SD-ARUN-009):

```cpp
// Late registration — must catch up manually
runtime.RegisterListener(&myAudioSystem);

auto stagedAssets = runtime.GetStagedAssets();
for (const auto& entry : stagedAssets)
{
    myAudioSystem.OnAssetReady(entry.mId, entry.mDeployPath);
}
```

---

## 7. Stage load — the game requests assets

Game transitions to the Gameplay stage:

```cpp
runtime.RequestStageLoad(StringCRC("stage.gameplay"));
```

`AssetRuntime` looks up `RuntimeStageEntry` for `stage.gameplay`, expands to its member asset list:
- `texture.player_ship`
- `entity.player_ship`
- `config.ship_stats`
- `folder.main_ui`

For each asset:

1. **Stage-scoped assets** (first time seen) — ref count goes from 0 → 1, state transitions `Registered → Staged`, fires `OnAssetReady`
2. **Global assets already loaded** by another Stage — ref count incremented, no state change, no event
3. **Global assets not yet loaded** — ref count goes from 0 → 1, state transitions `Registered → Staged`, fires `OnAssetReady`

For `texture.player_ship` (stage-scoped, first load):

```
State: Registered → Staged
Event: OnAssetReady("texture.player_ship", 
    FilePath("C:/.../assets/stages/Gameplay/characters/player_ship.texture.png"))
```

---

## 8. Content load — the consumer responds

`MyGraphicsSystem` implements `IAssetStateListener`:

```cpp
void MyGraphicsSystem::OnAssetReady(const StringCRC& assetId, const FilePath& path)
{
    if (assetId == StringCRC("texture.player_ship"))
        mTextures.Load(assetId, path);  // Actually reads PNG bytes into GPU
}
```

DiaAssetRuntime does not load content — it resolves paths and manages state (SD-ARUN-006). The consuming system loads the actual bytes.

After loading, the consumer acknowledges:

```cpp
runtime.AcknowledgeAssetLoaded(StringCRC("texture.player_ship"));
// State: Staged → Loaded
```

The texture is now fully active.

---

## 9. Stage unload — assets are released

Game transitions away from Gameplay:

```cpp
runtime.RequestStageUnload(StringCRC("stage.gameplay"));
```

For each asset in the Stage:

- **Stage-scoped assets** — ref count drops to 0, state transitions `Loaded → Unloading`, fires `OnAssetUnloading`
- **Global assets still used by other Stages** — ref count decremented but stays > 0, no state change
- **Global assets no longer used by any Stage** — ref count drops to 0, transitions to `Unloading`

For `texture.player_ship` (stage-scoped, ref count 0):

```
State: Loaded → Unloading
Event: OnAssetUnloading("texture.player_ship")
```

The consumer releases the GPU texture and acknowledges:

```cpp
runtime.AcknowledgeAssetUnloaded(StringCRC("texture.player_ship"));
// State: Unloading → Registered
```

The asset is back to idle. Its path is still resolvable — it's just not loaded.

For `config.ship_stats` (global, also used by `stage.main_menu` which is still loaded):

```
Ref count: 2 → 1. No state change. No event fired.
```

---

## 10. Live debugging — the editor observes

While the game runs, a developer has `DiaAssetRuntimeEditor` open in CluicheEditor. It connects to the game via DiaDebugServer's WebSocket gateway.

The editor subscribes to `asset_runtime.subscribe_transitions` and receives a stream:

```json
{"asset": "texture.player_ship", "old": "Registered", "new": "Staged", "time": 1234567890}
{"asset": "texture.player_ship", "old": "Staged",     "new": "Loaded", "time": 1234567891}
```

The **Asset State Table** shows all assets color-coded by state. The **Stage/Asset Tree** shows `stage.gameplay` expanded with its four member assets. The **Ref Count Inspector** shows `config.ship_stats` with ref count 2 (referenced by both `stage.gameplay` and `stage.main_menu`).

All panels are read-only (SD-ARED-002). If the game disconnects, all panels grey out with "Not connected" and the toolbar's global indicator goes red (SD-ARED-005).

---

## Summary — the full path

```
1. Author        Assets/CluicheTest/Characters/player_ship.texture.png
       │
2. Discover      DiaAssetCatalogueEditor → AssetTypeRegistry pattern match → AssetRecord
       │
3. Rules         CatalogueRulesEngine → tag, stage membership, scope
       │
4. Save          CatalogueManifestSerializer → Assets/CluicheTest/assets.catalogue.json
       │
5. Build         DiaAssetPipeline (Python json.load) → validate → deploy → generate
       │               ↓
       │         bin/.../assets/stages/Gameplay/characters/player_ship.texture.png
       │         bin/.../assets/assets.runtime.json
       │
6. Startup       RuntimeManifestLoader → asset table + stage table
       │
7. Stage load    RequestStageLoad("stage.gameplay") → Registered → Staged → OnAssetReady
       │
8. Content load  MyGraphicsSystem::OnAssetReady → loads PNG → AcknowledgeAssetLoaded → Loaded
       │
9. Stage unload  RequestStageUnload → Loaded → Unloading → OnAssetUnloading → Registered
       │
10. Debug        DiaAssetRuntimeEditor observes state transitions over WebSocket
```

---

## Key decisions traced in this journey

| Decision | Where it matters |
|----------|-----------------|
| SD-CAT-001 — `type.name` composite IDs | Step 2: ID is `texture.player_ship` |
| SD-CAT-004 — Two manifests | Steps 4-5: catalogue → pipeline → runtime manifest |
| SD-CAT-007 — Scope and tags drive deploy layout | Step 5c: `stage/Gameplay/characters/` |
| SD-CAT-008 — Automation in DiaAssetCatalogue, not editor | Step 3: rules engine is C++ API |
| SD-CAT-009 — Array order evaluation | Step 3: rules 1-4 in sequence |
| SD-CAT-010 — Dry-run and conflict detection | Step 3: preview before apply |
| SD-CAT-012 — Membership in RelationshipIndex, not stage file | Steps 3-5: `contains` edges define stage membership |
| SD-CAT-013 — Folder assets deploy as directory trees | Step 5c: recursive copy |
| SD-APIPE-001 — Pipeline reads catalogue, generates runtime | Step 5: Python bridge |
| SD-ARUN-002 — Stage is the unit of load/unload | Steps 7, 9: `RequestStageLoad/Unload` |
| SD-ARUN-003 — Global assets ref-counted | Step 9: `config.ship_stats` stays loaded |
| SD-ARUN-004 — Consumers acknowledge load/unload | Step 8: `AcknowledgeAssetLoaded` |
| SD-ARUN-006 — No content loading in DiaAssetRuntime | Step 8: consumer loads bytes |
| SD-ARED-002 — Read-only editor | Step 10: observe only |
