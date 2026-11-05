// S-1 Terrain Generator: Stage 2 (Mountains). See TerrainMountains.h.
// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/terrain-generator.md
#pragma fp_contract(off)

#include <Shared/WorldGeneration/TerrainMountains.h>
#include <Shared/WorldGeneration/TerrainGenerator.h>

#include <DiaPathfinding/HexTopology.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace CoW {
namespace WorldGeneration {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kReachStep = 0.25f;
constexpr int   kInradiusDirections = 24;

using Dia::Pathfinding::HexTopology;
using Dia::Pathfinding::CellCoord;

float PointSegDist(Vec2 p, Vec2 a, Vec2 b)
{
    const float abx = b.x - a.x, aby = b.y - a.y;
    const float len2 = abx * abx + aby * aby;
    float u = 0.0f;
    if (len2 > 0.0f)
    {
        u = std::clamp(((p.x - a.x) * abx + (p.y - a.y) * aby) / len2, 0.0f, 1.0f);
    }
    const float dx = p.x - (a.x + u * abx);
    const float dy = p.y - (a.y + u * aby);
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace

Vec2 CellPosition(int x, int y)
{
    const Dia::Maths::Vector2D c = HexTopology::CellCenter(CellCoord{ x, y }, kCellSize);
    return Vec2{ c.x, c.y };
}

int FindMountainCentre(const std::vector<uint8_t>& mask, GridDimensions dims, Vec2& centre)
{
    const int n = dims.width * dims.height;
    double sx = 0.0, sy = 0.0;
    int count = 0;
    for (int i = 0; i < n; ++i)
    {
        if (mask[static_cast<size_t>(i)])
        {
            const Vec2 p = CellPosition(i % dims.width, i / dims.width);
            sx += p.x;
            sy += p.y;
            ++count;
        }
    }
    if (count == 0)
    {
        centre = Vec2{};
        return -1;
    }
    const Vec2 mean{ static_cast<float>(sx / count), static_cast<float>(sy / count) };
    const CellCoord cc = HexTopology::CellFromPoint(Dia::Maths::Vector2D(mean.x, mean.y), kCellSize);
    if (cc.x >= 0 && cc.y >= 0 && cc.x < dims.width && cc.y < dims.height
        && mask[static_cast<size_t>(cc.y) * dims.width + cc.x])
    {
        centre = mean;
        return cc.y * dims.width + cc.x;
    }
    int best = -1;
    float bestD = std::numeric_limits<float>::max();
    for (int i = 0; i < n; ++i)
    {
        if (!mask[static_cast<size_t>(i)]) { continue; }
        const Vec2 p = CellPosition(i % dims.width, i / dims.width);
        const float d = (p.x - mean.x) * (p.x - mean.x) + (p.y - mean.y) * (p.y - mean.y);
        if (d < bestD) { bestD = d; best = i; }   // strict < keeps the lowest index on ties
    }
    centre = CellPosition(best % dims.width, best / dims.width);
    return best;
}

float Stage2Debug::Reach(float phi) const
{
    const float dx = std::cos(phi), dy = std::sin(phi);
    const float maxDist = static_cast<float>(dims.width + dims.height) * 2.0f;
    for (float s = 0.0f; s <= maxDist; s += kReachStep)
    {
        const CellCoord cc = HexTopology::CellFromPoint(
            Dia::Maths::Vector2D(centre.x + dx * s, centre.y + dy * s), kCellSize);
        const bool land = cc.x >= 0 && cc.y >= 0 && cc.x < dims.width && cc.y < dims.height
            && mainMask[static_cast<size_t>(cc.y) * dims.width + cc.x] != 0;
        if (!land) { return s; }
    }
    return maxDist;
}

Vec2 Stage2Debug::Line(float t) const
{
    const float s = 2.0f * t - 1.0f;
    const float r = (t > 0.5f ? reachFwd : reachBack) * spineReach;
    const float cx = std::cos(theta), cy = std::sin(theta);
    return Vec2{ centre.x - cy * offset + s * r * cx, centre.y + cx * offset + s * r * cy };
}

Vec2 Stage2Debug::Ring(float t) const
{
    const float phi = theta - kPi * 0.5f - 2.0f * kPi * t;
    const float r = ringReach * Reach(phi);
    return Vec2{ centre.x + r * std::cos(phi), centre.y + r * std::sin(phi) };
}

Vec2 Stage2Debug::Curve(float t) const
{
    const Vec2 l = Line(t);
    if (spineToRing <= 0.0f) { return l; }
    const Vec2 g = Ring(t);
    return Vec2{ l.x + (g.x - l.x) * spineToRing, l.y + (g.y - l.y) * spineToRing };
}

float Stage2Debug::ArcAt(float t) const
{
    if (curveArc.empty()) { return 0.0f; }
    const float f = std::clamp(t, 0.0f, 1.0f) * static_cast<float>(kCurveSegments);
    const int k = std::min(static_cast<int>(f), kCurveSegments - 1);
    const float u = f - static_cast<float>(k);
    return curveArc[static_cast<size_t>(k)] + u * (curveArc[static_cast<size_t>(k) + 1] - curveArc[static_cast<size_t>(k)]);
}

void ComputeStage2(const TerrainRules& rules, const WorldShapeParams& shape, GridDimensions dims,
                   const SeedFields& seeds, const std::vector<uint8_t>& mainMask,
                   float* ridgeMask, uint32_t* passMask, Stage2Debug& out, Effective& effective)
{
    const int n = std::max(dims.width, 0) * std::max(dims.height, 0);
    out = Stage2Debug{};
    out.dims = dims;
    out.mainMask = mainMask;
    out.rawRidge.assign(static_cast<size_t>(n), 0.0f);
    effective.passes = 0;
    if (ridgeMask != nullptr) { std::fill(ridgeMask, ridgeMask + n, 0.0f); }
    if (passMask != nullptr)  { std::fill(passMask, passMask + n, 0u); }

    out.centreCell = FindMountainCentre(mainMask, dims, out.centre);
    if (out.centreCell < 0) { return; }
    out.valid = true;

    float deg = std::fmod(shape.orientationDeg, 360.0f);
    if (deg < 0.0f) { deg += 360.0f; }
    out.theta = deg * kPi / 180.0f;
    out.spineToRing = std::clamp(shape.spineToRing, 0.0f, 1.0f);
    out.spineReach = rules.kSpineReach();
    out.ringReach = rules.kRingReach();

    out.inradius = std::numeric_limits<float>::max();
    for (int k = 0; k < kInradiusDirections; ++k)
    {
        out.inradius = std::min(out.inradius, out.Reach(2.0f * kPi * static_cast<float>(k) / kInradiusDirections));
    }
    out.offset = seeds.spineOffset * out.inradius;
    out.reachFwd = out.Reach(out.theta);
    out.reachBack = out.Reach(out.theta + kPi);

    const float mt = std::clamp(shape.mountainousness, 0.0f, 1.0f);
    out.width = rules.kRidgeWidthMin() + (rules.kRidgeWidthMax() - rules.kRidgeWidthMin()) * mt;
    out.gap = 0.7f * out.width + 0.5f;

    // Polyline and cumulative arc length.
    std::vector<Vec2> pts(static_cast<size_t>(kCurveSegments) + 1);
    out.curveArc.assign(pts.size(), 0.0f);
    for (int k = 0; k <= kCurveSegments; ++k)
    {
        pts[static_cast<size_t>(k)] = out.Curve(static_cast<float>(k) / static_cast<float>(kCurveSegments));
        if (k > 0)
        {
            const Vec2 a = pts[static_cast<size_t>(k) - 1], b = pts[static_cast<size_t>(k)];
            out.curveArc[static_cast<size_t>(k)] = out.curveArc[static_cast<size_t>(k) - 1]
                + std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
        }
    }
    out.curveLength = out.curveArc.back();

    // Raw mask.
    std::vector<float> mask(static_cast<size_t>(n), 0.0f);
    for (int i = 0; i < n; ++i)
    {
        if (!mainMask[static_cast<size_t>(i)]) { continue; }
        const Vec2 p = CellPosition(i % dims.width, i / dims.width);
        float d = std::numeric_limits<float>::max();
        for (int k = 0; k < kCurveSegments; ++k)
        {
            d = std::min(d, PointSegDist(p, pts[static_cast<size_t>(k)], pts[static_cast<size_t>(k) + 1]));
        }
        mask[static_cast<size_t>(i)] = std::max(0.0f, 1.0f - d / out.width);
    }
    out.rawRidge = mask;

    // Connected groups of ridge cells (>= kRidgeCellMin) over the six hex neighbours.
    const HexTopology topo(dims.width, dims.height);
    const float cellMin = rules.kRidgeCellMin();
    auto groupsOf = [&](const std::vector<float>& m, std::vector<int>& label)
    {
        label.assign(static_cast<size_t>(n), -1);
        std::vector<int> sizes;
        std::vector<int> queue;
        for (int i = 0; i < n; ++i)
        {
            if (m[static_cast<size_t>(i)] < cellMin || label[static_cast<size_t>(i)] >= 0) { continue; }
            const int id = static_cast<int>(sizes.size());
            sizes.push_back(0);
            queue.assign(1, i);
            label[static_cast<size_t>(i)] = id;
            for (size_t head = 0; head < queue.size(); ++head)
            {
                const int c = queue[head];
                ++sizes[static_cast<size_t>(id)];
                for (uint32_t d = 0; d < Dia::Pathfinding::kHexDirections; ++d)
                {
                    const uint32_t nb = topo.NeighbourAt(static_cast<uint32_t>(c), d);
                    if (nb != Dia::Pathfinding::kNoCell && label[nb] < 0 && m[nb] >= cellMin)
                    {
                        label[nb] = id;
                        queue.push_back(static_cast<int>(nb));
                    }
                }
            }
        }
        return sizes;
    };

    // One range: keep only the largest group (ties: the group found first, i.e. lowest cell index).
    std::vector<int> label;
    {
        const std::vector<int> sizes = groupsOf(mask, label);
        int best = -1, bestSize = 0;
        for (int g = 0; g < static_cast<int>(sizes.size()); ++g)
        {
            if (sizes[static_cast<size_t>(g)] > bestSize) { bestSize = sizes[static_cast<size_t>(g)]; best = g; }
        }
        for (int i = 0; i < n; ++i)
        {
            if (label[static_cast<size_t>(i)] != best) { mask[static_cast<size_t>(i)] = 0.0f; }
        }
    }

    // Passes: walk the candidates in rank order, accept when far enough (arc length) from all accepted.
    const int wanted = std::min(static_cast<int>(shape.passes), kPassCandidates);
    const float minArc = 2.0f * out.gap + rules.kMinPiece();
    std::vector<float> acceptedArc;
    for (int j = 0; j < kPassCandidates && static_cast<int>(out.passT.size()) < wanted; ++j)
    {
        const float t = seeds.passT[j];
        const float arc = out.ArcAt(t);
        bool ok = true;
        for (float a : acceptedArc)
        {
            if (std::fabs(arc - a) < minArc) { ok = false; break; }
        }
        if (ok)
        {
            acceptedArc.push_back(arc);
            out.passT.push_back(t);
        }
    }
    effective.passes = static_cast<int>(out.passT.size());

    // Every main-land cell within g of an accepted pass point is a pass cell; its mask is
    // multiplied by kPassFloor once, however many gaps cover it.
    std::vector<uint32_t> pass(static_cast<size_t>(n), 0u);
    for (float t : out.passT)
    {
        const Vec2 pp = out.Curve(t);
        for (int i = 0; i < n; ++i)
        {
            if (!mainMask[static_cast<size_t>(i)]) { continue; }
            const Vec2 p = CellPosition(i % dims.width, i / dims.width);
            const float dx = p.x - pp.x, dy = p.y - pp.y;
            if (std::sqrt(dx * dx + dy * dy) <= out.gap) { pass[static_cast<size_t>(i)] = 1u; }
        }
    }
    for (int i = 0; i < n; ++i)
    {
        if (pass[static_cast<size_t>(i)]) { mask[static_cast<size_t>(i)] *= rules.kPassFloor(); }
    }

    if (ridgeMask != nullptr) { std::copy(mask.begin(), mask.end(), ridgeMask); }
    if (passMask != nullptr)  { std::copy(pass.begin(), pass.end(), passMask); }
    out.ridgeGroups = static_cast<int>(groupsOf(mask, label).size());
}

} // namespace WorldGeneration
} // namespace CoW
