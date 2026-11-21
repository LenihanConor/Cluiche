// S-1 Reachability Validator. See ReachabilityValidator.h.
#include <Shared/WorldGeneration/ReachabilityValidator.h>
#include <Shared/WorldGeneration/NavigationQuery.h>

#include <DiaObservation/Log/DiaLog.h>

#include <cstdio>

namespace CoW {
namespace WorldGeneration {

ValidationResult ReachabilityValidator::Validate(const WorldStructure& world, const ValidationParams& params)
{
    ValidationResult result;
    bool allSatisfied = true;

    if (params.profiles.Size() == 0)
    {
        Diagnostic d;
        std::snprintf(d.text, sizeof(d.text), "no navigation profile was supplied: every reachability query fails");
        result.diagnostics.Add(d);
        allSatisfied = false;
        DIA_LOG_ERROR("CoW", "ReachabilityValidator: no navigation profile was supplied");
    }

    const NavigationQuery query(world, params);
    const ValidationContext context{ world, query };
    for (unsigned i = 0; i < params.guarantees.Size(); ++i)
    {
        const IWorldGuarantee* guarantee = params.guarantees[i];
        GuaranteeResult r = guarantee->Check(context);
        r.idCrc = guarantee->GetId().Value();   // the injected id is authoritative
        allSatisfied = allSatisfied && r.satisfied;
        result.guarantees.Add(std::move(r));
    }

    result.passed = allSatisfied;
    return result;
}

} // namespace WorldGeneration
} // namespace CoW
