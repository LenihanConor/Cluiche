// S-1 World Generation: typed command model behind the cow.worldgen.* commands (terrain slice).
// Pure logic: owns the pending seed/shape/size, the TerrainGenerator and the generated WorldStructure.
// Actions (Set, SetSize, Regenerate) only mutate pending state and mark the session dirty; Tick runs at
// most one generation, so several actions between ticks coalesce into a single regeneration.
// No Application, bus, DiaAPI or Dia module state: the module/command layer wraps this class.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/f1-debug-host-integration.md (Command set: terrain slice)
#pragma once

#include <Shared/WorldGeneration/TerrainGenerator.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>
#include <Shared/WorldGeneration/WorldShapePreset.h>

#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <string>

namespace CoW {
namespace WorldGeneration {

enum class WorldGenState : uint8_t { Idle, Complete, Failed };
enum class WorldSize : uint8_t { Small, Standard, Large };

const char*    WorldGenStateName(WorldGenState state);
const char*    WorldSizeName(WorldSize size);
GridDimensions WorldSizeDimensions(WorldSize size);

class WorldGenSession
{
public:
    WorldGenSession();
    explicit WorldGenSession(const TerrainRules& rules);

    // Actions: mutate pending state, mark dirty when something changed, return the command's JSON result.
    Json::Value Set(const Json::Value& args);          // any shape param key, "seed", "unlock" (ignored)
    Json::Value SetSize(const Json::Value& args);      // {"size":"Small"|"Standard"|"Large"}
    Json::Value Regenerate(const Json::Value& args);   // {"seed"?}: full re-run from scratch

    // Presets: "deployed" is the shipped read-only directory, "out" the writable one (wins on equal id).
    // An empty path disables that source.
    void        SetPresetDirectories(const std::string& deployedDir, const std::string& outDir);
    Json::Value PresetLoad(const Json::Value& args);   // {"id", "unlock"?}: replaces all shape params, keeps seed
    Json::Value PresetSave(const Json::Value& args);   // {"id","displayName","description"?,"overwrite"?}: out dir only

    // Runs at most one generation if dirty. Returns true when the world changed (revision incremented).
    bool Tick(Dia::Maths::RandomStream& wldStream);

    // Queries
    Json::Value Params() const;
    Json::Value Status() const;
    Json::Value Presets() const;                       // array of {id, displayName, description, version, sourcePath}
    Json::Value Layers(const Json::Value& args) const; // {} stage summary, or {"layer"} full values
    Json::Value Cell(const Json::Value& args) const;   // {"x","y"} or {"index"}

    const WorldStructure&   World() const      { return mWorld; }
    uint32_t                Revision() const   { return mRevision; }
    uint64_t                Seed() const       { return mSeed; }
    const WorldShapeParams& Shape() const      { return mShape; }
    GridDimensions          Dimensions() const { return mDims; }
    const TerrainGenerator& Generator() const  { return mGen; }
    WorldGenState           State() const      { return mState; }
    bool                    IsDirty() const    { return mDirty; }

private:
    TerrainRules     mRules;
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
};

} // namespace WorldGeneration
} // namespace CoW
