#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
using Dia::Core::StringCRC;

TEST(DiaValueSchemaRegistry, FindByFullKey)
{
	ValueSchemaRegistry reg;
	ASSERT_EQ(reg.Register(MakeValueInfo("weather", "rain_rate")), RegisterResult::Ok);
	const ValueInfo* v = reg.Find(StringCRC("weather/rain_rate"));
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mKey.AsCStr(), "rain_rate");
	EXPECT_STREQ(v->mFullKey.AsCStr(), "weather/rain_rate");
}

TEST(DiaValueSchemaRegistry, FindByForAndKeyMatchesFullKey)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("weather", "rain_rate"));
	EXPECT_EQ(reg.Find(StringCRC("weather"), StringCRC("rain_rate")), reg.Find(StringCRC("weather/rain_rate")));
	EXPECT_NE(reg.Find(StringCRC("weather"), StringCRC("rain_rate")), nullptr);
}

TEST(DiaValueSchemaRegistry, UnknownReturnsNull)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("weather", "rain_rate"));
	EXPECT_EQ(reg.Find(StringCRC("weather/nope")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("other"), StringCRC("rain_rate")), nullptr);
	EXPECT_EQ(reg.FindEvent(StringCRC("rain_rate")), nullptr);
}

TEST(DiaValueSchemaRegistry, ForFormatInRegistrationOrder)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("a", "z"));
	reg.Register(MakeValueInfo("b", "x"));
	reg.Register(MakeValueInfo("a", "m"));
	reg.Register(MakeValueInfo("a", "b"));
	ValueInfoSpan span = reg.ForFormat(StringCRC("a"));
	ASSERT_EQ(span.mCount, 3u);
	EXPECT_STREQ(span.mItems[0]->mKey.AsCStr(), "z");
	EXPECT_STREQ(span.mItems[1]->mKey.AsCStr(), "m");
	EXPECT_STREQ(span.mItems[2]->mKey.AsCStr(), "b");
}

TEST(DiaValueSchemaRegistry, ForFormatUnknownIsEmpty)
{
	ValueSchemaRegistry reg;
	ValueInfoSpan span = reg.ForFormat(StringCRC("nothing"));
	EXPECT_EQ(span.mCount, 0u);
}

TEST(DiaValueSchemaRegistry, DuplicateKeyFirstWins)
{
	ValueSchemaRegistry reg;
	ValueInfo first = MakeValueInfo("a", "k", ValueKind::Parameter, "one.diavalues");
	first.mLabel = "first";
	ValueInfo second = MakeValueInfo("a", "k", ValueKind::Parameter, "two.diavalues");
	second.mLabel = "second";
	EXPECT_EQ(reg.Register(first), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(second), RegisterResult::DuplicateKey);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_STREQ(reg.Find(StringCRC("a/k"))->mLabel.AsCStr(), "first");
}

TEST(DiaValueSchemaRegistry, HashCollisionRejected)
{
	ValueSchemaRegistry reg;
	const StringCRC forced("forced-hash");
	EXPECT_EQ(reg.Register(MakeValueInfo("a", "k1"), forced), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeValueInfo("a", "k2"), forced), RegisterResult::HashCollision);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_STREQ(reg.Find(forced)->mKey.AsCStr(), "k1");
	EXPECT_EQ(reg.Find(StringCRC("a"), StringCRC("k2")), nullptr);
}

TEST(DiaValueSchemaRegistry, FindEventAcrossFormats)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("weather", "storm_started", ValueKind::Event));
	reg.Register(MakeValueInfo("weather", "rain_rate"));
	const ValueInfo* e = reg.FindEvent(StringCRC("storm_started"));
	ASSERT_NE(e, nullptr);
	EXPECT_STREQ(e->mFor.AsCStr(), "weather");
	EXPECT_EQ(reg.FindEvent(StringCRC("rain_rate")), nullptr);
}

TEST(DiaValueSchemaRegistry, EventKeyDuplicateInOtherFormatRejected)
{
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Register(MakeValueInfo("weather", "boom", ValueKind::Event)), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeValueInfo("combat", "boom", ValueKind::Event)), RegisterResult::DuplicateEventKey);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_STREQ(reg.FindEvent(StringCRC("boom"))->mFor.AsCStr(), "weather");
	EXPECT_EQ(reg.Find(StringCRC("combat/boom")), nullptr);
}

TEST(DiaValueSchemaRegistry, NonEventSameKeyInOtherFormatAllowed)
{
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Register(MakeValueInfo("weather", "boom", ValueKind::Event)), RegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeValueInfo("combat", "boom", ValueKind::Parameter)), RegisterResult::Ok);
}

TEST(DiaValueSchemaRegistry, RegisterBeforePublishOk)
{
	ValueSchemaRegistry reg;
	EXPECT_FALSE(reg.IsPublished());
	EXPECT_EQ(reg.Register(MakeValueInfo("a", "k")), RegisterResult::Ok);
	reg.Publish();
	EXPECT_TRUE(reg.IsPublished());
}

TEST(DiaValueSchemaRegistry, RegisterAfterPublishRejectedAndUnchanged)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("a", "k"));
	reg.Publish();
	EXPECT_EQ(reg.Register(MakeValueInfo("a", "late")), RegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Count(), 1u);
	EXPECT_EQ(reg.Find(StringCRC("a/late")), nullptr);
	EXPECT_NE(reg.Find(StringCRC("a/k")), nullptr);
}

TEST(DiaValueSchemaRegistry, CountTracksAccepted)
{
	ValueSchemaRegistry reg;
	EXPECT_EQ(reg.Count(), 0u);
	reg.Register(MakeValueInfo("a", "k1"));
	reg.Register(MakeValueInfo("a", "k2"));
	reg.Register(MakeValueInfo("a", "k1"));
	EXPECT_EQ(reg.Count(), 2u);
}

TEST(DiaValueSchemaRegistry, PointersStableAcrossManyRegistrations)
{
	ValueSchemaRegistry reg;
	reg.Register(MakeValueInfo("a", "first"));
	const ValueInfo* first = reg.Find(StringCRC("a/first"));
	char name[32];
	for (int i = 0; i < 2000; ++i)
	{
		snprintf(name, sizeof(name), "k%d", i);
		ASSERT_EQ(reg.Register(MakeValueInfo("a", name)), RegisterResult::Ok);
	}
	EXPECT_EQ(reg.Find(StringCRC("a/first")), first);
	EXPECT_STREQ(first->mKey.AsCStr(), "first");
	EXPECT_EQ(reg.Count(), 2001u);
}

TEST(DiaValueSchemaRegistry, RecordPathSegmentsRoundTrip)
{
	ValueSchemaRegistry reg;
	ValueInfo info = MakeValueInfo("eco", "species[].growth_rate");
	info.mPathSegments.Reserve(3);
	info.mPathSegments.Add(Dia::Core::Containers::String64("species"));
	info.mPathSegments.Add(Dia::Core::Containers::String64("[]"));
	info.mPathSegments.Add(Dia::Core::Containers::String64("growth_rate"));
	ASSERT_EQ(reg.Register(info), RegisterResult::Ok);
	const ValueInfo* v = reg.Find(StringCRC("eco"), StringCRC("species[].growth_rate"));
	ASSERT_NE(v, nullptr);
	ASSERT_EQ(v->mPathSegments.Size(), 3u);
	EXPECT_STREQ(v->mPathSegments[0].AsCStr(), "species");
	EXPECT_STREQ(v->mPathSegments[1].AsCStr(), "[]");
	EXPECT_STREQ(v->mPathSegments[2].AsCStr(), "growth_rate");
}

TEST(DiaValueSchemaRegistry, CapacityConstantsExposed)
{
	EXPECT_GE(ValueInfo::kMaxDescriptionLength, 1000u);
	EXPECT_GT(ValueInfo::kMaxHintLength, 0u);
}
