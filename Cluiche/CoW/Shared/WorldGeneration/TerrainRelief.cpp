// S-1 Terrain Generator: Stage 3 (Barrier) and Stage 4 (Elevation). See TerrainRelief.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainRelief.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaMaths/Noise/Noise.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>

namespace CoW {
namespace WorldGeneration {

namespace {

using Dia::Pathfinding::CellCoord;
using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::kNoCell;

struct MapSpace
{
    float invMaxY = 1.0f;

    explicit MapSpace(GridDimensions dims)
    {
        const int w = std::max(dims.width, 1);
        const int h = std::max(dims.height, 1);
        float maxY = HexTopology::CellCenter(CellCoord{ w > 1 ? 1 : 0, h - 1 }, 1.0f).y;
        if (maxY < 1e-6f)
        {
            maxY = 1.0f;
        }
        invMaxY = 1.0f / maxY;
    }

    void Point(int x, int y, float& px, float& py) const
    {
        const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ x, y }, 1.0f);
        px = c.x * invMaxY;
        py = c.y * invMaxY;
    }
};

Dia::Maths::FractalParams MakeFractal(int octaves, float frequency)
{
    Dia::Maths::FractalParams p;
    p.octaves = octaves;
    p.frequency = frequency;
    return p;
}

} // namespace

// ---------------------------------------------------------------------------
// Stage 3
// ---------------------------------------------------------------------------
void ComputeStage3(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const uint32_t* landmass, const uint32_t* coastDistance,
                   int mainLandmass, Stage3Layers& layers, Stage3Debug& out)
{
    using Dia::Maths::NoiseType;
    out = Stage3Debug{};
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || layers.barrierMask == nullptr)
    {
        return;
    }
    std::fill(layers.barrierMask, layers.barrierMask + n, 0.0f);
    if (layers.volcanic != nullptr)
    {
        for (int i = 0; i < n; ++i)
        {
            layers.volcanic[i] &= ~kVolcanicChainBit;
        }
    }
    out.band.assign(static_cast<size_t>(n), 0.0f);
    if (landmass == nullptr || coastDistance == nullptr || mainLandmass <= 0)
    {
        return;
    }

    // b*: nearest-rank (lower) quantile of the coast distance over the main landmass.
    std::vector<uint32_t> depths;
    for (int i = 0; i < n; ++i)
    {
        if (landmass[i] == static_cast<uint32_t>(mainLandmass))
        {
            depths.push_back(coastDistance[i]);
        }
    }
    out.mainCells = static_cast<int>(depths.size());
    if (depths.empty())
    {
        return;
    }
    std::sort(depths.begin(), depths.end());
    const float q = std::clamp(rules.kBarrierQuantile(), 0.0f, 1.0f);
    const size_t qi = std::min(depths.size() - 1, static_cast<size_t>(q * static_cast<float>(depths.size() - 1)));
    const float bStar = static_cast<float>(depths[qi]);
    out.bStar = bStar;

    if (shape.barrier <= 0.0f)
    {
        return;   // no barrier: empty mask, no chain vents
    }

    const MapSpace space(dims);
    const Dia::Maths::FractalParams wobbleParams = MakeFractal(3, kBarrierWobbleFrequency);
    const uint32_t wobbleSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::BarrierWobble)];
    const float wobble = rules.kBarrierWobble();
    const float halfWidth = rules.kBarrierHalfWidth();
    const bool chain = shape.barrierForm == BarrierForm::VolcanicChain;

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int i = y * w + x;
            if (landmass[i] != static_cast<uint32_t>(mainLandmass))
            {
                continue;
            }
            float px, py;
            space.Point(x, y, px, py);
            const float noise = Dia::Maths::FractalNoise2D<NoiseType::Gradient>(wobbleSeed, px, py, wobbleParams);   // [-1,1] = 2*fBm01-1
            const float d = static_cast<float>(coastDistance[i]);
            const float band = std::max(0.0f, 1.0f - std::fabs(d + wobble * noise - bStar) / halfWidth);
            out.band[i] = band;
            if (chain)
            {
                layers.barrierMask[i] = 0.5f * band;
            }
            else
            {
                layers.barrierMask[i] = std::max(band, d > bStar ? rules.kPlateauInner() : 0.0f);
            }
        }
    }

    if (!chain || layers.volcanic == nullptr)
    {
        return;
    }

    // Chain vents: band cells that are local maxima of the vent noise (over the six hex
    // neighbours; ties go to the lower index), accepted greedily from the highest noise down,
    // at least kVentSpacing hex steps apart.
    const Dia::Maths::FractalParams ventParams = MakeFractal(2, kVentFrequency);
    const uint32_t ventSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::Vent)];
    std::vector<float> ventNoise(static_cast<size_t>(n), 0.0f);
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            float px, py;
            space.Point(x, y, px, py);
            ventNoise[y * w + x] = Dia::Maths::FractalNoise2D<NoiseType::Gradient>(ventSeed, px, py, ventParams);
        }
    }

    const HexTopology topo(w, h);
    std::vector<int> candidates;
    for (int i = 0; i < n; ++i)
    {
        if (out.band[i] <= 0.0f)
        {
            continue;
        }
        bool isMax = true;
        for (uint32_t k = 0; k < 6 && isMax; ++k)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), k);
            if (nb == kNoCell)
            {
                continue;
            }
            const int j = static_cast<int>(nb);
            if (ventNoise[j] > ventNoise[i] || (ventNoise[j] == ventNoise[i] && j < i))
            {
                isMax = false;
            }
        }
        if (isMax)
        {
            candidates.push_back(i);
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&](int a, int b) { return ventNoise[a] > ventNoise[b]; });

    const uint32_t spacing = static_cast<uint32_t>(rules.GetInt(Rule::kVentSpacing));
    for (int c : candidates)
    {
        bool ok = true;
        for (int v : out.chainVents)
        {
            if (HexTopology::Distance(CellCoord{ c % w, c / w }, CellCoord{ v % w, v / w }) < spacing)
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            out.chainVents.push_back(c);
            layers.volcanic[c] |= kVolcanicChainBit;
        }
    }
}

// ---------------------------------------------------------------------------
// Stage 4
// ---------------------------------------------------------------------------
void ComputeStage4(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const uint32_t* landmass, const uint32_t* coastDistance,
                   const float* ridgeMask, const float* barrierMask, const uint32_t* volcanic,
                   Stage4Layers& layers)
{
    using Dia::Maths::NoiseType;
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;
    if (n == 0 || layers.elevationRaw == nullptr || layers.oceanDepth == nullptr)
    {
        return;
    }
    std::fill(layers.elevationRaw, layers.elevationRaw + n, 0.0f);
    std::fill(layers.oceanDepth, layers.oceanDepth + n, 0.0f);
    if (landmass == nullptr || coastDistance == nullptr)
    {
        return;
    }

    std::vector<CellCoord> vents;
    if (volcanic != nullptr)
    {
        for (int i = 0; i < n; ++i)
        {
            if (volcanic[i] != 0u)
            {
                vents.push_back(CellCoord{ i % w, i / w });
            }
        }
    }

    const MapSpace space(dims);
    Dia::Maths::RidgedParams ridgedParams;
    ridgedParams.fractal = MakeFractal(4, kRidgedFrequency);
    const Dia::Maths::FractalParams ruggedParams = MakeFractal(4, kRuggedFrequency);
    const uint32_t ridgedSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::Ridged)];
    const uint32_t ruggedSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::Rugged)];

    const float coastElev = rules.kCoastElev();
    const float lowlandRise = rules.kLowlandRise();
    const float inlandScale = rules.kInlandScale();
    const float mountainHeight = rules.kMountainHeight();
    const float barrierHeight = rules.kBarrierHeight();
    const float ventHeight = rules.kVentHeight();
    const float ventRadius = rules.kVentRadius();
    const float ruggedAmp = rules.kRuggedAmp();
    const float shelfScale = rules.kShelfScale();

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int i = y * w + x;
            const float d = static_cast<float>(coastDistance[i]);
            if (landmass[i] == 0u)
            {
                layers.oceanDepth[i] = std::min(1.0f, d / shelfScale);
                continue;
            }

            float px, py;
            space.Point(x, y, px, py);
            const float base = lowlandRise * (1.0f - std::exp(-(d - 1.0f) / inlandScale));

            const float ridge = ridgeMask != nullptr ? std::max(0.0f, ridgeMask[i]) : 0.0f;
            const float ridged = Dia::Maths::RidgedNoise2D<NoiseType::Gradient>(ridgedSeed, px, py, ridgedParams);   // [0,1]
            const float mount = shape.mountainousness * mountainHeight * ridge * std::sqrt(ridge) * (0.65f + 0.35f * ridged);

            const float barrier = shape.barrier * barrierHeight * (barrierMask != nullptr ? barrierMask[i] : 0.0f);

            float nearest = 1e9f;
            for (const CellCoord& v : vents)
            {
                nearest = std::min(nearest, static_cast<float>(HexTopology::Distance(CellCoord{ x, y }, v)));
            }
            const float ventTerm = (ventRadius > 0.0f && !vents.empty())
                ? ventHeight * std::max(0.0f, 1.0f - nearest / ventRadius) : 0.0f;

            const float fbm = Dia::Maths::FractalNoise2D<NoiseType::Gradient>(ruggedSeed, px, py, ruggedParams);   // [-1,1] = 2*fBm01-1
            const float rugged = shape.ruggedness * ruggedAmp * fbm * std::min(1.0f, (d - 1.0f) / 2.0f);

            layers.elevationRaw[i] = std::clamp(coastElev + base + mount + barrier + ventTerm + rugged, coastElev, 1.0f);
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
