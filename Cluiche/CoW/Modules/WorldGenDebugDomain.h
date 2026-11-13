// S-1 World Generation panel: IDebugDomain card for DiaDebugPanel.
//
// THREADING: this domain runs ON the SimPU, next to WorldGenerationModule, and touches its session directly.
// It must never be read from Main; it is exposed to Main by Cluiche::AppFlow::DomainStateRelayModule::Attach(domain)
// (wired in the stage module). Do not copy GameSpeedDebugDomain's "read the module from Main" shape.
//
// Every command goes through the SAME WorldGenSession methods the cow.worldgen.* commands call (SD-S1-002),
// then WorldGenerationModule::NotifyAction().
//
// Commands (args JSON; unknown commands and bad args are ignored with a DIA_LOG_WARNING on channel "CoW"):
//   setScale     {key, value}                         -> session.Set({key: value}) (any shape param key, or "seed")
//   regenerate   {seed?}                              -> session.Regenerate
//   setSeed      {seed}                               -> session.Set({seed})
//   setSize      {size: Small|Standard|Large}         -> session.SetSize
//   presetLoad   {id}                                 -> session.PresetLoad
//   presetSave   {id, displayName, description?, overwrite?} -> session.PresetSave (result kept as "lastSave")
//   selectCell   {x, y} / clearCell                   -> view->SelectCell / ClearSelection (domain-held selection when no view)
//   setColourMode {mode}                              -> view->SetColourMode ("terrain" = the viewer's "relief"); ignored+warned if unknown
//   toggle       {drawer}                             -> view->SetOverlayEnabled ("rivers", "selection.neighbours"); unavailable ones ignored
//                (no view: flips a local drawer, "Tiles", "Grid lines", ...)
//   validate     {}                                   -> session.Validate (cow.worldgen.validate); a refusal shows as "validateError"
//   export       {name?, overwrite?}                  -> module.ExportWorldFile (cow.worldgen.export); result kept as file.lastExport
//   import       {name}                               -> module.ImportWorldFile (cow.worldgen.import); result kept as file.lastImport
//   cancel       {}                                   -> session.Cancel (cow.worldgen.cancel)
//   selectWorld  {name}                               -> file.selected (a name from the merged worlds list; others ignored)
//   discard      {cmd?, args?}                        -> SD-S1-009 "Discard": drops a locked import's edits through the existing
//                "unlock":true arg. With {cmd, args} (cmd one of setScale/setSeed/setSize/presetLoad/regenerate, args as that
//                command takes them) the pending change the locked world refused is replayed with unlock:true; without, the
//                world is unlocked and regenerates from its recorded recipe (session.Set({unlock:true})). Not locked: no-op.
//                "Keep" needs no command: the panel restores the slider. Every shape command also accepts "unlock":true itself.
//
// State JSON contract (consumed by debug-panel.html; compact size must stay under 12 KB at Large).
// The state is split into a STATIC part (control definitions, preset list, size dimensions, view modes, stage names) and a
// DYNAMIC part (everything else). A frame is either
//   FULL  ("full": true)  : dynamic + static fields, i.e. the complete shape documented below; or
//   LEAN  ("full": false) : dynamic fields only. Static fields are omitted: controls[] are {key, value}, sizes[] are
//                           {name, selected}, timings[] are {stage, ms, ran}, presets, view.modes and view.layers are absent.
// Every frame carries "staticVersion" (int, bumped whenever the static content changes). A FULL frame is sent for the first
// frame, whenever staticVersion changes, and every kFullFrameInterval frames (so a reloaded page recovers within ~1 s). The
// page keeps the static fields of the last FULL frame and merges them into LEAN frames (debug-panel.html wgResolveState).
// Why: the static JSON was rebuilt (preset list = directory scan) and re-serialised, parsed and pushed every Sim frame and
// pushed MainPU/SimPU past their 33.3 ms budget in Debug.
//   full, staticVersion : bool, int
//   drawers  : []                                     (no world drawers)
//   stats    : {status, revision, seed, size, width, height, locked, source, lastMs,
//               preset (id or ""), presetModified, stagesRan[names]}
//   controls : [{key,label,type:"slider",min,max,step,value,default,stage,stageName}] one per WorldShapeParams key.
//              barrierForm is {type:"select", values[], value:string, default:string} (no min/max/step).
//              step is 1 for integer params, 0.01 for floats; stage = first TerrainStage affected (enum index).
//   presets  : [{id, displayName}]
//   sizes    : [{name, width, height, selected}]
//   timings  : [{stage, name, ms, ran}]               (per-stage ms of the last generation, ran = re-run last Update)
//   view     : {colourMode, modes[names], layers[{id,label,group}], drawers:[{name, enabled, available}], legend?}
//              layers = [{id, label, group}] (STATIC, same order as modes, back-compat): the picker entries. "terrain" is
//              labelled "Relief"; label = humanised id ("terrainType" -> "Terrain type"); group = the WorldLayerSource group
//              (view: "Terrain", ...; no view: "Terrain" for the Relief entry, "Layers" for the rest).
//              No view (fallback): modes = "terrain" followed by every terrain layer name; drawers = Tiles, Grid lines, Selection
//              (available) then Region edges, Sites, Evidence (available=false); no legend key.
//              With a view (IWorldGenView, the S-1 stage): colourMode = view->GetColourMode() with "relief" reported as "terrain";
//              modes = "terrain" then every other registered base layer id (STATIC part: only changes when the registered layers
//              change, bumping staticVersion); drawers = Tiles (always on), one entry per viewer overlay ("rivers",
//              "selection.neighbours"; available = has something to draw), Selection (always on); legend (DYNAMIC part, it follows
//              the active base layer and the world) = view->FillLegend: null (none yet), a [{label, colour:"#rrggbb"}] array for
//              category layers, or {min, max, ramp, unit, stops:[{t, colour}]} for float layers.
//   cell     : null, or session.Cell({x,y}) result (all fields, layers{}, neighbours[]) plus "selected":{x,y}.
//              With a view the selection is the viewer's first selected cell (so click selection shows up here too).
//              Selection out of range / before the first world => cell null and "cellError" string.
//              cell also carries "absorbed" (bool): the cell sits in a non-island region of another depth zone (an undersize-merge
//              or fragment cell, regions-and-sites.md); false without regions.
//   lastSave : null, or the last presetSave result ({success, writtenTo, isSource} or {success:false, error, status?}).
// Panel sections (worldgen-panel-rest plan task 2). All three are DYNAMIC (sent in every frame, lean or full):
//   validation : null until a generation report or a Validate result exists, else
//                {outcome, seed, worldRevision, fresh, passed, source, guarantees[], attemptCount, attempts[]}.
//                outcome = the last report's outcome text (Complete/Unsatisfiable/TIMEOUT/CANCEL/FAILURE) or "Validated" without a report;
//                source = "validate" (fresh Validate evidence for this revision) | "generation" (the report's final validation);
//                fresh = the rows describe the CURRENT world (Validate ran for this revision, or the report produced it);
//                worldRevision = the revision the rows belong to (0 for a failed run: no world came out of it);
//                guarantees[] = {id, name, ok, counts{proofPaths,violatingCells,violatingPairs}, diagnostics[<=4 strings]};
//                attempts[] (first 12 of attemptCount) = {index, subSeed, failedGuarantees[names], ms}.
//                No proof paths or violating cells here: evidence geometry is consumed by the overlays on the SimPU, not the panel.
//   validateError : only present (string) while the last validate command was refused ("busy", "no world generated yet").
//   file       : {worlds[{name,source}], selected, defaultName, lastExport, lastImport, locked, imported, origin?}
//                worlds = the merged out+deployed list (cow.worldgen.worlds; re-read every 5 s, after export/import/selectWorld
//                and on full frames); selected = the selectWorld choice ("" if none); defaultName = the export name used when none is
//                given (from the seed); lastExport/lastImport = null or {success, name, path, bytes, revision, warningCount,
//                warnings[<=3], error, status, field, line, column} (line/column only on a malformed import file); locked / imported
//                = the session flags (SD-S1-009); origin = {seed, generatorVersion, rulesVersion, preset} of the current world.
//   progress   : {state, attempt, maxAttempts, elapsedMs, timeoutMs, lastOutcome}
//                state = Idle | Generating | Complete | Failed; after a failed or cancelled Regenerate that kept the previous
//                world the pill reads Complete (the card is the red one). attempt/maxAttempts/elapsedMs/timeoutMs are 0 unless
//                Generating. lastOutcome = null, or the card
//                  {kind:"generation", outcome, message, attempts, maxAttempts, revision, previousWorldKept} (outcome =
//                   Complete | Unsatisfiable | TIMEOUT | CANCEL | FAILURE), or
//                  {kind:"import", outcome:"Imported", name, revision, seed, previousWorldKept:false}.
//                An imported world shows the import card, never a generation outcome; a later generation report replaces it
//                (a failed Regenerate on an imported world keeps the import in force: previousWorldKept true), a live preview
//                (Set) clears it. A failed import is not a card: its error is file.lastImport.
#pragma once
#ifdef DIA_DEBUG

#include "Modules/IWorldGenView.h"
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <chrono>
#include <string>
#include <vector>

namespace CoW {

class WorldGenerationModule;

class WorldGenDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit WorldGenDebugDomain(WorldGenerationModule& module);

    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    // A full frame is sent every this many GetJSONState calls (30 calls is about 1 s at the 30 Hz Sim snapshot rate).
    static constexpr int kFullFrameInterval = 30;
    // The preset list is re-read from disk at most this often (and right after a presetSave).
    static constexpr int kStaticRefreshSeconds = 5;

    void GetJSONState(Json::Value& out) override;
    void ForceFullFrame() { mForceFull = true; }
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // Nullable. With a view set, selection / colour mode / overlays live in the viewer (single source of truth) and the
    // domain's own state below is unused; without one the domain keeps its local fallback state (unit tests, headless).
    // Must be set and cleared on the SimPU, and cleared before the view is destroyed.
    void SetView(IWorldGenView* view);

private:
    IWorldGenView* mView = nullptr;
    struct Drawer { const char* name; bool enabled; bool available; };

    WorldGenerationModule& mModule;
    bool        mHasSelection = false;
    int         mSelX = 0;
    int         mSelY = 0;
    std::string mColourMode = "terrain";
    Drawer      mDrawers[6];
    Json::Value mLastSave;   // null until a presetSave ran

    // Cached static part (see "State JSON contract").
    struct StaticPart
    {
        std::vector<std::string> controlKeys;    // display order (grouped by first stage)
        Json::Value controlDefs;                 // key -> full control object without "value"
        Json::Value presets;                     // [{id, displayName}]
        Json::Value modes;                       // ["terrain", layer names...]
        Json::Value layers;                      // [{id, label, group}] (picker), same order as modes
        std::vector<std::string> timingNames;    // by stage index
    };
    void RebuildStatic();

    // Panel sections (dynamic).
    Json::Value BuildValidation() const;
    Json::Value BuildFile(bool full);
    Json::Value BuildProgress();
    void        UpdateOutcomeCard();
    void        RefreshWorlds();

    // File / outcome state.
    Json::Value  mLastExport;            // compact result, null until an export command ran
    Json::Value  mLastImport;            // compact result, null until an import command ran
    std::string  mValidateError;         // empty unless the last validate command was refused
    std::string  mSelectedWorld;
    Json::Value  mWorlds;                // cached merged list (directory scan)
    bool         mWorldsBuilt = false;
    bool         mWorldsDirty = false;
    std::chrono::steady_clock::time_point mWorldsBuiltAt;
    Json::Value  mCard;                  // the last-outcome card, null if none
    uint32_t     mSeenReportRevision = 0;
    uint32_t     mCardImportRevision = 0;
    std::string  mImportName;            // name of the last import this domain ran
    uint32_t     mImportNameRevision = 0;

    // Last param values (key -> value), refreshed only when the session's shape changes.
    Json::Value                       mValues;
    WorldGeneration::WorldShapeParams mValuesShape;
    bool                              mHaveValues = false;

    StaticPart mStatic;
    bool       mStaticBuilt     = false;
    bool       mStaticDirty     = false;   // a presetSave ran: re-read the preset list now
    int        mStaticVersion   = 0;
    int        mLastFullVersion = -1;
    int        mFramesSinceFull = 0;
    bool       mForceFull       = false;
    std::chrono::steady_clock::time_point mStaticBuiltAt;
};

} // namespace CoW

#endif // DIA_DEBUG
