// S-1 Reachability Validator: WorldLayoutGuarantee. See WorldLayoutGuarantee.h.
#include <Shared/WorldGeneration/WorldLayoutGuarantee.h>

namespace CoW {
namespace WorldGeneration {

using Dia::Core::StringCRC;

StringCRC WorldLayoutGuarantee::GetId() const { return StringCRC("CoW.Guarantee.WorldLayout"); }

GuaranteeResult WorldLayoutGuarantee::Check(const ValidationContext& context) const
{
    const LayoutReport report = CheckLayout(context.world, mRules, mLimits);

    GuaranteeResult result;
    result.idCrc = GetId().Value();
    result.satisfied = report.ok;

    const bool terrainLimited = mLimits.regionLimitedBy != kNoZone;
    for (const LayoutViolation& v : report.violations)
    {
        const bool terrainCause = terrainLimited && (v.code == LayoutCode::RegionCountRange || v.code == LayoutCode::AbsorbedOverCap);
        if (v.region != kNoRegion)
        {
            result.AddDiagnostic("%s: count %u, region %u%s", LayoutCodeName(v.code), v.count, static_cast<unsigned>(v.region),
                                 terrainCause ? " (terrain cause: terrain-limited world)" : "");
        }
        else
        {
            result.AddDiagnostic("%s: count %u%s", LayoutCodeName(v.code), v.count,
                                 terrainCause ? " (terrain cause: terrain-limited world)" : "");
        }
        for (unsigned i = 0; i < v.cellCount; ++i) { result.violatingCells.Add(v.cells[i]); }

        Measurement m;
        m.keyCrc = MeasurementKeys::ViolationCount().Value();
        m.a = v.cellCount > 0 ? v.cells[0] : kNoCellIndex;
        m.tag = static_cast<int32_t>(v.code);
        m.value = static_cast<float>(v.count);
        result.measurements.Add(m);
    }
    return result;
}

} // namespace WorldGeneration
} // namespace CoW
