// Tests for CoW::F2DragonStub, the C6 band-resolver fixture: the resolver runs once per load, live reads
// never re-resolve, and only a band_schema_version bump plus an explicit reload re-resolves.
#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <Modules/F2DragonStub.h>

using namespace CoW;

namespace {
F2DragonRecord MakeRecord()
{
    F2DragonRecord r;
    r.dragonId = 7;
    r.bandSchemaVersion = 1;
    return r;
}
constexpr F2DragonAttr kAttrs[kF2DragonAttrCount] = {
    F2DragonAttr::Health, F2DragonAttr::Strength, F2DragonAttr::Agility, F2DragonAttr::Fertility };
}

TEST(F2DragonStub, NoResolveBeforeLoad)
{
    F2DragonStub stub;
    EXPECT_EQ(stub.GetResolverInvocations(), 0u);
}

TEST(F2DragonStub, ResolverRunsOnceOnLoad)
{
    F2DragonStub stub;
    stub.Load(MakeRecord());
    EXPECT_EQ(stub.GetResolverInvocations(), 1u);
}

TEST(F2DragonStub, ResolvedFloatsFallInsideBandRange)
{
    F2DragonStub stub;
    const F2DragonRecord rec = MakeRecord();
    stub.Load(rec);
    for (int a = 0; a < kF2DragonAttrCount; ++a)
    {
        float lo = 0.0f, hi = 0.0f;
        ASSERT_TRUE(stub.GetBandRange(kAttrs[a], rec.bands[a], lo, hi)) << a;
        const float v = stub.Read(kAttrs[a]);
        EXPECT_GE(v, lo) << F2DragonStub::AttrName(kAttrs[a]);
        EXPECT_LE(v, hi) << F2DragonStub::AttrName(kAttrs[a]);
    }
    // Distinct bands resolve to distinct floats: strength HIGH is above health BASELINE.
    EXPECT_GT(stub.Read(F2DragonAttr::Strength), stub.Read(F2DragonAttr::Health));
    EXPECT_LT(stub.Read(F2DragonAttr::Fertility), stub.Read(F2DragonAttr::Health));
}

TEST(F2DragonStub, RepeatedReadsDoNotReResolve)
{
    F2DragonStub stub;
    stub.Load(MakeRecord());
    const float first = stub.Read(F2DragonAttr::Strength);
    for (int i = 0; i < 20; ++i)
        for (F2DragonAttr a : kAttrs)
            (void)stub.Read(a);
    EXPECT_EQ(stub.Read(F2DragonAttr::Strength), first);
    EXPECT_EQ(stub.GetResolverInvocations(), 1u);
}

TEST(F2DragonStub, MutatingBandsWithoutVersionBumpLeavesFloatsAndCountUnchanged)
{
    F2DragonStub stub;
    stub.Load(MakeRecord());
    float before[kF2DragonAttrCount];
    for (int a = 0; a < kF2DragonAttrCount; ++a)
        before[a] = stub.Read(kAttrs[a]);

    for (int a = 0; a < kF2DragonAttrCount; ++a)
        stub.MutableRecord().bands[a] = F2Band::Apex;

    for (int a = 0; a < kF2DragonAttrCount; ++a)
        EXPECT_EQ(stub.Read(kAttrs[a]), before[a]) << a;
    EXPECT_FALSE(stub.VersionChanged());
    EXPECT_FALSE(stub.ReloadIfVersionChanged());
    EXPECT_EQ(stub.GetResolverInvocations(), 1u);
    EXPECT_EQ(stub.Read(F2DragonAttr::Health), before[0]);
}

TEST(F2DragonStub, VersionBumpPlusReloadReResolvesAndChangesFloats)
{
    F2DragonStub stub;
    stub.Load(MakeRecord());
    const float before = stub.Read(F2DragonAttr::Health);

    stub.MutableRecord().bands[0] = F2Band::Apex;
    stub.MutableRecord().bandSchemaVersion = 2;
    EXPECT_TRUE(stub.VersionChanged());
    EXPECT_EQ(stub.Read(F2DragonAttr::Health), before) << "the bump alone does not re-resolve";
    EXPECT_TRUE(stub.ReloadIfVersionChanged());

    EXPECT_EQ(stub.GetResolverInvocations(), 2u);
    EXPECT_NE(stub.Read(F2DragonAttr::Health), before);
    EXPECT_FALSE(stub.VersionChanged());
    EXPECT_FALSE(stub.ReloadIfVersionChanged());
    EXPECT_EQ(stub.GetResolverInvocations(), 2u);
}

TEST(F2DragonStub, ReResolveOnReadFaultHookCountsEveryRead)
{
    F2DragonStub stub;
    stub.SetReResolveOnRead(true);
    stub.Load(MakeRecord());
    (void)stub.Read(F2DragonAttr::Health);
    (void)stub.Read(F2DragonAttr::Health);
    EXPECT_EQ(stub.GetResolverInvocations(), 3u);
}
