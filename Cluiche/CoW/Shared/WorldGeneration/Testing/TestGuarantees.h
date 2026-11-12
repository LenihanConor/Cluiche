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

// Always satisfied; counts Check() calls (AC6: the validator runs once per attempt, never after success).
class CountingGuarantee : public IWorldGuarantee
{
public:
    explicit CountingGuarantee(const char* id) : mId(id) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        ++calls;
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = true;
        return r;
    }
    mutable int calls = 0;
private:
    std::string mId;
};

// Fails its first N Check() calls, then passes. Stateful on purpose (a test double controlling which attempts fail).
class FailFirstNGuarantee : public IWorldGuarantee
{
public:
    FailFirstNGuarantee(const char* id, int failCalls, bool terrainCause = false)
        : mId(id), mFailCalls(failCalls), mTerrainCause(terrainCause) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = calls++ >= mFailCalls;
        if (!r.satisfied) { r.AddDiagnostic(mTerrainCause ? "%s fails (terrain cause: test)" : "%s fails", mId.c_str()); }
        return r;
    }
    mutable int calls = 0;
private:
    std::string mId;
    int mFailCalls;
    bool mTerrainCause;
};

// Depends on the placed world (so it varies with the attempt's sub-seed): satisfied when the region count is even.
// Deterministic for a given seed, which is what the retry-sequence tests need.
class EvenRegionCountGuarantee : public IWorldGuarantee
{
public:
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.Guarantee.EvenRegionCount"); }
    GuaranteeResult Check(const ValidationContext& ctx) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        const unsigned n = ctx.world.regions.regions.Size();
        r.satisfied = (n % 2) == 0;
        if (!r.satisfied) { r.AddDiagnostic("region count %u is odd", n); }
        return r;
    }
};

// Fails without any diagnostic (a vacuous failure must never be mistaken for a terrain cause).
class SilentFailGuarantee : public IWorldGuarantee
{
public:
    explicit SilentFailGuarantee(const char* id) : mId(id) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mId.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = false;
        return r;
    }
private:
    std::string mId;
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
