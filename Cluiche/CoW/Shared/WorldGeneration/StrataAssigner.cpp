// Strata assigner seam: registry, built-in assigner and contract wrapper. See StrataAssigner.h.
#include <Shared/WorldGeneration/StrataAssigner.h>

#include <DiaObservation/Log/DiaLog.h>

#include <algorithm>
#include <cstring>

namespace CoW {
namespace WorldGeneration {

void CoastCostBandsAssigner::Assign(const StrataAssignContext& ctx, const World::StratumSetSpec& set, std::span<StratumIndex> out)
{
    Stage8Debug local;
    BandStrata(set, ctx.dims, ctx.mainLandmass, ctx.landmass, ctx.coastDistance, ctx.zoneCostBase, ctx.zoneCost, out.data(), local);
    if (ctx.debug != nullptr)
    {
        *ctx.debug = local;
    }
}

StrataAssignerRegistry::StrataAssignerRegistry()
{
    mEntries.push_back({ World::kBuiltinAssignerType, &mBuiltin });
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

World::AssignerTypeCheck StrataAssignerRegistry::TypeCheck() const
{
    return [this](const char* typeName) { return Has(typeName); };
}

bool RunStrataAssigner(const char* typeName, IStrataAssigner& assigner, const StrataAssignContext& ctx,
                       const World::StratumSetSpec& set, std::span<StratumIndex> out, StrataAssignError& err)
{
    err = StrataAssignError{};
    std::fill(out.begin(), out.end(), kNoStratum);
    assigner.Assign(ctx, set, out);

    const uint32_t count = std::min(set.count, World::StratumTable::kMaxStrata);
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
            DIA_LOG_ERROR("CoW", "%s", err.reason.c_str());
            return false;
        }
    }
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
