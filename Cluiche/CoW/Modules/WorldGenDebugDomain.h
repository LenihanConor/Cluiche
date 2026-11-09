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
//   selectCell   {x, y} / clearCell                   -> domain-held selection
//                TODO(plan task 10): route click selection through DiaCellLayers cells.select.cell instead.
//   setColourMode {mode}                              -> View colour layer
//   toggle       {drawer}                             -> flips a View drawer ("Tiles", "Grid lines", ...)
//
// State JSON contract (consumed by debug-panel.html; compact size must stay under 12 KB at Large).
// The state is split into a STATIC part (control definitions, preset list, size dimensions, view modes, stage names) and a
// DYNAMIC part (everything else). A frame is either
//   FULL  ("full": true)  : dynamic + static fields, i.e. the complete shape documented below; or
//   LEAN  ("full": false) : dynamic fields only. Static fields are omitted: controls[] are {key, value}, sizes[] are
//                           {name, selected}, timings[] are {stage, ms, ran}, presets and view.modes are absent.
// Every frame carries "staticVersion" (int, bumped whenever the static content changes). A FULL frame is sent for the first
// frame, whenever staticVersion changes, and every kFullFrameInterval frames (so a reloaded page recovers within ~1 s). The
// page keeps the static fields of the last FULL frame and merges them into LEAN frames (debug-panel.html wgResolveState).
// Why: the static JSON was rebuilt (preset list = directory scan) and re-serialised, parsed and pushed every Sim frame and
// pushed MainPU/SimPU past their 33.3 ms budget in Debug.
//   full, staticVersion : bool, int
//   drawers  : []                                     (no world drawers)
//   stats    : {status, revision, seed, size, width, height, locked(false), lastMs,
//               preset (id or ""), presetModified, stagesRan[names]}
//   controls : [{key,label,type:"slider",min,max,step,value,default,stage,stageName}] one per WorldShapeParams key.
//              barrierForm is {type:"select", values[], value:string, default:string} (no min/max/step).
//              step is 1 for integer params, 0.01 for floats; stage = first TerrainStage affected (enum index).
//   presets  : [{id, displayName}]
//   sizes    : [{name, width, height, selected}]
//   timings  : [{stage, name, ms, ran}]               (per-stage ms of the last generation, ran = re-run last Update)
//   view     : {colourMode, modes[names], drawers:[{name, enabled, available}]}
//              modes = "terrain" followed by every terrain layer name; drawers = Tiles, Grid lines, Selection (available)
//              then Region edges, Sites, Evidence (available=false).
//   cell     : null, or session.Cell({x,y}) result (all fields, layers{}, neighbours[]) plus "selected":{x,y}.
//              Selection out of range / before the first world => cell null and "cellError" string.
//   lastSave : null, or the last presetSave result ({success, writtenTo, isSource} or {success:false, error, status?}).
#pragma once
#ifdef DIA_DEBUG

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

private:
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
        std::vector<std::string> timingNames;    // by stage index
    };
    void RebuildStatic();

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
