// S-1 World Generation: typed command model behind the cow.worldgen.* commands (terrain slice).
// Pure logic: owns the pending seed/shape/size, the TerrainGenerator and the generated WorldStructure.
// Actions (Set, SetSize, Regenerate) only mutate pending state and mark the session dirty; Tick runs at
// most one generation, so several actions between ticks coalesce into a single regeneration.
// No Application, bus, DiaAPI or Dia module state: the module/command layer wraps this class.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#pragma once

#include <Shared/WorldGeneration/GenerationGuard.h>
#include <Shared/WorldGeneration/GenerationReport.h>
#include <Shared/WorldGeneration/GenerationRun.h>
#include <Shared/WorldGeneration/LayoutCheck.h>
#include <Shared/WorldGeneration/RegionsAndSites.h>
#include <Shared/WorldGeneration/SiteRegistry.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <memory>
#include <string>

namespace Dia::Attribute { class ParameterFile; struct ParamError; }

namespace CoW {
namespace WorldGeneration {

// Generating: a Regenerate is stepping through its attempts (one per Tick); the world and revision are still the
// previous ones. Failed: the last Regenerate ended without a world (the previous world, if any, was kept).
enum class WorldGenState : uint8_t { Idle, Complete, Failed, Generating };
enum class WorldSize : uint8_t { Small, Standard, Large };

const char*    WorldGenStateName(WorldGenState state);
const char*    WorldSizeName(WorldSize size);
GridDimensions WorldSizeDimensions(WorldSize size);

class WorldGenSession
{
public:
    // A default-constructed session has NO rules: Set, SetSize, Regenerate, PresetLoad, ImportWorld and Validate
    // refuse with {success:false, error:"rules_not_loaded"} and Tick generates nothing until LoadRules succeeds
    // (there is no compiled fallback; the rules come from the terrain_rules sidecar).
    WorldGenSession();
    // A session with caller-supplied rules (already loaded; tests that vary one rule use this).
    explicit WorldGenSession(const TerrainRules& rules);
    ~WorldGenSession();
    WorldGenSession(WorldGenSession&&) noexcept;
    WorldGenSession& operator=(WorldGenSession&&) noexcept;

    // Loads the terrain_rules sidecar text (a Dia parameter file) and binds the session's rules to it. On failure
    // returns false with `err` naming the key and leaves the session (rules, loaded flag, world) unchanged. A
    // configuration-time call: refused (err.key "state") while a Regenerate runs or once a world exists.
    bool LoadRules(const char* sidecarText, Dia::Attribute::ParamError& err);
    bool RulesLoaded() const { return mRulesLoaded; }
    const TerrainRules& Rules() const { return mRules; }

    // Actions: mutate pending state, mark dirty when something changed, return the command's JSON result.
    Json::Value Set(const Json::Value& args);          // any shape param key, "seed", "unlock" (ignored)
    Json::Value SetSize(const Json::Value& args);      // {"size":"Small"|"Standard"|"Large"}
    Json::Value Regenerate(const Json::Value& args);   // {"seed"?, "maxRetryAttempts"?, "timeoutSeconds"?}: full re-run from scratch
    // Cancels the Regenerate in progress (or drops one still pending): the previous world is kept. {} -> {success,
    // cancelled, ...}; cancelled is false (still success) when there is nothing to cancel. Cancel is the one action
    // allowed while a run is active; Set, SetSize, PresetLoad, Regenerate, ImportWorld and ExportWorld refuse with
    // {success:false, error:"busy", busy:true, generating:true} until the run ends.
    Json::Value Cancel(const Json::Value& args);

    // Presets: "deployed" is the shipped read-only directory, "out" the writable one (wins on equal id).
    // An empty path disables that source.
    void        SetPresetDirectories(const std::string& deployedDir, const std::string& outDir);
    Json::Value PresetLoad(const Json::Value& args);   // {"id", "unlock"?}: replaces all shape params, keeps seed
    Json::Value PresetSave(const Json::Value& args);   // {"id","displayName","description"?,"overwrite"?}: out dir only

    // World files (WorldJson.h / WorldFiles.h): where `<name>.wld.json` is written (out) and also read from
    // (deployed, read-only). An empty path disables that source.
    void        SetWorldDirectories(const std::string& deployedDir, const std::string& outDir);
    // {"name"?, "overwrite"?}: writes the current world (terrain, biomes, regions, sites, provenance) to
    // `<out>/<name>.wld.json`; the default name is the seed. Reply: {success, name, path, bytes, warnings[]}.
    Json::Value ExportWorld(const Json::Value& args) const;
    // {"name"}: reads `<name>.wld.json` (out directory first, then deployed) and, only if it validates completely,
    // replaces the world with it. The session adopts the file's seed, shape params, size and preset, the generator
    // layers are rebuilt from the cells, and the world opens LOCKED (SD-S1-009): Set, SetSize, PresetLoad and
    // Regenerate refuse with {success:false, error:"locked", locked:true, needsConfirm:true} unless they carry
    // "unlock":true, which drops the hand edits and regenerates from the recorded recipe. A failed import changes
    // nothing. Reply: {success, name, path, revision, locked, warnings[]} or {success:false, error, field}.
    Json::Value ImportWorld(const Json::Value& args);

    // Runs the validation set on the CURRENT world (the terrain navigation profile, HearthNexus, DynastySpacing, the
    // registry's guarantees, WorldLayout when regions are fresh, and WorldThrone when the world holds throne sites)
    // without touching the world or its revision, and keeps the result as the one fresh-evidence slot, keyed to the
    // revision it was computed for. Refuses like the other actions while a Regenerate runs.
    // Reply: {success, ok, revision, guarantees[{id, name, ok, counts{proofPaths,violatingCells,violatingPairs}, diagnostics[]}]}.
    Json::Value Validate(const Json::Value& args);
    // Merged listing of the out and deployed worlds directories (out wins on a name clash): {success, worlds[{name, source}]}.
    Json::Value Worlds() const;
    // The evidence slot: the last Validate result, valid for the world revision it carries (stale once the revision moves).
    bool                    HasEvidence() const        { return mHasEvidence; }
    uint32_t                EvidenceRevision() const   { return mEvidenceRevision; }
    const ValidationResult& Evidence() const           { return mEvidence; }

    // Site registry used by the placement pass; nullptr (default) means no registered kinds. Not owned: the owner
    // (WorldGenerationModule) keeps it alive for the session's lifetime.
    void SetSiteRegistry(const SiteRegistry* registry) { mRegistry = registry; }
    // The registry in use (an empty one when none is set).
    const SiteRegistry& Registry() const { return mRegistry != nullptr ? *mRegistry : mEmptyRegistry; }

    // Runs at most one generation step if dirty or if a Regenerate is in progress. Returns true when the world changed
    // (revision incremented).
    // Regions and sites are computed on Regenerate only, by a GenerationRun (retry loop over RegionsAndSites::Place,
    // GenParams::maxRetryAttempts attempts, GenParams::timeoutSeconds budget, terrain fixed); any other generation
    // (first one, Set / SetSize / preset load = live preview) leaves them empty (regionsFresh = false) and never retries.
    // A Regenerate generates its candidate terrain on the first Tick and runs ONE attempt per Tick, so Cancel is
    // handled between frames; one that succeeds on attempt 1 finishes in that same Tick. The run keeps the random
    // stream it started with. While it runs the session keeps the previous world, generator layers and revision.
    // Success: the candidate replaces the world (revision + 1, state Complete). Unsatisfiable, Timeout, Error or
    // Cancelled with a previous world: the candidate is discarded, the previous world, revision, regions, sites,
    // layers, lock and imported flags are untouched, the pending seed/shape/size/preset revert to the accepted ones,
    // the report records the failure (worldRevision 0) and Tick returns false; state is Failed (Cancelled returns to
    // the previous state). With NO previous world (the first generation was a Regenerate) there is nothing to keep:
    // the candidate terrain is adopted with regions and sites cleared, state Failed, revision + 1 and Tick returns true.
    bool Tick(Dia::Maths::RandomStream& wldStream);
    bool IsGenerating() const { return mRun != nullptr; }
    // Test seam: the clock the wall-clock budget reads (nullptr = steady clock). Not owned; set before the Regenerate.
    void SetGenerationClock(const IGenerationClock* clock) { mClock = clock; }

    // Queries
    Json::Value Params() const;
    Json::Value Status() const;
    Json::Value Presets() const;                       // array of {id, displayName, description, version, sourcePath}
    Json::Value Layers(const Json::Value& args) const; // {} stage summary, or {"layer"} full values
    Json::Value Cell(const Json::Value& args) const;   // {"x","y"} or {"index"}
    Json::Value Regions() const;                       // every RegionRecord + table summary + fresh
    Json::Value Sites(const Json::Value& args) const;  // {"kind"?,"offset"?,"limit"?}: paged sites + kind table
    // {"offset"?,"limit"?}: the last GenerationReport, attempts paged. finalValidation carries per-guarantee evidence:
    // counts plus the proofPaths/violatingCells/violatingPairs arrays, capped per page ("evidenceOffset"?, "evidenceLimit"?,
    // "guarantee"? = guarantee name or id to restrict to one) with "truncated" set when more remain. It is the fresh
    // Validate evidence when that belongs to the current revision, else the generation's final validation.
    Json::Value Report(const Json::Value& args) const;

    // The last finished generation's report (replaced by every Regenerate); nullptr before the first.
    const GenerationReport* LastReport() const { return mHasReport ? &mReport : nullptr; }
    uint32_t                ReportRevision() const { return mReportRevision; }
    // True when the last finished Regenerate ended without a new world and the previous world was kept (failed or cancelled
    // with a world to fall back on). False after a success and when the failed run was the very first generation.
    bool                    LastRunKeptPreviousWorld() const { return mLastRunKeptPrevious; }
    // True after a failed run kept the previous world and no Validate has run since: all evidence is hidden.
    bool                    EvidenceHiddenByFailure() const { return mLastRunKeptPrevious && !mValidatedSinceFailure; }
    // Display name of a guarantee id (the known built-ins by name, else "0x%08X"), as used by Report and Validate.
    static std::string      GuaranteeLabel(uint32_t idCrc);
    uint32_t                MaxRetryAttempts() const { return mGenParams.maxRetryAttempts; }
    // Config-time setter (clamped to 1..64); does not trigger a generation.
    void                    SetMaxRetryAttempts(uint32_t attempts) { mGenParams.maxRetryAttempts = ClampMaxRetryAttempts(attempts); }
    float                   TimeoutSeconds() const { return mGenParams.timeoutSeconds; }
    // Config-time setter (ClampTimeoutSeconds); does not trigger a generation.
    void                    SetTimeoutSeconds(float seconds) { mGenParams.timeoutSeconds = ClampTimeoutSeconds(seconds); }

    const WorldStructure&   World() const      { return mWorld; }
    uint32_t                Revision() const   { return mRevision; }
    uint64_t                Seed() const       { return mSeed; }
    const WorldShapeParams& Shape() const      { return mShape; }
    const std::string&      PresetId() const   { return mPresetId; }       // cheap reads for per-frame callers (Params() formats every float)
    bool                    PresetModified() const { return mPresetModified; }
    GridDimensions          Dimensions() const { return mDims; }
    const TerrainGenerator& Generator() const  { return mGen; }
    WorldGenState           State() const      { return mState; }
    bool                    IsDirty() const    { return mDirty; }
    bool                    RegionsFresh() const { return mRegionsFresh; }
    bool                    IsLocked() const   { return mLocked; }         // an imported world that has not been unlocked
    bool                    IsImported() const { return mImported; }       // the world came from a file, not the generator

private:
    TerrainRules     mRules;
    std::unique_ptr<Dia::Attribute::ParameterFile> mRulesFile;   // the loaded sidecar (null when rules were supplied directly)
    bool             mRulesLoaded = false;
    TerrainGenerator mGen;
    WorldStructure   mWorld;
    uint64_t         mSeed = 1;
    WorldShapeParams mShape;
    GridDimensions   mDims;
    std::string      mPresetId;               // empty when none or modified
    bool             mPresetModified = false;
    WorldGenState    mState = WorldGenState::Idle;
    uint32_t         mRevision = 0;
    bool             mDirty = true;           // a first generation is pending from construction
    bool             mForceFull = false;      // Regenerate: Invalidate() before the next Update
    std::string      mDeployedDir;
    std::string      mOutDir;
    bool             mHasGenerated = false;
    float            mLastMs = 0.0f;
    Json::Value      mStagesRan{Json::arrayValue};

    // Regions and sites (S-1 placement pass).
    const SiteRegistry*  mRegistry = nullptr;
    SiteRegistry         mEmptyRegistry;          // used when mRegistry is null
    bool                 mRegionsFresh = false;   // the world's regions/sites belong to its current terrain
    LayoutLimits         mLimits;
    LayoutReport         mLayout;
    PlaceResult          mPlaceResult;

    // Imported worlds (Feature 4, SD-S1-009).
    std::string          mWorldsDeployedDir;
    std::string          mWorldsOutDir;
    bool                 mLocked = false;     // set by ImportWorld; cleared by an action carrying "unlock":true
    bool                 mImported = false;   // cleared by the next generation

    // Lock gate for the actions: true if the action may proceed. A locked session refuses unless args.unlock is true
    // (then *unlocking is set and the caller calls DropLock() once the action is known to proceed).
    bool CheckLock(const Json::Value& args, Json::Value& refusal, bool& unlocking) const;
    void DropLock();

    // Fresh evidence (Validate): one slot, valid while mEvidenceRevision == mRevision.
    ValidationResult     mEvidence;
    bool                 mHasEvidence = false;
    uint32_t             mEvidenceRevision = 0;

    // Retry (Feature 3).
    GenParams            mGenParams;
    GenerationReport     mReport;
    bool                 mHasReport = false;
    uint32_t             mReportRevision = 0;
    bool                 mLastRunKeptPrevious = false;
    bool                 mValidatedSinceFailure = false;

    // Timeout and failure handling (Feature 5): the Regenerate in progress. The candidate generator and terrain are
    // separate from mGen / mWorld, which stay the accepted ones until the run succeeds.
    const IGenerationClock*            mClock = nullptr;
    std::unique_ptr<TerrainGenerator>  mCandGen;
    WorldStructure                     mCandTerrain;      // the run reads this by reference: it must not move
    std::unique_ptr<GenerationGuard>   mGuard;
    std::unique_ptr<GenerationRun>     mRun;

    // What the last accepted world was generated/imported with: restored when a Regenerate fails.
    struct Accepted
    {
        uint64_t         seed = 1;
        WorldShapeParams shape;
        GridDimensions   dims;
        std::string      presetId;
        bool             presetModified = false;
        bool             locked = false;
        WorldGenState    state = WorldGenState::Idle;
    };
    Accepted             mAccepted;

    bool BusyRefusal(Json::Value& refusal) const;
    // Refusal for every generation entry point until the rules are loaded: {success:false, error:"rules_not_loaded"}.
    bool RulesRefusal(Json::Value& refusal) const;
    // Ids of the map layers (WorldLayerSource) that hold no data for the current world: all of them before the first
    // world, "regions" until regions are fresh, and the generator-only layers of an imported world.
    Json::Value UnavailableLayers() const;
    void CaptureAccepted();
    bool StartRegenerate(Dia::Maths::RandomStream& wldStream);
    bool AdvanceRun();
    bool CommitRun();
    bool FailRegenerate(const GenerationReport& report);
    void ReleaseRun();
    void RefreshStageInfo();
};

} // namespace WorldGeneration
} // namespace CoW
