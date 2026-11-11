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

class TestGuarantee : public ILayoutGuarantee
{
public:
    explicit TestGuarantee(const char* name) : mName(name) {}
    const char* Name() const override { return mName.c_str(); }
private:
    std::string mName;
};

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
