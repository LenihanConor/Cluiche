// Tests for CoW::F2Fingerprint: field-by-field capture and compare.
#include <gtest/gtest.h>
#include <Modules/F2Fingerprint.h>
#include <Modules/RandomStreamModule.h>
#include <DiaMaths/Core/RandomStream.h>
#include <memory>
#include <string>

using namespace CoW;

namespace {

struct Streams
{
    std::unique_ptr<Dia::Maths::RandomStream> owned[kF2FingerprintStreamCount];
    Dia::Maths::RandomStream* ptrs[kF2FingerprintStreamCount] = {};

    Streams()
    {
        for (int i = 0; i < kF2FingerprintStreamCount; ++i)
        {
            owned[i] = std::make_unique<Dia::Maths::RandomStream>(static_cast<unsigned int>((i + 1) * 10000));
            ptrs[i]  = owned[i].get();
        }
    }
};

F2Fingerprint Base()
{
    Streams s;
    return F2Fingerprint::Capture(s.ptrs, GameSpeedState{});
}

void ExpectOnly(const F2Fingerprint& a, const F2Fingerprint& b, const std::string& field)
{
    const F2CompareResult r = F2Fingerprint::Compare(a, b);
    ASSERT_EQ(r.differingFields.size(), 1u) << r.Summary();
    EXPECT_EQ(r.differingFields[0], field);
    EXPECT_FALSE(a == b);
}

} // namespace

TEST(F2Fingerprint, EqualStateHasNoDiffs)
{
    const F2Fingerprint a = Base();
    const F2Fingerprint b = Base();
    const F2CompareResult r = F2Fingerprint::Compare(a, b);
    EXPECT_TRUE(r.IsIdentical());
    EXPECT_TRUE(r.differingFields.empty());
    EXPECT_EQ(r.Summary(), "identical");
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
}

TEST(F2Fingerprint, OneStreamSeedDiffNamesThatField)
{
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        F2Fingerprint a = Base();
        F2Fingerprint b = a;
        b.streams[i].seed += 1;
        ExpectOnly(a, b, std::string("stream ") + RandomStreamModule::kStreamNames[i] + " seed");
    }
}

TEST(F2Fingerprint, OneStreamDrawCountDiffNamesThatField)
{
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        F2Fingerprint a = Base();
        F2Fingerprint b = a;
        b.streams[i].drawCount += 1;
        ExpectOnly(a, b, std::string("stream ") + RandomStreamModule::kStreamNames[i] + " drawCount");
    }
}

TEST(F2Fingerprint, DayCounterDiff)
{
    F2Fingerprint a = Base();
    F2Fingerprint b = a;
    b.dayCounter = 5;
    ExpectOnly(a, b, "dayCounter");
}

TEST(F2Fingerprint, EachSpeedStateFieldDiff)
{
    F2Fingerprint a = Base();
    F2Fingerprint b = a;
    b.speedTier = SpeedTier::Fast;
    ExpectOnly(a, b, "speedTier");

    b = a;
    b.pauseSource = PauseSource::Modal;
    ExpectOnly(a, b, "pauseSource");

    b = a;
    b.modalDepth = 2;
    ExpectOnly(a, b, "modalDepth");

    b = a;
    b.preModalSpeedTier = SpeedTier::Fastest;
    ExpectOnly(a, b, "preModalSpeedTier");
}

TEST(F2Fingerprint, MultipleDiffsSummarised)
{
    F2Fingerprint a = Base();
    F2Fingerprint b = a;
    b.dayCounter = 1;
    b.modalDepth = 1;
    b.streams[1].drawCount = 9;
    const F2CompareResult r = F2Fingerprint::Compare(a, b);
    EXPECT_EQ(r.differingFields.size(), 3u);
    const std::string s = r.Summary();
    EXPECT_EQ(s.rfind("3 fields differ: ", 0), 0u) << s;
    EXPECT_NE(s.find("stream A1 drawCount"), std::string::npos);
    EXPECT_NE(s.find("dayCounter"), std::string::npos);
    EXPECT_NE(s.find("modalDepth"), std::string::npos);
}

TEST(F2Fingerprint, CaptureReadsStateAndSeeds)
{
    Streams s;
    GameSpeedState state;
    state.dayCounter = 42;
    state.speedTier = SpeedTier::Fast;
    state.pauseSource = PauseSource::None;
    state.modalDepth = 3;
    state.preModalSpeedTier = SpeedTier::Slow;
    const F2Fingerprint fp = F2Fingerprint::Capture(s.ptrs, state);
    EXPECT_EQ(fp.dayCounter, 42u);
    EXPECT_EQ(fp.speedTier, SpeedTier::Fast);
    EXPECT_EQ(fp.pauseSource, PauseSource::None);
    EXPECT_EQ(fp.modalDepth, 3);
    EXPECT_EQ(fp.preModalSpeedTier, SpeedTier::Slow);
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
    {
        EXPECT_EQ(fp.streams[i].seed, static_cast<uint32_t>((i + 1) * 10000));
        EXPECT_EQ(fp.streams[i].drawCount, 0u);
    }
}

TEST(F2Fingerprint, DrawingFromOneStreamChangesOnlyItsDrawCount)
{
    Streams s;
    const GameSpeedState state;
    const F2Fingerprint before = F2Fingerprint::Capture(s.ptrs, state);
    s.ptrs[3]->RandomInt();
    const F2Fingerprint after = F2Fingerprint::Capture(s.ptrs, state);
    ExpectOnly(before, after, std::string("stream ") + RandomStreamModule::kStreamNames[3] + " drawCount");
}

TEST(F2Fingerprint, CaptureIsRepeatable)
{
    Streams s;
    const GameSpeedState state;
    s.ptrs[0]->RandomUnit();
    const F2Fingerprint a = F2Fingerprint::Capture(s.ptrs, state);
    const F2Fingerprint b = F2Fingerprint::Capture(s.ptrs, state);
    EXPECT_TRUE(a == b);
}

TEST(F2Fingerprint, NullStreamCapturedAsZeros)
{
    Streams s;
    s.ptrs[2] = nullptr;
    const F2Fingerprint fp = F2Fingerprint::Capture(s.ptrs, GameSpeedState{});
    EXPECT_EQ(fp.streams[2].seed, 0u);
    EXPECT_EQ(fp.streams[2].drawCount, 0u);
}

// ---- CompareRestored (C5a): streams + dayCounter vs saved, speed state vs paused defaults ----

namespace {
F2Fingerprint SavedLike()
{
    F2Fingerprint fp = Base();
    fp.dayCounter = 120;
    fp.speedTier = SpeedTier::Fastest;     // the saved speed state is NOT restored
    fp.pauseSource = PauseSource::None;
    fp.preModalSpeedTier = SpeedTier::Fast;
    for (int i = 0; i < kF2FingerprintStreamCount; ++i)
        fp.streams[i].drawCount = 40 + i;
    return fp;
}
F2Fingerprint LoadedLike(const F2Fingerprint& saved)
{
    F2Fingerprint fp = saved;
    fp.speedTier = SpeedTier::Paused;
    fp.pauseSource = PauseSource::Player;
    fp.modalDepth = 0;
    return fp;
}
}

TEST(F2FingerprintRestored, MatchesWhenStreamsAndDayRestoredAndSpeedIsPausedDefault)
{
    const F2Fingerprint saved = SavedLike();
    EXPECT_TRUE(F2Fingerprint::CompareRestored(saved, LoadedLike(saved)).IsIdentical());
}

TEST(F2FingerprintRestored, StreamDrawCountDifferenceIsNamed)
{
    const F2Fingerprint saved = SavedLike();
    F2Fingerprint loaded = LoadedLike(saved);
    loaded.streams[2].drawCount += 1;
    const F2CompareResult r = F2Fingerprint::CompareRestored(saved, loaded);
    ASSERT_EQ(r.differingFields.size(), 1u) << r.Summary();
    EXPECT_EQ(r.differingFields[0], std::string("stream ") + RandomStreamModule::kStreamNames[2] + " drawCount");
}

TEST(F2FingerprintRestored, StreamSeedAndDayCounterDifferencesAreNamed)
{
    const F2Fingerprint saved = SavedLike();
    F2Fingerprint loaded = LoadedLike(saved);
    loaded.streams[0].seed += 1;
    loaded.dayCounter += 1;
    const F2CompareResult r = F2Fingerprint::CompareRestored(saved, loaded);
    ASSERT_EQ(r.differingFields.size(), 2u) << r.Summary();
    EXPECT_EQ(r.differingFields[0], std::string("stream ") + RandomStreamModule::kStreamNames[0] + " seed");
    EXPECT_EQ(r.differingFields[1], "dayCounter");
}

TEST(F2FingerprintRestored, NonDefaultLoadedSpeedStateIsReported)
{
    const F2Fingerprint saved = SavedLike();
    F2Fingerprint loaded = LoadedLike(saved);
    loaded.speedTier = SpeedTier::Normal;
    loaded.pauseSource = PauseSource::None;
    loaded.modalDepth = 1;
    const F2CompareResult r = F2Fingerprint::CompareRestored(saved, loaded);
    ASSERT_EQ(r.differingFields.size(), 3u) << r.Summary();
    EXPECT_EQ(r.differingFields[0], "speedTier");
    EXPECT_EQ(r.differingFields[1], "pauseSource");
    EXPECT_EQ(r.differingFields[2], "modalDepth");
}

TEST(F2FingerprintRestored, SavedSpeedStateDoesNotMatterOnlyLoadedDoes)
{
    F2Fingerprint saved = SavedLike();
    saved.speedTier = SpeedTier::Paused;   // even a saved Paused/None... loaded must be paused defaults
    saved.pauseSource = PauseSource::Player;
    EXPECT_TRUE(F2Fingerprint::CompareRestored(saved, LoadedLike(saved)).IsIdentical());
    F2Fingerprint loaded = LoadedLike(saved);
    loaded.speedTier = saved.speedTier = SpeedTier::Fast;
    EXPECT_FALSE(F2Fingerprint::CompareRestored(saved, loaded).IsIdentical());
}

TEST(F2FingerprintRestored, PreModalSpeedTierMustBeRestored)
{
    const F2Fingerprint saved = SavedLike();
    F2Fingerprint loaded = LoadedLike(saved);
    loaded.preModalSpeedTier = SpeedTier::Normal;
    const F2CompareResult r = F2Fingerprint::CompareRestored(saved, loaded);
    ASSERT_EQ(r.differingFields.size(), 1u) << r.Summary();
    EXPECT_EQ(r.differingFields[0], "preModalSpeedTier");
}
