#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <DiaCore/CRC/StringCRC.h>
#include <string.h>

// Feature 1 task 6: ValueSchemaService thin wrappers over a published registry (AC 2).

using namespace Dia::ValueSchema;
using Dia::ValueSchema::Testing::MakeValueInfo;
using Dia::Core::StringCRC;

namespace
{
	void Fill(ValueSchemaRegistry& reg)
	{
		reg.Register(MakeValueInfo("eco.params", "grazing.rate"));
		reg.Register(MakeValueInfo("eco.params", "growth.rate"));
		reg.Register(MakeValueInfo("eco.fields", "biome"));
		reg.Register(MakeValueInfo("eco.events", "fire_started", ValueKind::Event));
		reg.Publish();
	}
}

TEST(DiaValueSchemaService, FindByFullKeyAndByForKey)
{
	ValueSchemaRegistry reg;
	Fill(reg);
	ValueSchemaService svc(reg);

	const ValueInfo* a = svc.Find(StringCRC("eco.params/grazing.rate"));
	ASSERT_NE(a, nullptr);
	EXPECT_STREQ(a->mKey.AsCStr(), "grazing.rate");
	EXPECT_EQ(svc.Find(StringCRC("eco.params"), StringCRC("grazing.rate")), a);
	EXPECT_EQ(svc.Find(StringCRC("eco.params/nope")), nullptr);
	EXPECT_EQ(svc.Find(StringCRC("eco.params"), StringCRC("nope")), nullptr);
}

TEST(DiaValueSchemaService, ForFormatInOrderFindEventAndCount)
{
	ValueSchemaRegistry reg;
	Fill(reg);
	ValueSchemaService svc(reg);

	const ValueInfoSpan span = svc.ForFormat(StringCRC("eco.params"));
	ASSERT_EQ(span.mCount, 2u);
	EXPECT_STREQ(span.mItems[0]->mKey.AsCStr(), "grazing.rate");
	EXPECT_STREQ(span.mItems[1]->mKey.AsCStr(), "growth.rate");
	EXPECT_EQ(svc.ForFormat(StringCRC("unknown")).mCount, 0u);

	const ValueInfo* e = svc.FindEvent(StringCRC("fire_started"));
	ASSERT_NE(e, nullptr);
	EXPECT_STREQ(e->mFor.AsCStr(), "eco.events");
	EXPECT_EQ(svc.FindEvent(StringCRC("grazing.rate")), nullptr);

	EXPECT_EQ(svc.Count(), 4u);
}
