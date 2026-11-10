#include <gtest/gtest.h>
#include <DiaValueSchema/RampRegistry.h>
#include <DiaValueSchema/RampData.h>

// Feature 5 task 3: RampRegistry rules (Dia wins, first game file wins, immutable after Publish).

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;

namespace
{
	RampData MakeRamp(const char* name, uint32_t first, uint32_t last)
	{
		RampData r;
		r.mName = name;
		r.mId = StringCRC(name);
		r.mKind = RampKind::Continuous;
		r.mStops[0].mT = 0.0f; r.mStops[0].mRgba8 = first;
		r.mStops[1].mT = 1.0f; r.mStops[1].mRgba8 = last;
		r.mStopCount = 2;
		return r;
	}
}

TEST(DiaValueSchemaRampRegistry, FindReturnsRegisteredAndNullForUnknown)
{
	RampRegistry reg;
	EXPECT_EQ(reg.Register(MakeRamp("a.one", 0x000000FF, 0xFFFFFFFF), false, "a.ramps.json"), RampRegisterResult::Ok);
	reg.Publish();
	const RampData* r = reg.Find(StringCRC("a.one"));
	ASSERT_NE(r, nullptr);
	EXPECT_EQ(r->mStops[1].mRgba8, 0xFFFFFFFFu);
	EXPECT_EQ(reg.Find(StringCRC("nope")), nullptr);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaRampRegistry, GameRampClashingWithDiaIsRejectedAndDiaPreserved)
{
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("x", 0x111111FF, 0x222222FF), true, "dia/ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("x", 0xAAAAAAFF, 0xBBBBBBFF), false, "game.ramps.json"), RampRegisterResult::ClashesWithDia);
	reg.Publish();
	const RampData* r = reg.Find(StringCRC("x"));
	ASSERT_NE(r, nullptr);
	EXPECT_EQ(r->mStops[0].mRgba8, 0x111111FFu);
	EXPECT_EQ(r->mStops[1].mRgba8, 0x222222FFu);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaRampRegistry, GameRampsFirstWinsOnDuplicate)
{
	RampRegistry reg;
	ASSERT_EQ(reg.Register(MakeRamp("x", 0x111111FF, 0x222222FF), false, "a.ramps.json"), RampRegisterResult::Ok);
	EXPECT_EQ(reg.Register(MakeRamp("x", 0xAAAAAAFF, 0xBBBBBBFF), false, "b.ramps.json"), RampRegisterResult::DuplicateId);
	reg.Publish();
	EXPECT_EQ(reg.Find(StringCRC("x"))->mStops[0].mRgba8, 0x111111FFu);
	EXPECT_EQ(reg.Count(), 1u);
}

TEST(DiaValueSchemaRampRegistry, RegisterAfterPublishRejected)
{
	RampRegistry reg;
	reg.Publish();
	EXPECT_TRUE(reg.IsPublished());
	EXPECT_EQ(reg.Register(MakeRamp("late", 0, 0), false, "l.ramps.json"), RampRegisterResult::AlreadyPublished);
	EXPECT_EQ(reg.Count(), 0u);
	EXPECT_EQ(reg.Find(StringCRC("late")), nullptr);
}

TEST(DiaValueSchemaRampRegistry, PointersStableAcrossLaterRegisters)
{
	RampRegistry reg;
	reg.Register(MakeRamp("first", 1, 2), false, "f");
	const RampData* p = reg.Find(StringCRC("first"));
	for (int i = 0; i < 200; ++i)
	{
		char name[32];
		snprintf(name, sizeof(name), "filler.%d", i);
		reg.Register(MakeRamp(name, 3, 4), false, "f");
	}
	EXPECT_EQ(reg.Find(StringCRC("first")), p);
	EXPECT_EQ(reg.Count(), 201u);
}
