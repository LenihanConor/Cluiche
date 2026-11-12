// S-1 Reachability Validator test doubles: guarantees. Header-only.
#pragma once

#include <Shared/WorldGeneration/ValidationTypes.h>

#include <string>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Always satisfied or always failed, with one diagnostic when failed.
class FixedGuarantee : public IWorldGuarantee
{
public:
    FixedGuarantee(const char* id, bool satisfied) : mId(id), mSatisfied(satisfied) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = mSatisfied;
        if (!mSatisfied) { r.AddDiagnostic("%s always fails", mId.c_str()); }
        return r;
    }
private:
    std::string mId;
    bool mSatisfied;
};

// Asks the injected navigation about one pair and reports the answer as satisfied (proves custom guarantees see
// the same answers as the built-ins).
class NavigationProbeGuarantee : public IWorldGuarantee
{
public:
    NavigationProbeGuarantee(const char* id, uint32_t from, uint32_t to) : mId(id), mFrom(from), mTo(to) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext& ctx) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = ctx.navigation.IsReachable(mFrom, mTo);
        return r;
    }
private:
    std::string mId;
    uint32_t mFrom, mTo;
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
