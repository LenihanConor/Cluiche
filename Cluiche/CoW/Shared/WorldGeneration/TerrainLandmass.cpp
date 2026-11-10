// S-1 Terrain Generator: Stage 1 (Landmass), steps 1-9. See TerrainLandmass.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaMaths/Noise/Noise.h>
#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

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
                   const SeedFields& seeds, Stage1Layers& layers, Stage1Debug& out, Effective& effective)
{
    float* landPotential = layers.landPotential;
    effective = Effective{};
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

    // ---- Part B ----
    effective.continents = continents;
    const Dia::Pathfinding::HexTopology topo(w, h);
    const float minEdge = borderWidth + 1.0f;
    auto edgeSteps = [w, h](int i)
    {
        const int x = i % w, y = i / w;
        return std::min(std::min(x, w - 1 - x), std::min(y, h - 1 - y));
    };
    const int minLandmass = rules.GetInt(Rule::kMinLandmass);
    const int peak = out.rank.empty() ? -1 : out.rank[0];

    // Step 5: reference mainland M_ref = top L cells, cleaned up. Independent of ContinentIslands.
    out.refMainland = out.TopMask(out.landTarget);
    CleanUpLand(out.refMainland, dims, minLandmass, out.landTarget > 0 ? peak : -1);

    // Hex distance from M_ref (multi-source BFS); -1 = unreachable / no M_ref.
    std::vector<int> distRef(static_cast<size_t>(n), -1);
    {
        std::vector<int> queue;
        queue.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
        {
            if (out.refMainland[static_cast<size_t>(i)]) { distRef[static_cast<size_t>(i)] = 0; queue.push_back(i); }
        }
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int c = queue[head];
            for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != Dia::Pathfinding::kNoCell && distRef[nb] < 0)
                {
                    distRef[nb] = distRef[static_cast<size_t>(c)] + 1;
                    queue.push_back(static_cast<int>(nb));
                }
            }
        }
    }
    auto nextToRef = [&](int c)
    {
        for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb != Dia::Pathfinding::kNoCell && out.refMainland[nb]) { return true; }
        }
        return false;
    };

    // Step 6: walk the island candidates in rank (draw) order.
    const int wantedIslands = std::clamp(static_cast<int>(shape.islands), 0, kIslandCandidates);
    std::vector<int> candidateOf;   // accepted group -> candidate index
    for (int k = 0; k < kIslandCandidates && static_cast<int>(effective.islandCentres.size()) < wantedIslands; ++k)
    {
        const IslandCandidate& cand = seeds.islands[k];
        const Dia::Pathfinding::CellCoord cc = Dia::Pathfinding::HexTopology::CellFromPoint(
            Dia::Maths::Vector2D{ cand.x * maxY, cand.y * maxY }, 1.0f);
        const int cx = std::clamp(cc.x, 0, w - 1);
        const int cy = std::clamp(cc.y, 0, h - 1);
        const int cell = cy * w + cx;
        const int dRef = distRef[static_cast<size_t>(cell)];
        if (out.refMainland[static_cast<size_t>(cell)] || dRef < 0) { continue; }
        if (static_cast<float>(dRef) < rules.kIslandGapMin() || static_cast<float>(dRef) > rules.kIslandGapMax()) { continue; }
        if (static_cast<float>(edgeSteps(cell)) < minEdge) { continue; }
        bool spaced = true;
        for (int other : effective.islandCentres)
        {
            if (static_cast<float>(Dia::Pathfinding::HexTopology::Distance(
                    topo.ToCoord(static_cast<uint32_t>(cell)), topo.ToCoord(static_cast<uint32_t>(other)))) < rules.kIslandSpacing())
            {
                spaced = false;
                break;
            }
        }
        if (!spaced) { continue; }
        effective.islandCentres.push_back(cell);
        candidateOf.push_back(k);
    }
    const int groups = static_cast<int>(effective.islandCentres.size());
    effective.islands = groups;

    // Island potential I(c) = max over accepted blobs of (1 - dist/r). Blob distance is Euclidean
    // between cell centres in cell units; the group's centre cell counts as I = 1 (its anchor),
    // so every group gets its centre first even when the blob offsets do not land on it.
    constexpr float kCellPitch = 1.7320508f;   // centre-to-centre distance of adjacent cells at size 1
    std::vector<float> islandPot(static_cast<size_t>(n), 0.0f);
    std::vector<int>   owner(static_cast<size_t>(n), -1);
    for (int g = 0; g < groups; ++g)
    {
        const IslandCandidate& cand = seeds.islands[candidateOf[static_cast<size_t>(g)]];
        for (int i = 0; i < n; ++i)
        {
            const Dia::Maths::Vector2D c =
                Dia::Pathfinding::HexTopology::CellCenter(Dia::Pathfinding::CellCoord{ i % w, i / w }, 1.0f);
            float best = 0.0f;
            for (int b = 0; b < cand.blobCount; ++b)
            {
                const float bx = cand.x * maxY + cand.blobDx[b] * kCellPitch;
                const float by = cand.y * maxY + cand.blobDy[b] * kCellPitch;
                const float dx = c.x - bx, dy = c.y - by;
                const float dist = std::sqrt(dx * dx + dy * dy) / kCellPitch;
                best = std::max(best, 1.0f - dist / cand.blobRadius[b]);
            }
            if (i == effective.islandCentres[static_cast<size_t>(g)]) { best = 1.0f; }
            if (best > islandPot[static_cast<size_t>(i)])
            {
                islandPot[static_cast<size_t>(i)] = best;
                owner[static_cast<size_t>(i)] = g;
            }
        }
    }

    // Eligible: I > 0, outside M_ref, not next to M_ref, far enough from the edge. Extra rule (not
    // in the spec text, needed for its "groups never merge" guarantee): where two eligible cells of
    // different groups are adjacent, the lower-I cell (ties: higher index) is dropped.
    std::vector<uint8_t> eligible(static_cast<size_t>(n), 0);
    for (int i = 0; i < n; ++i)
    {
        eligible[static_cast<size_t>(i)] = islandPot[static_cast<size_t>(i)] > 0.0f && !out.refMainland[static_cast<size_t>(i)]
            && static_cast<float>(edgeSteps(i)) >= minEdge && !nextToRef(i);
    }
    std::vector<uint8_t> keep = eligible;
    for (int i = 0; i < n; ++i)
    {
        if (!eligible[static_cast<size_t>(i)]) { continue; }
        for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(i), d);
            if (nb == Dia::Pathfinding::kNoCell || !eligible[nb] || owner[nb] == owner[static_cast<size_t>(i)]) { continue; }
            const float mine = islandPot[static_cast<size_t>(i)], theirs = islandPot[nb];
            if (mine < theirs || (mine == theirs && static_cast<uint32_t>(i) > nb)) { keep[static_cast<size_t>(i)] = 0; break; }
        }
    }
    std::vector<int> candidates;
    for (int i = 0; i < n; ++i) { if (keep[static_cast<size_t>(i)]) { candidates.push_back(i); } }
    out.islandCapacity = static_cast<int>(candidates.size());

    std::vector<uint8_t> isAnchor(static_cast<size_t>(n), 0);
    for (int c : effective.islandCentres) { isAnchor[static_cast<size_t>(c)] = 1; }
    std::sort(candidates.begin(), candidates.end(), [&](int a, int b)
    {
        if (isAnchor[static_cast<size_t>(a)] != isAnchor[static_cast<size_t>(b)]) { return isAnchor[static_cast<size_t>(a)] > isAnchor[static_cast<size_t>(b)]; }
        if (islandPot[static_cast<size_t>(a)] != islandPot[static_cast<size_t>(b)]) { return islandPot[static_cast<size_t>(a)] > islandPot[static_cast<size_t>(b)]; }
        return a < b;
    });
    // The budget is a preference bounded by capacity; any unplaced budget returns to the mainland.
    const int placedTarget = groups == 0 ? 0
        : std::min(std::min(out.islandCapacity, std::max(out.islandBudget, 3 * groups)), out.landTarget);
    out.islandMask.assign(static_cast<size_t>(n), 0);
    out.islandGroup.assign(static_cast<size_t>(n), -1);
    for (int k = 0; k < placedTarget; ++k)
    {
        const int c = candidates[static_cast<size_t>(k)];
        out.islandMask[static_cast<size_t>(c)] = 1;
        out.islandGroup[static_cast<size_t>(c)] = owner[static_cast<size_t>(c)];
    }
    out.islandCellsPlaced = placedTarget;

    // Step 7: mainland = the (L - placed) highest-M cells, cleaned up. Island cells are exempt.
    const int mainlandCells = out.landTarget - placedTarget;
    out.mainland = out.TopMask(mainlandCells);
    CleanUpLand(out.mainland, dims, minLandmass, mainlandCells > 0 ? peak : -1);
    for (int i = 0; i < n; ++i)
    {
        if (out.islandMask[static_cast<size_t>(i)]) { out.mainland[static_cast<size_t>(i)] = 0; }
    }
    out.protectedPeak = (mainlandCells > 0 && peak >= 0 && out.mainland[static_cast<size_t>(peak)]) ? peak : -1;

    std::vector<uint8_t> land(static_cast<size_t>(n), 0);
    int landCells = 0;
    for (int i = 0; i < n; ++i)
    {
        land[static_cast<size_t>(i)] = (out.mainland[static_cast<size_t>(i)] || out.islandMask[static_cast<size_t>(i)]) ? 1 : 0;
        landCells += land[static_cast<size_t>(i)];
    }
    out.landCells = landCells;
    effective.islandShare = landCells > 0 ? static_cast<float>(placedTarget) / static_cast<float>(landCells) : 0.0f;

    // Steps 8-9 and layer output.
    std::vector<uint32_t> labels(static_cast<size_t>(n), 0u);
    LabelLandmasses(land, dims, labels.data());
    effective.mainLandmass = out.protectedPeak >= 0 ? static_cast<int>(labels[static_cast<size_t>(out.protectedPeak)]) : 0;

    if (layers.islandPotential != nullptr)
    {
        std::copy(islandPot.begin(), islandPot.end(), layers.islandPotential);
    }
    if (layers.landmass != nullptr)
    {
        std::copy(labels.begin(), labels.end(), layers.landmass);
    }
    if (layers.coastDistance != nullptr)
    {
        ComputeCoastDistance(land, dims, layers.coastDistance);
    }
    if (layers.volcanic != nullptr)
    {
        std::fill(layers.volcanic, layers.volcanic + n, 0u);
        if (groups > 0) { layers.volcanic[effective.islandCentres[0]] = 1u; }   // first group = volcanic island
    }
}

// ---------------------------------------------------------------------------
// Grid helpers (free functions so they can be tested on hand-built masks)
// ---------------------------------------------------------------------------
namespace {

// Flood fill over cells whose mask value equals 'value', starting at 'start'; appends visited
// cells to 'cells' and marks them in 'seen'.
void Flood(const Dia::Pathfinding::HexTopology& topo, const std::vector<uint8_t>& mask, uint8_t value, int start,
           std::vector<uint8_t>& seen, std::vector<int>& cells)
{
    const size_t first = cells.size();
    cells.push_back(start);
    seen[static_cast<size_t>(start)] = 1;
    for (size_t head = first; head < cells.size(); ++head)
    {
        const int c = cells[head];
        for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
        {
            const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
            if (nb != Dia::Pathfinding::kNoCell && !seen[nb] && mask[nb] == value)
            {
                seen[nb] = 1;
                cells.push_back(static_cast<int>(nb));
            }
        }
    }
}

} // namespace

void CleanUpLand(std::vector<uint8_t>& land, GridDimensions dims, int minLandmass, int protectedCell)
{
    const int w = dims.width, h = dims.height;
    if (w <= 0 || h <= 0 || land.size() != static_cast<size_t>(w) * static_cast<size_t>(h)) { return; }
    const Dia::Pathfinding::HexTopology topo(w, h);
    const int n = w * h;

    // Small land components become ocean (the protected cell's component is kept).
    {
        std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
        std::vector<int> cells;
        for (int i = 0; i < n; ++i)
        {
            if (!land[static_cast<size_t>(i)] || seen[static_cast<size_t>(i)]) { continue; }
            cells.clear();
            Flood(topo, land, 1, i, seen, cells);
            if (static_cast<int>(cells.size()) < minLandmass && std::find(cells.begin(), cells.end(), protectedCell) == cells.end())
            {
                for (int c : cells) { land[static_cast<size_t>(c)] = 0; }
            }
        }
    }

    // Ocean not connected to the map-edge ocean becomes land.
    {
        std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
        std::vector<int> cells;
        for (int i = 0; i < n; ++i)
        {
            const int x = i % w, y = i / w;
            const bool onEdge = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            if (onEdge && !land[static_cast<size_t>(i)] && !seen[static_cast<size_t>(i)])
            {
                Flood(topo, land, 0, i, seen, cells);
            }
        }
        for (int i = 0; i < n; ++i)
        {
            if (!land[static_cast<size_t>(i)] && !seen[static_cast<size_t>(i)]) { land[static_cast<size_t>(i)] = 1; }
        }
    }
}

int LabelLandmasses(const std::vector<uint8_t>& land, GridDimensions dims, uint32_t* out)
{
    const int w = dims.width, h = dims.height;
    if (w <= 0 || h <= 0 || out == nullptr) { return 0; }
    const Dia::Pathfinding::HexTopology topo(w, h);
    const int n = w * h;
    std::fill(out, out + n, 0u);
    std::vector<uint8_t> seen(static_cast<size_t>(n), 0);
    std::vector<int> cells;
    uint32_t id = 0;
    for (int i = 0; i < n; ++i)
    {
        if (!land[static_cast<size_t>(i)] || seen[static_cast<size_t>(i)]) { continue; }
        ++id;
        cells.clear();
        Flood(topo, land, 1, i, seen, cells);
        for (int c : cells) { out[c] = id; }
    }
    return static_cast<int>(id);
}

void ComputeCoastDistance(const std::vector<uint8_t>& land, GridDimensions dims, uint32_t* out)
{
    const int w = dims.width, h = dims.height;
    if (w <= 0 || h <= 0 || out == nullptr) { return; }
    const Dia::Pathfinding::HexTopology topo(w, h);
    const int n = w * h;
    std::fill(out, out + n, 0u);
    // Pass 0: sources are ocean cells, results written to land cells. Pass 1: the reverse.
    for (uint8_t sourceValue = 0; sourceValue <= 1; ++sourceValue)
    {
        std::vector<int> dist(static_cast<size_t>(n), -1);
        std::vector<int> queue;
        queue.reserve(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i)
        {
            if (land[static_cast<size_t>(i)] == sourceValue) { dist[static_cast<size_t>(i)] = 0; queue.push_back(i); }
        }
        for (size_t head = 0; head < queue.size(); ++head)
        {
            const int c = queue[head];
            for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
            {
                const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                if (nb != Dia::Pathfinding::kNoCell && dist[nb] < 0)
                {
                    dist[nb] = dist[static_cast<size_t>(c)] + 1;
                    queue.push_back(static_cast<int>(nb));
                }
            }
        }
        for (int i = 0; i < n; ++i)
        {
            if (land[static_cast<size_t>(i)] != sourceValue && dist[static_cast<size_t>(i)] > 0)
            {
                out[i] = static_cast<uint32_t>(dist[static_cast<size_t>(i)]);
            }
        }
    }
}

} // namespace WorldGeneration
} // namespace CoW
