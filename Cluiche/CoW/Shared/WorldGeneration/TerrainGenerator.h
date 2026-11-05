// S-1 Terrain Generator: staged, incrementally re-evaluated terrain pipeline.
// Stage 0 (SeedFields) reseeds the WLD stream and caches all random draws; later stages are
// pure functions of (cached draws, sliders, rules, grid). Dirty propagation is data-driven
// (kStageReads + FirstStageAffected) so a slider change re-runs only the affected stages.
//
// Adding a per-cell layer: add ONE row to TERRAIN_LAYERS_TABLE below (name, type, producing stage).
// Storage (one std::vector per layer, sized width*height by the generator) and lookup follow
// automatically; the producing stage's RunStage case writes it through the generator's
// TerrainLayers friend access (FloatLayer(index) / UIntLayer(index)).
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma once

#include <Shared/WorldGeneration/TerrainDepthZones.h>
#include <Shared/WorldGeneration/TerrainDrainage.h>
#include <Shared/WorldGeneration/TerrainLandmass.h>
#include <Shared/WorldGeneration/TerrainMountains.h>
#include <Shared/WorldGeneration/TerrainRelief.h>
#include <Shared/WorldGeneration/TerrainRivers.h>
#include <Shared/WorldGeneration/TerrainRules.h>
#include <Shared/WorldGeneration/TerrainTypes.h>

#include <DiaMaths/Core/RandomStream.h>

#include <cstdint>
#include <vector>

namespace CoW {
namespace WorldGeneration {

enum class LayerType : uint8_t { Float, UInt };

struct LayerInfo
{
    const char*  name;
    LayerType    type;
    TerrainStage stage;
};

struct StageTiming
{
    TerrainStage stage         = TerrainStage::SeedFields;
    float        milliseconds  = 0.0f;
    bool         ranLastUpdate = false;
};

// X(name, LayerType, producing TerrainStage).
#define TERRAIN_LAYERS_TABLE(X) \
    X("landPotential",   LayerType::Float, TerrainStage::Landmass) \
    X("islandPotential", LayerType::Float, TerrainStage::Landmass) \
    X("landmass",        LayerType::UInt,  TerrainStage::Landmass) \
    X("coastDistance",   LayerType::UInt,  TerrainStage::Landmass) \
    X("volcanic",        LayerType::UInt,  TerrainStage::Landmass) \
    X("ridgeMask",       LayerType::Float, TerrainStage::Mountains) \
    X("passMask",        LayerType::UInt,  TerrainStage::Mountains) \
    X("barrierMask",     LayerType::Float, TerrainStage::Barrier) \
    X("elevationRaw",    LayerType::Float, TerrainStage::Elevation) \
    X("oceanDepth",      LayerType::Float, TerrainStage::Elevation) \
    X("filledElevation", LayerType::Float, TerrainStage::Drainage) \
    X("basinDepth",      LayerType::Float, TerrainStage::Drainage) \
    X("basinRank",       LayerType::UInt,  TerrainStage::Drainage) \
    X("flowDirection",   LayerType::UInt,  TerrainStage::Drainage) \
    X("flowAccumulation", LayerType::Float, TerrainStage::Drainage) \
    X("riverMask",       LayerType::UInt,  TerrainStage::Rivers) \
    X("lakeMask",        LayerType::UInt,  TerrainStage::Lakes) \
    X("zoneCostBase",    LayerType::Float, TerrainStage::DepthZones) \
    X("zoneCost",        LayerType::Float, TerrainStage::DepthZones) \
    X("depthZone",       LayerType::UInt,  TerrainStage::DepthZones)

// Number of stages the generator currently implements (stages >= this are Feature 8).
constexpr int kImplementedStageCount = 12;

constexpr uint32_t StageBit(TerrainStage s) { return 1u << static_cast<uint32_t>(s); }

// kStageReads[t] has bit s set when stage t reads the output of upstream stage s.
// Stage 0 has no reads (reseeds from the world seed); stage 11 reads the grid only;
// stages 12-14 belong to Feature 8 and are never scheduled yet.
inline constexpr uint32_t kStageReads[static_cast<int>(TerrainStage::Count)] =
{
    0,                                                                                  // 0 SeedFields
    StageBit(TerrainStage::SeedFields),                                                 // 1 Landmass {0}
    StageBit(TerrainStage::SeedFields) | StageBit(TerrainStage::Landmass),              // 2 Mountains {0,1}
    StageBit(TerrainStage::SeedFields) | StageBit(TerrainStage::Landmass),              // 3 Barrier {0,1}
    0x0F,                                                                               // 4 Elevation {0,1,2,3}
    StageBit(TerrainStage::Elevation),                                                  // 5 Drainage {4}
    StageBit(TerrainStage::Elevation) | StageBit(TerrainStage::Drainage),               // 6 Rivers {4,5}
    StageBit(TerrainStage::Drainage),                                                   // 7 Lakes {5}
    (1u << 1) | (1u << 2) | (1u << 3) | (1u << 6),                                      // 8 DepthZones {1,2,3,6}
    (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7),                                      // 9 Valleys {4,5,6,7}
    (1u << 1) | (1u << 2) | (1u << 3) | (1u << 6) | (1u << 7) | (1u << 8) | (1u << 9),  // 10 Classify
    0,                                                                                  // 11 Latitude (grid only)
    0, 0, 0                                                                             // 12-14 Feature 8
};

// ---------------------------------------------------------------------------
// Layer registry and storage
// ---------------------------------------------------------------------------
class TerrainGenerator;

class TerrainLayers
{
public:
    TerrainLayers();

    static int             GetLayerCount();
    static const LayerInfo& GetLayerInfo(int index);

    // -1 if unknown.
    int FindLayer(const char* name) const;

    // nullptr if the index is invalid, the layer has a different type, or it is empty.
    const float*    GetFloat(int index) const;
    const uint32_t* GetUInt(int index) const;

private:
    friend class TerrainGenerator;
    void      Resize(int cellCount);
    float*    FloatLayer(int index);
    uint32_t* UIntLayer(int index);

    std::vector<std::vector<float>>    mFloat;   // indexed by layer; unused for UInt layers
    std::vector<std::vector<uint32_t>> mUInt;    // indexed by layer; unused for Float layers
};

// ---------------------------------------------------------------------------
// Stage 0 cached draws
// ---------------------------------------------------------------------------
enum class NoiseChannel : uint8_t { Warp, Coast, Ridged, Rugged, BarrierWobble, Vent, Count };

constexpr int kContinentCandidates = 3;
constexpr int kPeninsulaCandidates = 4;
constexpr int kIslandCandidates    = 24;
constexpr int kMaxIslandBlobs      = 4;
constexpr int kPassCandidates      = 8;

struct ContinentCandidate
{
    float x = 0.0f, y = 0.0f;   // map space (1 tall, mapWidth wide)
    float axisRatio = 1.0f;     // [0.65, 1]
    float angle = 0.0f;         // radians [0, 2pi)
    bool  exhausted = false;    // rejection budget ran out; the furthest try was used
    bool operator==(const ContinentCandidate&) const = default;
};

struct IslandCandidate
{
    float x = 0.0f, y = 0.0f;                // map space
    int   blobCount = 1;                     // 1..4
    float blobDx[kMaxIslandBlobs] = {};      // cells, |offset| <= 2.5
    float blobDy[kMaxIslandBlobs] = {};
    float blobRadius[kMaxIslandBlobs] = {};  // cells, 1.5..3.5
    bool operator==(const IslandCandidate&) const = default;
};

struct SeedFields
{
    uint32_t           foldedSeed = 0;
    uint64_t           drawCount = 0;     // stream draw count after stage 0
    float              mapWidth = 1.0f;   // aspect A
    uint32_t           noiseSeeds[static_cast<int>(NoiseChannel::Count)] = {};
    ContinentCandidate continents[kContinentCandidates];
    float              peninsulaAngles[kPeninsulaCandidates] = {};   // radians around continent 0
    IslandCandidate    islands[kIslandCandidates];
    float              spineOffset = 0.0f;                           // fraction of inradius
    float              passT[kPassCandidates] = {};                  // [0.12, 0.88]
    bool operator==(const SeedFields&) const = default;
};

// 64 -> 32 bit seed fold: splitmix64 finaliser, then xor of the high and low halves.
uint32_t FoldSeed(uint64_t seed);

// ---------------------------------------------------------------------------
// Generator
// ---------------------------------------------------------------------------
class TerrainGenerator
{
public:
    explicit TerrainGenerator(const TerrainRules& rules);

    // Re-runs only the stages made dirty by changes since the last Update.
    void Update(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims,
                Dia::Maths::RandomStream& wldStream);
    void Invalidate();

    const TerrainLayers& GetLayers() const { return mLayers; }
    const StageTiming*   GetTimings() const { return mTimings; }   // TerrainStage::Count entries
    float                GetLastTotalMs() const { return mLastTotalMs; }
    const SeedFields&    GetSeedFields() const { return mSeedFields; }
    // Stage 1 internals (budgets, M ranking, masks, island groups) and effective slider values.
    const Stage1Debug&   GetStage1Debug() const { return mStage1; }
    const Effective&     GetEffective() const { return mEffective; }
    // Stage 2 internals (centre, curve, accepted passes, raw ridge).
    const Stage2Debug&   GetStage2Debug() const { return mStage2; }
    // Stage 3 internals (b*, band, chain vents).
    const Stage3Debug&   GetStage3Debug() const { return mStage3; }
    // Stage 5 internals (every basin, eligible ranking, epsilon-free fill level).
    const Stage5Debug&   GetStage5Debug() const { return mStage5; }
    // Stage 6 internals (threshold, river trees) and stage 7 lakes (basin, cells, surface, outflow).
    const Stage6Debug&   GetStage6Debug() const { return mStage6; }
    const Stage7Debug&   GetStage7Debug() const { return mStage7; }
    // Stage 8 internals (t1, t2, hasAncient).
    const Stage8Debug&   GetStage8Debug() const { return mStage8; }

private:
    void RunStage(TerrainStage stage, Dia::Maths::RandomStream& wldStream);
    void RunSeedFields(Dia::Maths::RandomStream& wldStream);

    TerrainRules     mRules;
    TerrainLayers    mLayers;
    SeedFields       mSeedFields;
    Stage1Debug      mStage1;
    Stage2Debug      mStage2;
    Stage3Debug      mStage3;
    Stage5Debug      mStage5;
    Stage6Debug      mStage6;
    Stage7Debug      mStage7;
    Stage8Debug      mStage8;
    Effective        mEffective;
    StageTiming      mTimings[static_cast<int>(TerrainStage::Count)];
    float            mLastTotalMs = 0.0f;

    bool             mHasCache = false;
    uint64_t         mSeed = 0;
    WorldShapeParams mShape;
    GridDimensions   mDims;
};

} // namespace WorldGeneration
} // namespace CoW
