// S-1 Terrain Generator test support: scripted slider sequences, layer/Cell hashing and a
// "fresh" regeneration comparator for determinism (AC1) and slider isolation (AC14) tests.
// Header-only; uses a local PRNG so scripts never depend on the Dia RandomStream.
#pragma once

#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// The 18 terrain sliders, in ShapeParam order (LandFraction .. LatitudeSouth).
constexpr int kTerrainSliderCount = static_cast<int>(ShapeParam::LatitudeSouth) + 1;

inline ShapeParam SliderParam(int slider) { return static_cast<ShapeParam>(slider); }

// Fixed per-slider value lists spanning the range, both ends included.
inline int SliderValueCount(int slider)
{
    switch (SliderParam(slider))
    {
    case ShapeParam::LandFraction:     return 5;
    case ShapeParam::Continents:       return 3;
    case ShapeParam::Islands:          return 4;
    case ShapeParam::ContinentIslands: return 4;
    case ShapeParam::Peninsulas:       return 4;
    case ShapeParam::Coastline:        return 4;
    case ShapeParam::Mountainousness:  return 4;
    case ShapeParam::SpineToRing:      return 4;
    case ShapeParam::Orientation:      return 6;
    case ShapeParam::Passes:           return 5;
    case ShapeParam::Barrier:          return 4;
    case ShapeParam::BarrierForm:      return 2;
    case ShapeParam::Ruggedness:       return 4;
    case ShapeParam::Rivers:           return 3;
    case ShapeParam::ValleyDepth:      return 3;
    case ShapeParam::Lakes:            return 4;
    case ShapeParam::LatitudeNorth:    return 4;
    case ShapeParam::LatitudeSouth:    return 4;
    default:                           break;
    }
    return 1;
}

// Applies value index v (modulo the list size) of the slider to the params, then clamps.
inline void SetSliderValue(WorldShapeParams& p, int slider, int v)
{
    static const float kLand[]    = { 0.20f, 0.30f, 0.42f, 0.55f, 0.65f };
    static const float kUnit4[]   = { 0.0f, 0.35f, 0.7f, 1.0f };
    static const float kCoast[]   = { 0.0f, 0.25f, 0.5f, 1.0f };
    static const float kMount[]   = { 0.0f, 0.3f, 0.55f, 1.0f };
    static const float kSpine[]   = { 0.0f, 0.15f, 0.5f, 1.0f };
    static const float kOrient[]  = { 0.0f, 30.0f, 90.0f, 180.0f, 270.0f, 359.0f };
    static const float kBarrier[] = { 0.0f, 0.3f, 0.6f, 1.0f };
    static const float kRugged[]  = { 0.0f, 0.4f, 0.8f, 1.0f };
    static const float kRivers[]  = { 0.0f, 0.5f, 1.0f };
    static const float kValley[]  = { 0.0f, 0.4f, 1.0f };
    static const uint8_t kLakes[] = { 0, 4, 8, 12 };
    static const float kLatN[]    = { -90.0f, 0.0f, 60.0f, 90.0f };
    static const float kLatS[]    = { -90.0f, 20.0f, 45.0f, 90.0f };

    const int n = SliderValueCount(slider);
    const int i = ((v % n) + n) % n;
    switch (SliderParam(slider))
    {
    case ShapeParam::LandFraction:     p.landFraction = kLand[i]; break;
    case ShapeParam::Continents:       p.continents = static_cast<uint8_t>(1 + i); break;
    case ShapeParam::Islands:          p.islands = static_cast<uint8_t>(i * 2); break;   // 0,2,4,6
    case ShapeParam::ContinentIslands: p.continentIslands = kUnit4[i]; break;
    case ShapeParam::Peninsulas:       p.peninsulas = static_cast<uint8_t>(i); break;
    case ShapeParam::Coastline:        p.coastline = kCoast[i]; break;
    case ShapeParam::Mountainousness:  p.mountainousness = kMount[i]; break;
    case ShapeParam::SpineToRing:      p.spineToRing = kSpine[i]; break;
    case ShapeParam::Orientation:      p.orientationDeg = kOrient[i]; break;
    case ShapeParam::Passes:           p.passes = static_cast<uint8_t>(i); break;
    case ShapeParam::Barrier:          p.barrier = kBarrier[i]; break;
    case ShapeParam::BarrierForm:      p.barrierForm = i == 0 ? BarrierForm::Plateau : BarrierForm::VolcanicChain; break;
    case ShapeParam::Ruggedness:       p.ruggedness = kRugged[i]; break;
    case ShapeParam::Rivers:           p.rivers = kRivers[i]; break;
    case ShapeParam::ValleyDepth:      p.valleyDepth = kValley[i]; break;
    case ShapeParam::Lakes:            p.lakes = kLakes[i]; break;
    case ShapeParam::LatitudeNorth:    p.latitudeNorthDeg = kLatN[i]; break;
    case ShapeParam::LatitudeSouth:    p.latitudeSouthDeg = kLatS[i]; break;
    default: break;
    }
    p.Clamp();
}

struct SliderChange
{
    int slider = 0;
    int value  = 0;   // value index, see SetSliderValue
};

// Tiny fixed PRNG (splitmix64) so scripts are identical across builds and platforms.
class ScriptRng
{
public:
    explicit ScriptRng(uint64_t seed) : mState(seed) {}
    uint32_t Next(uint32_t bound)
    {
        mState += 0x9E3779B97F4A7C15ull;
        uint64_t z = mState;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        return static_cast<uint32_t>(z >> 32) % bound;
    }
private:
    uint64_t mState;
};

// Deterministic script of `count` changes: consecutive blocks of 18 are a shuffled permutation of
// every slider (so every slider is hit), each with a random value index.
inline std::vector<SliderChange> BuildSliderScript(uint64_t prngSeed, int count)
{
    ScriptRng rng(prngSeed);
    std::vector<SliderChange> script;
    while (static_cast<int>(script.size()) < count)
    {
        int order[kTerrainSliderCount];
        for (int i = 0; i < kTerrainSliderCount; ++i) { order[i] = i; }
        for (int i = kTerrainSliderCount - 1; i > 0; --i)
        {
            const int j = static_cast<int>(rng.Next(static_cast<uint32_t>(i + 1)));
            const int t = order[i]; order[i] = order[j]; order[j] = t;
        }
        for (int i = 0; i < kTerrainSliderCount && static_cast<int>(script.size()) < count; ++i)
        {
            SliderChange c;
            c.slider = order[i];
            c.value  = static_cast<int>(rng.Next(static_cast<uint32_t>(SliderValueCount(order[i]))));
            script.push_back(c);
        }
    }
    return script;
}

// FNV-1a 64 over raw bytes.
inline uint64_t Fnv1a64(const void* data, size_t bytes, uint64_t h = 0xCBF29CE484222325ull)
{
    const uint8_t* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < bytes; ++i)
    {
        h ^= p[i];
        h *= 0x100000001B3ull;
    }
    return h;
}

// Hash of layer `index` over cellCount cells (0 if the layer is empty or invalid).
inline uint64_t HashLayer(const TerrainLayers& layers, int index, int cellCount)
{
    const size_t bytes = static_cast<size_t>(cellCount) * 4u;
    if (const float* f = layers.GetFloat(index)) { return Fnv1a64(f, bytes); }
    if (const uint32_t* u = layers.GetUInt(index)) { return Fnv1a64(u, bytes); }
    return 0;
}

inline uint64_t HashCells(const WorldStructure& w)
{
    return Fnv1a64(w.Cells(), static_cast<size_t>(w.CellCount()) * sizeof(Cell));
}

struct TerrainSnapshot
{
    std::vector<uint64_t> layerHash;   // indexed by layer
    uint64_t              cellHash = 0;
};

inline TerrainSnapshot TakeSnapshot(const TerrainGenerator& g, GridDimensions dims, const WorldStructure& w)
{
    TerrainSnapshot s;
    const int cells = dims.width * dims.height;
    for (int i = 0; i < TerrainLayers::GetLayerCount(); ++i)
    {
        s.layerHash.push_back(HashLayer(g.GetLayers(), i, cells));
    }
    s.cellHash = HashCells(w);
    return s;
}

// Names of layers whose hashes differ (cell array reported as "<cells>").
inline std::vector<std::string> DiffSnapshots(const TerrainSnapshot& a, const TerrainSnapshot& b)
{
    std::vector<std::string> d;
    for (size_t i = 0; i < a.layerHash.size() && i < b.layerHash.size(); ++i)
    {
        if (a.layerHash[i] != b.layerHash[i]) { d.push_back(TerrainLayers::GetLayerInfo(static_cast<int>(i)).name); }
    }
    if (a.cellHash != b.cellHash) { d.push_back("<cells>"); }
    return d;
}

// Owns a generator, its WLD stream and world; Run() = Update + WriteTerrain + snapshot.
class TerrainRunner
{
public:
    TerrainRunner() : mRules(TerrainRules::Defaults()), mGen(mRules), mRng(1) {}

    TerrainSnapshot Run(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims)
    {
        mGen.Update(seed, shape, dims, mRng);
        mGen.WriteTerrain(mWorld);
        return TakeSnapshot(mGen, dims, mWorld);
    }
    // The "fresh" path: drop all caches, then regenerate with identical arguments.
    TerrainSnapshot RunFresh(uint64_t seed, const WorldShapeParams& shape, GridDimensions dims)
    {
        mGen.Invalidate();
        return Run(seed, shape, dims);
    }

    TerrainGenerator&       Gen()         { return mGen; }
    const TerrainGenerator& Gen() const   { return mGen; }
    const WorldStructure&   World() const { return mWorld; }

private:
    TerrainRules             mRules;
    TerrainGenerator         mGen;
    Dia::Maths::RandomStream mRng;
    WorldStructure           mWorld;
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
