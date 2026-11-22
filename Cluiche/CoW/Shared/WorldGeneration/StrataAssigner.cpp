// Strata assigner seam: registry, built-in assigner and contract wrapper. See StrataAssigner.h.
#include <Shared/WorldGeneration/StrataAssigner.h>

#include <DiaObservation/Log/DiaLog.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

namespace
{
    // Fills values (one per cell) with the metric; false (ctx.error written) when it is unknown or unavailable.
    bool ResolveMetric(const StrataAssignContext& ctx, const Draoi::HexWorld::StratumSetSpec& set, std::vector<float>& values)
    {
        const size_t n = values.size();
        const std::string metric = set.metric;
        if (metric == Draoi::HexWorld::kMetricElevation)
        {
            const Cell* cells = ctx.world.Cells();
            for (size_t i = 0; i < n; ++i) { values[i] = cells[i].elevation; }
            return true;
        }
        if (metric == Draoi::HexWorld::kMetricLatitude)
        {
            const int w = std::max(ctx.dims.width, 1);
            const float h = static_cast<float>(std::max(ctx.dims.height, 1));
            for (size_t i = 0; i < n; ++i)
            {
                const float row = static_cast<float>(static_cast<int>(i) / w);
                values[i] = std::fabs(2.0f * (row + 0.5f) / h - 1.0f);
            }
            return true;
        }
        const StrataFieldProvider* provider = ctx.registry != nullptr ? ctx.registry->FindField(metric.c_str()) : nullptr;
        std::string problem;
        if (provider == nullptr)
        {
            problem = "stratum set '" + std::string(set.id) + "': unknown banding metric '" + metric + "' (not " + Draoi::HexWorld::kMetricCoastCost + ", " +
                      Draoi::HexWorld::kMetricElevation + ", " + Draoi::HexWorld::kMetricLatitude + " or a registered cell field)";
        }
        else if (!(*provider)(ctx, std::span<float>(values.data(), values.size())))
        {
            problem = "stratum set '" + std::string(set.id) + "': cell field '" + metric + "' is not available";
        }
        if (!problem.empty())
        {
            if (ctx.error != nullptr) { *ctx.error = problem; }
            DIA_LOG_ERROR("HexWorld", "%s", problem.c_str());
            return false;
        }
        return true;
    }
}

void CoastCostBandsAssigner::Assign(const StrataAssignContext& ctx, const Draoi::HexWorld::StratumSetSpec& set, std::span<StratumIndex> out)
{
    Stage8Debug local;
    if (std::strcmp(set.metric, Draoi::HexWorld::kMetricCoastCost) != 0)
    {
        std::vector<float> values(out.size(), 0.0f);
        if (!ResolveMetric(ctx, set, values)) { return; }
        // The same banding over the metric: it supplies both the cut costs and each cell's cost. Coast-distance rules are
        // coast_cost rules (the table loader rejects them on other metrics) so none apply here.
        BandStrata(set, ctx.dims, ctx.mainLandmass, ctx.landmass, ctx.coastDistance, values.data(), values.data(), out.data(), local);
        if (ctx.debug != nullptr) { *ctx.debug = local; }
        return;
    }
    BandStrata(set, ctx.dims, ctx.mainLandmass, ctx.landmass, ctx.coastDistance, ctx.zoneCostBase, ctx.zoneCost, out.data(), local);
    if (ctx.debug != nullptr)
    {
        *ctx.debug = local;
    }
}

StrataAssignerRegistry::StrataAssignerRegistry()
{
    mEntries.push_back({ Draoi::HexWorld::kBuiltinAssignerType, &mBuiltin });
}

bool StrataAssignerRegistry::Register(const char* typeName, IStrataAssigner* assigner)
{
    if (assigner == nullptr || typeName == nullptr || typeName[0] == '\0' || std::strlen(typeName) > kMaxTypeNameLength ||
        Find(typeName) != nullptr)
    {
        return false;
    }
    mEntries.push_back({ typeName, assigner });
    return true;
}

IStrataAssigner* StrataAssignerRegistry::Find(const char* typeName) const
{
    if (typeName == nullptr)
    {
        return nullptr;
    }
    for (const Entry& e : mEntries)
    {
        if (e.name == typeName)
        {
            return e.assigner;
        }
    }
    return nullptr;
}

Draoi::HexWorld::AssignerTypeCheck StrataAssignerRegistry::TypeCheck() const
{
    return [this](const char* typeName) { return Has(typeName); };
}

bool StrataAssignerRegistry::RegisterField(const char* name, StrataFieldProvider provider)
{
    if (!provider || name == nullptr || name[0] == '\0' || std::strlen(name) > kMaxTypeNameLength || HasMetric(name))
    {
        return false;
    }
    mFields.push_back({ name, std::move(provider) });
    return true;
}

const StrataFieldProvider* StrataAssignerRegistry::FindField(const char* name) const
{
    if (name == nullptr) { return nullptr; }
    for (const Field& f : mFields)
    {
        if (f.name == name) { return &f.provider; }
    }
    return nullptr;
}

bool StrataAssignerRegistry::HasMetric(const char* name) const
{
    if (name == nullptr) { return false; }
    return std::strcmp(name, Draoi::HexWorld::kMetricCoastCost) == 0 || std::strcmp(name, Draoi::HexWorld::kMetricElevation) == 0 ||
           std::strcmp(name, Draoi::HexWorld::kMetricLatitude) == 0 || FindField(name) != nullptr;
}

Draoi::HexWorld::MetricCheck StrataAssignerRegistry::MetricCheck() const
{
    return [this](const char* metric) { return HasMetric(metric); };
}

bool RunStrataAssigner(const char* typeName, IStrataAssigner& assigner, const StrataAssignContext& ctx,
                       const Draoi::HexWorld::StratumSetSpec& set, std::span<StratumIndex> out, StrataAssignError& err)
{
    err = StrataAssignError{};
    std::fill(out.begin(), out.end(), kNoStratum);
    if (ctx.error != nullptr) { ctx.error->clear(); }
    assigner.Assign(ctx, set, out);
    if (ctx.error != nullptr && !ctx.error->empty())
    {
        err.assigner = typeName != nullptr ? typeName : "";
        err.reason = "strata assigner '" + err.assigner + "' could not run: " + *ctx.error;
        DIA_LOG_ERROR("HexWorld", "%s", err.reason.c_str());
        return false;
    }

    const uint32_t count = std::min(set.count, Draoi::HexWorld::StratumTable::kMaxStrata);
    for (size_t i = 0; i < out.size(); ++i)
    {
        const bool land = ctx.landmass != nullptr && ctx.landmass[i] != 0u;
        const uint32_t v = out[i];
        const char* problem = nullptr;
        if (v > count)
        {
            problem = "value above the stratum count";
        }
        else if (!land && v != 0u)
        {
            problem = "non-zero value on an ocean cell";
        }
        else if (land && v == 0u)
        {
            problem = "land cell left unassigned (0)";
        }
        if (problem != nullptr)
        {
            err.assigner = typeName != nullptr ? typeName : "";
            err.cell = static_cast<int>(i);
            err.value = v;
            err.reason = "strata assigner '" + err.assigner + "' broke its contract at cell " + std::to_string(i) + ": " + problem +
                         " (value " + std::to_string(v) + ", set has " + std::to_string(count) + " strata)";
            DIA_LOG_ERROR("HexWorld", "%s", err.reason.c_str());
            return false;
        }
    }
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
