// Tests for CoW::CheckStreamIndependence (F-2 check C1).
#include <gtest/gtest.h>
#include <Modules/F2StreamIndependence.h>
#include <Modules/RandomStreamModule.h>
#include <DiaMaths/Core/RandomStream.h>
#include <memory>
#include <random>
#include <string>

using namespace CoW;

namespace {

const char* const kNames[8] = { "WLD", "A1", "B4", "B6", "C1", "C2", "B5", "C4" };

// Independent streams: one mt19937 per index.
class GoodSet : public F2IStreamSet
{
public:
    GoodSet() { for (int i = 0; i < 8; ++i) mGen[i].seed(1000u + i); }
    uint32_t Draw(int i) override { return mGen[i](); }
    std::mt19937 mGen[8];
};

// Streams 2 and 5 share one generator (shared-state leak).
class LeakySet : public F2IStreamSet
{
public:
    LeakySet() { for (int i = 0; i < 8; ++i) mGen[i].seed(1000u + i); }
    uint32_t Draw(int i) override { return (i == 5 ? mGen[2] : mGen[i])(); }
    std::mt19937 mGen[8];
};

struct LiveStreams
{
    std::unique_ptr<Dia::Maths::RandomStream> owned[8];
    Dia::Maths::RandomStream* ptrs[8] = {};
    explicit LiveStreams(unsigned int baseSeed, bool preDraw)
    {
        for (int i = 0; i < 8; ++i)
        {
            owned[i] = std::make_unique<Dia::Maths::RandomStream>(baseSeed + i * 37u);
            if (preDraw) for (int d = 0; d < i * 3; ++d) owned[i]->RandomInt();
            ptrs[i] = owned[i].get();
        }
    }
};

} // namespace

TEST(F2StreamIndependence, PassesOnIndependentFakeStreams)
{
    const F2StreamIndependenceResult r = CheckStreamIndependence(
        []() -> std::unique_ptr<F2IStreamSet> { return std::make_unique<GoodSet>(); }, kNames);
    EXPECT_TRUE(r.passed) << r.summary;
    EXPECT_EQ(r.independentCount, 8);
    EXPECT_EQ(r.summary, "8 / 8 streams independent over 64 draws");
}

TEST(F2StreamIndependence, PassesOnRealSeededStreams)
{
    LiveStreams s(500u, false);
    const F2StreamIndependenceResult r = CheckStreamIndependence(s.ptrs);
    EXPECT_TRUE(r.passed) << r.summary;
    EXPECT_EQ(r.summary, "8 / 8 streams independent over 64 draws");
}

TEST(F2StreamIndependence, SharedStateLeakFailsAndNamesOffender)
{
    const F2StreamIndependenceResult r = CheckStreamIndependence(
        []() -> std::unique_ptr<F2IStreamSet> { return std::make_unique<LeakySet>(); }, kNames);
    EXPECT_FALSE(r.passed);
    ASSERT_GE(r.firstDivergingStream, 0);
    EXPECT_GE(r.firstDivergingDraw, 0);
    EXPECT_LT(r.independentCount, 8);
    const std::string name = kNames[r.firstDivergingStream];
    EXPECT_NE(r.summary.find(name), std::string::npos) << r.summary;
    EXPECT_NE(r.summary.find("diverged at draw " + std::to_string(r.firstDivergingDraw)), std::string::npos) << r.summary;
}

TEST(F2StreamIndependence, LiveStreamStateUnchanged)
{
    LiveStreams s(77u, true);
    unsigned int seeds[8]; uint64_t counts[8];
    for (int i = 0; i < 8; ++i) { seeds[i] = s.ptrs[i]->GetSeed(); counts[i] = s.ptrs[i]->GetDrawCount(); }

    CheckStreamIndependence(s.ptrs);

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_EQ(s.ptrs[i]->GetSeed(), seeds[i]) << i;
        EXPECT_EQ(s.ptrs[i]->GetDrawCount(), counts[i]) << i;
    }
}

TEST(F2StreamIndependence, MissingLiveStreamFails)
{
    Dia::Maths::RandomStream* ptrs[8] = {};
    const F2StreamIndependenceResult r = CheckStreamIndependence(ptrs);
    EXPECT_FALSE(r.passed);
    EXPECT_NE(r.summary.find("WLD"), std::string::npos);
}
