// Test placers for the S-1 site registry / placement tests. Header-only.
#pragma once

#include "../SiteRegistry.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Places one site of its kind at a fixed cell, or returns the configured failure.
class TestPlacer : public ISitePlacer
{
public:
    TestPlacer(const char* kindId, std::initializer_list<const char*> dependsOn = {}, uint32_t cell = 0,
               PlaceResult::Code result = PlaceResult::Code::Ok)
        : mInfo(MakeSiteKindInfo(Dia::Core::StringCRC(kindId), kindId, 0xFF00FF, SiteShape::Dot))
        , mCell(cell), mResult(result)
    {
        for (const char* d : dependsOn) mDeps.push_back(Dia::Core::StringCRC(d));
    }

    const SiteKindInfo& Info() const override { return mInfo; }
    std::span<const Dia::Core::StringCRC> DependsOn() const override { return { mDeps.data(), mDeps.size() }; }

    PlaceResult Place(const PlacementContext&, Dia::Maths::RandomStream&, SiteSet& out) override
    {
        ++placeCalls;
        if (mResult == PlaceResult::Code::Retryable)    return PlaceResult::Retryable("test retryable");
        if (mResult == PlaceResult::Code::TerrainCause) return PlaceResult::TerrainCause("test terrain cause");
        Site s; s.kindCrc = mInfo.kindCrc; s.cell = mCell;
        out.Add(s);
        return PlaceResult::Ok();
    }

    int placeCalls = 0;

private:
    SiteKindInfo mInfo;
    std::vector<Dia::Core::StringCRC> mDeps;
    uint32_t mCell;
    PlaceResult::Code mResult;
};

// Convenience: A (no deps), B (depends on A), C (no deps).
struct TestPlacerSet
{
    TestPlacer a{ "Test.A" };
    TestPlacer b{ "Test.B", { "Test.A" } };
    TestPlacer c{ "Test.C" };
};

// A registrable guarantee that is always satisfied (id = the name).
class TestGuarantee : public IWorldGuarantee
{
public:
    explicit TestGuarantee(const char* name) : mName(name) {}
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC(mName.c_str()); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        GuaranteeResult r;
        r.idCrc = GetId().Value();
        r.satisfied = true;
        return r;
    }
private:
    std::string mName;
};

// Fails Retryable while PlacementContext::attempt < succeedFromAttempt, then places one site (attempt-gated, so
// the retry tests control exactly which attempts fail without depending on the seed).
class AttemptGatedPlacer : public ISitePlacer
{
public:
    AttemptGatedPlacer(const char* kindId, int succeedFromAttempt, PlaceResult::Code failure = PlaceResult::Code::Retryable)
        : mInfo(MakeSiteKindInfo(Dia::Core::StringCRC(kindId), kindId, 0x00FFFF, SiteShape::Dot))
        , mSucceedFrom(succeedFromAttempt), mFailure(failure) {}

    const SiteKindInfo& Info() const override { return mInfo; }
    std::span<const Dia::Core::StringCRC> DependsOn() const override { return {}; }
    PlaceResult Place(const PlacementContext& ctx, Dia::Maths::RandomStream&, SiteSet& out) override
    {
        ++placeCalls;
        if (ctx.attempt < mSucceedFrom)
        {
            return mFailure == PlaceResult::Code::TerrainCause ? PlaceResult::TerrainCause("gated terrain cause")
                                                              : PlaceResult::Retryable("gated retryable");
        }
        Site s; s.kindCrc = mInfo.kindCrc; s.cell = 0;
        out.Add(s);
        return PlaceResult::Ok();
    }

    int placeCalls = 0;

private:
    SiteKindInfo mInfo;
    int mSucceedFrom;
    PlaceResult::Code mFailure;
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
