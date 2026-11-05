// S-1 Terrain Generator: Stage 1 (Landmass) part A. See TerrainLandmass.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaMaths/Noise/Noise.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace CoW {
namespace WorldGeneration {

namespace {

constexpr float kBlobWeight[kContinentCandidates] = { 1.0f, 0.75f, 0.6f };
constexpr float kBlobRadius[kContinentCandidates] = { 0.55f, 0.36f, 0.30f };

// Peninsula lobe geometry, as fractions of continent 0's radius R0.
constexpr float kLobeOffset = 0.62f;
constexpr float kLobeLong   = 0.75f;
constexpr float kLobeShort  = 0.20f;

constexpr float kEdgeFadeReach = 6.0f;   // steps from the edge where the fade ends (spec: 2-6 steps)

constexpr float kWarpFrequency  = 2.0f;
constexpr float kCoastFrequency = 6.0f;

// Distance of (dx,dy) in an ellipse frame: long axis along 'angle' with half-length 1, short axis
// scaled by 'ratio' (ratio 1 = circle). Returns a unit-normalised distance (1 on the boundary).
float EllipseDist(float dx, float dy, float angle, float longAxis, float shortAxis)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float u = dx * c + dy * s;
    const float v = -dx * s + dy * c;
    const float a = u / longAxis;
    const float b = v / shortAxis;
    return std::sqrt(a * a + b * b);
}

int RoundToInt(float v) { return static_cast<int>(std::floor(v + 0.5f)); }

} // namespace

std::vector<uint8_t> Stage1Debug::TopMask(int n) const
{
    std::vector<uint8_t> mask(static_cast<size_t>(cellCount), 0);
    const int count = std::clamp(n, 0, static_cast<int>(rank.size()));
    for (int i = 0; i < count; ++i)
    {
        mask[static_cast<size_t>(rank[static_cast<size_t>(i)])] = 1;
    }
    return mask;
}

void ComputeStage1(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, float* landPotential, Stage1Debug& out)
{
    using Dia::Maths::NoiseType;
    const int w = std::max(dims.width, 0);
    const int h = std::max(dims.height, 0);
    const int n = w * h;

    out = Stage1Debug{};
    out.cellCount = n;
    if (n == 0 || landPotential == nullptr)
    {
        return;
    }

    const float negInf = -std::numeric_limits<float>::infinity();
    const float coastline = shape.coastline;

    // Map space: centres divided by the maximum centre Y (matches stage 0).
    const Dia::Pathfinding::CellCoord maxYCell{ w > 1 ? 1 : 0, h - 1 };
    float maxY = Dia::Pathfinding::HexTopology::CellCenter(maxYCell, 1.0f).y;
    if (maxY < 1e-6f)
    {
        maxY = 1.0f;
    }

    Dia::Maths::FractalParams warpParams;
    warpParams.octaves = 3;
    warpParams.frequency = kWarpFrequency;
    Dia::Maths::FractalParams coastParams;
    coastParams.octaves = 4;
    coastParams.frequency = kCoastFrequency;

    const float warpStrength = coastline * rules.kWarpAmp();
    const float coastAmp = coastline * rules.kCoastAmp();
    const uint32_t warpSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::Warp)];
    const uint32_t coastSeed = seeds.noiseSeeds[static_cast<int>(NoiseChannel::Coast)];

    const int continents = std::clamp(static_cast<int>(shape.continents), 1, kContinentCandidates);
    const int peninsulas = std::clamp(static_cast<int>(shape.peninsulas), 0, kPeninsulaCandidates);
    const ContinentCandidate& c0 = seeds.continents[0];
    const float R0 = kBlobRadius[0];

    const float borderWidth = rules.kBorderWidth();
    const float edgeFade = rules.kEdgeFade();

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const int i = y * w + x;

            // Steps from the nearest map edge (0 = outer ring), offset-grid approximation.
            const int edge = std::min(std::min(x, w - 1 - x), std::min(y, h - 1 - y));
            if (static_cast<float>(edge) < borderWidth)
            {
                landPotential[i] = negInf;
                continue;
            }

            const Dia::Maths::Vector2D centre =
                Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ x, y }, 1.0f);
            const float px = centre.x / maxY;
            const float py = centre.y / maxY;

            // Domain warp, scaled by Coastline.
            float qx = px, qy = py;
            if (warpStrength > 0.0f)
            {
                const Dia::Maths::Warp2DResult q =
                    Dia::Maths::Warp2D<NoiseType::Gradient>(warpSeed, px, py, warpStrength, warpParams);
                qx = q.x;
                qy = q.y;
            }

            // Step 1: continent blobs (ellipse frame from the stage-0 candidates).
            float m = negInf;
            for (int k = 0; k < continents; ++k)
            {
                const ContinentCandidate& c = seeds.continents[k];
                const float d = EllipseDist(qx - c.x, qy - c.y, c.angle, 1.0f, c.axisRatio);
                m = std::max(m, kBlobWeight[k] * (1.0f - d / kBlobRadius[k]));
            }

            // Step 2: peninsula lobes on continent 0.
            for (int j = 0; j < peninsulas; ++j)
            {
                const float phi = seeds.peninsulaAngles[j];
                const float lx = c0.x + kLobeOffset * R0 * std::cos(phi);
                const float ly = c0.y + kLobeOffset * R0 * std::sin(phi);
                const float d = EllipseDist(qx - lx, qy - ly, phi, kLobeLong * R0, kLobeShort * R0);
                m = std::max(m, kBlobWeight[0] * (1.0f - d));
            }

            // Step 3: coast noise (fBm in [-1,1], i.e. 2*fBm01 - 1) and edge fade.
            if (coastAmp > 0.0f)
            {
                m += coastAmp * Dia::Maths::FractalNoise2D<NoiseType::Gradient>(coastSeed, px, py, coastParams);
            }
            const float e = static_cast<float>(edge);
            if (e < kEdgeFadeReach)
            {
                const float t = std::clamp((e - borderWidth) / (kEdgeFadeReach - borderWidth), 0.0f, 1.0f);
                m -= edgeFade * (1.0f - t);
            }
            landPotential[i] = m;
        }
    }

    // Rank eligible cells: M descending, ties by lowest index.
    out.rank.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        if (std::isfinite(landPotential[i]))
        {
            out.rank.push_back(i);
        }
    }
    std::sort(out.rank.begin(), out.rank.end(), [landPotential](int a, int b)
    {
        return landPotential[a] != landPotential[b] ? landPotential[a] > landPotential[b] : a < b;
    });
    out.eligibleCount = static_cast<int>(out.rank.size());

    // Steps 4-5: land budget L (clamped to eligible cells) and island budget K.
    out.landTargetRequested = RoundToInt(shape.landFraction * static_cast<float>(n));
    out.landTarget = std::min(out.landTargetRequested, out.eligibleCount);
    out.landClamped = out.landTargetRequested > out.eligibleCount;
    if (shape.islands == 0)
    {
        out.islandBudget = 0;
    }
    else
    {
        const float share = std::clamp(1.0f - shape.continentIslands, rules.kMinIslandShare(), rules.kMaxIslandShare());
        out.islandBudget = RoundToInt(static_cast<float>(out.landTarget) * share);
    }
}

} // namespace WorldGeneration
} // namespace CoW
