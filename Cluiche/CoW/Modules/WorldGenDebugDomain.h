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
// State JSON contract (consumed by debug-panel.html; compact size must stay under 12 KB at Large):
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

#include <string>

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

    void GetJSONState(Json::Value& out) override;
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
};

} // namespace CoW

#endif // DIA_DEBUG
