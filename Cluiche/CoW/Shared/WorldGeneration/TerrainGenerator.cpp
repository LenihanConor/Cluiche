// S-1 Terrain Generator: stage scheduling, dirty propagation, layer registry and stage 0.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

namespace {

#define TERRAIN_LAYER_ROW(name, type, stage) { name, type, stage },
#define TERRAIN_LAYER_COUNT(name, type, stage) + 1

// Trailing sentinel row keeps the array non-empty while the table has no rows.
const LayerInfo kLayerInfos[] =
{
    TERRAIN_LAYERS_TABLE(TERRAIN_LAYER_ROW)
    { nullptr, LayerType::Float, TerrainStage::SeedFields }
};
constexpr int kLayerCount = 0 TERRAIN_LAYERS_TABLE(TERRAIN_LAYER_COUNT);

constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

constexpr int   kContinentTries = 32;     // fixed rejection-sampling draw budget per candidate
constexpr float kMinSeparation  = 0.45f;
constexpr float kEdgeMargin     = 0.2f;
constexpr float kCentreRadius   = 0.12f;

float Dist(float ax, float ay, float bx, float by)
{
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

bool ParamChanged(ShapeParam p, const WorldShapeParams& a, const WorldShapeParams& b)
{
    switch (p)
    {
    case ShapeParam::LandFraction:       return a.landFraction != b.landFraction;
    case ShapeParam::Continents:         return a.continents != b.continents;
    case ShapeParam::Islands:            return a.islands != b.islands;
    case ShapeParam::ContinentIslands:   return a.continentIslands != b.continentIslands;
    case ShapeParam::Peninsulas:         return a.peninsulas != b.peninsulas;
    case ShapeParam::Coastline:          return a.coastline != b.coastline;
    case ShapeParam::Mountainousness:    return a.mountainousness != b.mountainousness;
    case ShapeParam::SpineToRing:        return a.spineToRing != b.spineToRing;
    case ShapeParam::Orientation:        return a.orientationDeg != b.orientationDeg;
    case ShapeParam::Passes:             return a.passes != b.passes;
    case ShapeParam::Barrier:            return a.barrier != b.barrier;
    case ShapeParam::BarrierForm:        return a.barrierForm != b.barrierForm;
    case ShapeParam::Ruggedness:         return a.ruggedness != b.ruggedness;
    case ShapeParam::Rivers:             return a.rivers != b.rivers;
    case ShapeParam::ValleyDepth:        return a.valleyDepth != b.valleyDepth;
    case ShapeParam::Lakes:              return a.lakes != b.lakes;
    case ShapeParam::LatitudeNorth:      return a.latitudeNorthDeg != b.latitudeNorthDeg;
    case ShapeParam::LatitudeSouth:      return a.latitudeSouthDeg != b.latitudeSouthDeg;
    case ShapeParam::TemperatureOffset:  return a.temperatureOffset != b.temperatureOffset;
    case ShapeParam::MoistureOffset:     return a.moistureOffset != b.moistureOffset;
    case ShapeParam::Count:              break;
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// TerrainLayers
// ---------------------------------------------------------------------------
TerrainLayers::TerrainLayers()
    : mFloat(static_cast<size_t>(kLayerCount))
    , mUInt(static_cast<size_t>(kLayerCount))
{
}

int TerrainLayers::GetLayerCount() { return kLayerCount; }

const LayerInfo& TerrainLayers::GetLayerInfo(int index)
{
    if (index < 0 || index >= kLayerCount)
    {
        return kLayerInfos[kLayerCount];   // sentinel row (name == nullptr)
    }
    return kLayerInfos[index];
}

int TerrainLayers::FindLayer(const char* name) const
{
    if (name == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < kLayerCount; ++i)
    {
        if (std::strcmp(kLayerInfos[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

const float* TerrainLayers::GetFloat(int index) const
{
    if (index < 0 || index >= kLayerCount || kLayerInfos[index].type != LayerType::Float || mFloat[index].empty())
    {
        return nullptr;
    }
    return mFloat[index].data();
}

const uint32_t* TerrainLayers::GetUInt(int index) const
{
    if (index < 0 || index >= kLayerCount || kLayerInfos[index].type != LayerType::UInt || mUInt[index].empty())
    {
        return nullptr;
    }
    return mUInt[index].data();
}

void TerrainLayers::Resize(int cellCount)
{
    const size_t n = cellCount > 0 ? static_cast<size_t>(cellCount) : 0u;
    for (int i = 0; i < kLayerCount; ++i)
    {
        if (kLayerInfos[i].type == LayerType::Float)
        {
            mFloat[i].assign(n, 0.0f);
        }
        else
        {
            mUInt[i].assign(n, 0u);
        }
    }
}

float* TerrainLayers::FloatLayer(int index)
{
    return (index >= 0 && index < kLayerCount) ? mFloat[index].data() : nullptr;
}

uint32_t* TerrainLayers::UIntLayer(int index)
{
    return (index >= 0 && index < kLayerCount) ? mUInt[index].data() : nullptr;
}

// ---------------------------------------------------------------------------
// Seed fold
// ---------------------------------------------------------------------------
uint32_t FoldSeed(uint64_t seed)
{
    uint64_t z = seed + 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    z = z ^ (z >> 31);
    return static_cast<uint32_t>(z >> 32) ^ static_cast<uint32_t>(z & 0xFFFFFFFFull);
}

// ---------------------------------------------------------------------------
// TerrainGenerator
// ---------------------------------------------------------------------------
TerrainGenerator::TerrainGenerator(const TerrainRules& rules)
    : mRules(rules)
{
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        mTimings[s].stage = static_cast<TerrainStage>(s);
    }
}

void TerrainGenerator::Invalidate()
{
    mHasCache = false;
}

void TerrainGenerator::Update(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims,
                              Dia::Maths::RandomStream& wldStream)
{
    constexpr uint32_t kAllImplemented = (1u << kImplementedStageCount) - 1u;

    const bool dimsChanged = !mHasCache || dims.width != mDims.width || dims.height != mDims.height;

    uint32_t dirty = 0;
    if (!mHasCache || seed != mSeed || dimsChanged)
    {
        dirty = kAllImplemented;
    }
    else
    {
        for (int p = 0; p < static_cast<int>(ShapeParam::Count); ++p)
        {
            const ShapeParam param = static_cast<ShapeParam>(p);
            if (ParamChanged(param, shape, mShape))
            {
                const int first = static_cast<int>(FirstStageAffected(param));
                if (first < kImplementedStageCount)
                {
                    dirty |= 1u << first;
                }
            }
        }
    }

    // Downstream propagation: reads only reference upstream stages, so one ordered pass suffices.
    for (int s = 1; s < kImplementedStageCount; ++s)
    {
        if ((kStageReads[s] & dirty) != 0)
        {
            dirty |= 1u << s;
        }
    }

    if (dimsChanged)
    {
        mLayers.Resize((dims.width > 0 && dims.height > 0) ? dims.width * dims.height : 0);
    }

    mSeed = seed;
    mShape = shape;
    mDims = dims;
    mHasCache = true;

    float total = 0.0f;
    for (int s = 0; s < static_cast<int>(TerrainStage::Count); ++s)
    {
        StageTiming& t = mTimings[s];
        t.ranLastUpdate = false;
        t.milliseconds = 0.0f;
        if (s < kImplementedStageCount && (dirty & (1u << s)) != 0)
        {
            const auto start = std::chrono::steady_clock::now();
            RunStage(static_cast<TerrainStage>(s), wldStream);
            const auto end = std::chrono::steady_clock::now();
            t.milliseconds = std::chrono::duration<float, std::milli>(end - start).count();
            t.ranLastUpdate = true;
            total += t.milliseconds;
        }
    }
    mLastTotalMs = total;
}

void TerrainGenerator::RunStage(TerrainStage stage, Dia::Maths::RandomStream& wldStream)
{
    switch (stage)
    {
    case TerrainStage::SeedFields:  RunSeedFields(wldStream); break;
    case TerrainStage::Landmass:
    {
        Stage1Layers out;
        out.landPotential   = mLayers.FloatLayer(mLayers.FindLayer("landPotential"));
        out.islandPotential = mLayers.FloatLayer(mLayers.FindLayer("islandPotential"));
        out.landmass        = mLayers.UIntLayer(mLayers.FindLayer("landmass"));
        out.coastDistance   = mLayers.UIntLayer(mLayers.FindLayer("coastDistance"));
        out.volcanic        = mLayers.UIntLayer(mLayers.FindLayer("volcanic"));
        ComputeStage1(mRules, mShape, mDims, mSeedFields, out, mStage1, mEffective);
        break;
    }
    case TerrainStage::Mountains:   break;   // TODO: mountains (later task)
    case TerrainStage::Barrier:     break;   // TODO: barrier (later task)
    case TerrainStage::Elevation:   break;   // TODO: elevation (later task)
    case TerrainStage::Drainage:    break;   // TODO: drainage (later task)
    case TerrainStage::Rivers:      break;   // TODO: rivers (later task)
    case TerrainStage::Lakes:       break;   // TODO: lakes (later task)
    case TerrainStage::DepthZones:  break;   // TODO: depth zones (later task)
    case TerrainStage::Valleys:     break;   // TODO: valleys (later task)
    case TerrainStage::Classify:    break;   // TODO: classify (later task)
    case TerrainStage::Latitude:    break;   // TODO: latitude (later task)
    default:                        break;   // Feature 8 stages never run
    }
}

// Stage 0. Draws use RandomUnit/RandomInt only and every loop draws its full budget regardless
// of acceptance, so the draw count depends on neither sliders nor grid dimensions.
//   Peninsula angles: a base angle plus 4 weights; gap_j = 60deg + 120deg * w_j / sum(w), so the
//   4 gaps sum to 360deg and every gap (hence every pair) is >= 60deg by construction.
//   Continents 1/2: kContinentTries (x,y) pairs are always drawn; the first satisfying the
//   separation and edge constraints is used, else the try with the largest margin (exhausted).
void TerrainGenerator::RunSeedFields(Dia::Maths::RandomStream& rng)
{
    SeedFields f{};
    f.foldedSeed = FoldSeed(mSeed);
    rng.Restore(f.foldedSeed, 0);

    // Map space: centres divided by the maximum centre Y, so the map is 1 tall and A wide.
    const int w = std::max(mDims.width, 1);
    const int h = std::max(mDims.height, 1);
    const Dia::Pathfinding::CellCoord corner{ w - 1, h - 1 };
    const Dia::Pathfinding::CellCoord maxYCell{ w > 1 ? 1 : 0, h - 1 };
    float maxY = Dia::Pathfinding::HexTopology::CellCenter(maxYCell, 1.0f).y;
    if (maxY < 1e-6f)
    {
        maxY = 1.0f;
    }
    f.mapWidth = Dia::Pathfinding::HexTopology::CellCenter(corner, 1.0f).x / maxY;
    if (f.mapWidth < 1e-6f)
    {
        f.mapWidth = 1.0f;
    }
    const float A = f.mapWidth;

    // 1. Noise seeds.
    for (int i = 0; i < static_cast<int>(NoiseChannel::Count); ++i)
    {
        f.noiseSeeds[i] = static_cast<uint32_t>(rng.RandomInt());
    }

    // 2. Continent candidates.
    {
        ContinentCandidate& c0 = f.continents[0];
        const float r = kCentreRadius * std::sqrt(rng.RandomUnit());
        const float a = rng.RandomUnit() * kTwoPi;
        c0.x = A * 0.5f + r * std::cos(a);
        c0.y = 0.5f + r * std::sin(a);
        c0.axisRatio = 0.65f + 0.35f * rng.RandomUnit();
        c0.angle = rng.RandomUnit() * kTwoPi;

        for (int k = 1; k < kContinentCandidates; ++k)
        {
            ContinentCandidate& c = f.continents[k];
            float bestX = 0.0f, bestY = 0.0f, bestMargin = -1e30f;
            float accX = 0.0f, accY = 0.0f;
            bool accepted = false;
            for (int t = 0; t < kContinentTries; ++t)
            {
                const float x = rng.RandomUnit() * A;
                const float y = rng.RandomUnit();
                // Margin = smallest slack over all constraints (>= 0 means accepted).
                float margin = std::min(std::min(x, A - x), std::min(y, 1.0f - y)) - kEdgeMargin;
                for (int j = 0; j < k; ++j)
                {
                    margin = std::min(margin, Dist(x, y, f.continents[j].x, f.continents[j].y) - kMinSeparation);
                }
                if (margin >= 0.0f && !accepted)
                {
                    accepted = true;
                    accX = x;
                    accY = y;
                }
                if (margin > bestMargin)
                {
                    bestMargin = margin;
                    bestX = x;
                    bestY = y;
                }
            }
            c.x = accepted ? accX : bestX;
            c.y = accepted ? accY : bestY;
            c.exhausted = !accepted;
            c.axisRatio = 0.65f + 0.35f * rng.RandomUnit();
            c.angle = rng.RandomUnit() * kTwoPi;
        }
    }

    // 3. Peninsula candidates.
    {
        const float base = rng.RandomUnit() * kTwoPi;
        float wgt[kPeninsulaCandidates];
        float sum = 0.0f;
        for (int j = 0; j < kPeninsulaCandidates; ++j)
        {
            wgt[j] = rng.RandomUnit();
            sum += wgt[j];
        }
        const float sixty = kPi / 3.0f;
        float angle = base;
        for (int j = 0; j < kPeninsulaCandidates; ++j)
        {
            f.peninsulaAngles[j] = std::fmod(angle, kTwoPi);
            const float share = sum > 0.0f ? wgt[j] / sum : 1.0f / kPeninsulaCandidates;
            angle += sixty + 2.0f * sixty * share;
        }
    }

    // 4. Island candidates (blob offsets and radii in cells).
    for (int i = 0; i < kIslandCandidates; ++i)
    {
        IslandCandidate& isl = f.islands[i];
        isl.x = rng.RandomUnit() * A;
        isl.y = rng.RandomUnit();
        isl.blobCount = 1 + std::min(3, static_cast<int>(rng.RandomUnit() * 4.0f));
        for (int b = 0; b < kMaxIslandBlobs; ++b)
        {
            const float r = 2.5f * std::sqrt(rng.RandomUnit());
            const float a = rng.RandomUnit() * kTwoPi;
            isl.blobDx[b] = r * std::cos(a);
            isl.blobDy[b] = r * std::sin(a);
            isl.blobRadius[b] = 1.5f + 2.0f * rng.RandomUnit();
        }
    }

    // 5. Spine lateral offset.
    f.spineOffset = -0.15f + 0.30f * rng.RandomUnit();

    // 6. Pass candidates.
    for (int i = 0; i < kPassCandidates; ++i)
    {
        f.passT[i] = 0.12f + 0.76f * rng.RandomUnit();
    }

    f.drawCount = rng.GetDrawCount();
    mSeedFields = f;
}

} // namespace WorldGeneration
} // namespace CoW
