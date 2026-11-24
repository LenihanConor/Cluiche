#include <gtest/gtest.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <string>

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
using Dia::Core::StringCRC;
using Dia::API::ValueParam;

namespace
{
	const ValueInfo* FindCmd(const ValueSchemaRegistry& r, const char* key)
	{
		return r.Find(StringCRC("dia.command"), StringCRC(key));
	}
	const ValueInfo* FindMetric(const ValueSchemaRegistry& r, const char* key)
	{
		return r.Find(StringCRC("dia.metric"), StringCRC(key));
	}
}

TEST(DiaValueSchemaCodeSide, DescribedMetricMapsToValueInfo)
{
	FakeCodeSideSource src;
	src.AddMetric("cow.ecology.herbivores_total", "Herbivores", "Total herbivores alive", "animals");
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindMetric(reg, "cow.ecology.herbivores_total");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mFor.AsCStr(), "dia.metric");
	EXPECT_STREQ(v->mKey.AsCStr(), "cow.ecology.herbivores_total");
	EXPECT_EQ(v->mKind, ValueKind::Metric);
	EXPECT_EQ(v->mType, ValueType::Float);
	EXPECT_STREQ(v->mLabel.AsCStr(), "Herbivores");
	EXPECT_STREQ(v->mDescription.AsCStr(), "Total herbivores alive");
	EXPECT_STREQ(v->mUnit.AsCStr(), "animals");
	EXPECT_EQ(rep.mMetricsCollected, 1u);
	EXPECT_EQ(rep.mMetricsDescribed, 1u);
	EXPECT_EQ(rep.mMetricsUndescribed, 0u);
	EXPECT_EQ(rep.mUndescribedMetrics.Size(), 0u);
}

TEST(DiaValueSchemaCodeSide, MetricWithoutInfoUsesNameAsLabelAndIsUndescribed)
{
	FakeCodeSideSource src;
	src.AddMetric("cow.ecology.foo");
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindMetric(reg, "cow.ecology.foo");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mLabel.AsCStr(), "cow.ecology.foo");
	EXPECT_STREQ(v->mDescription.AsCStr(), "");
	ASSERT_EQ(rep.mUndescribedMetrics.Size(), 1u);
	EXPECT_STREQ(rep.mUndescribedMetrics[0].mName.AsCStr(), "cow.ecology.foo");
	EXPECT_STREQ(rep.mUndescribedMetrics[0].mOwner.AsCStr(), "cow");
	EXPECT_EQ(rep.mMetricsUndescribed, 1u);
	EXPECT_EQ(rep.mMetricsDescribed, 0u);
}

TEST(DiaValueSchemaCodeSide, MetricWithEmptyDescriptionIsUndescribed)
{
	FakeCodeSideSource src;
	src.AddMetric("plain", "Plain", "", "s");
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindMetric(reg, "plain");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mLabel.AsCStr(), "Plain");
	EXPECT_STREQ(v->mUnit.AsCStr(), "s");
	ASSERT_EQ(rep.mUndescribedMetrics.Size(), 1u);
	EXPECT_STREQ(rep.mUndescribedMetrics[0].mOwner.AsCStr(), "plain");	// no dot: whole name
}

TEST(DiaValueSchemaCodeSide, CommandArgMapping)
{
	static const ValueParam params[] = {
		{ "rate", "float", true, "Spawn rate", "per_s", true, 0.5f, 9.5f, nullptr },
		{ "count", "int", false, "How many" },
		{ "flag", "bool", false, "A flag" },
		{ "mode", "enum", true, "Mode", nullptr, false, 0.0f, 0.0f, "weather|ecology" },
		{ "note", "string", false, "A note" },
	};
	FakeCodeSideSource src;
	src.AddCommand("spawn", "CoW", "Spawns", params, 5);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* rate = FindCmd(reg, "spawn.rate");
	ASSERT_NE(rate, nullptr);
	EXPECT_STREQ(rate->mFor.AsCStr(), "dia.command");
	EXPECT_EQ(rate->mKind, ValueKind::Arg);
	EXPECT_EQ(rate->mType, ValueType::Float);
	EXPECT_STREQ(rate->mLabel.AsCStr(), "rate");
	EXPECT_STREQ(rate->mDescription.AsCStr(), "Spawn rate");
	EXPECT_STREQ(rate->mUnit.AsCStr(), "per_s");
	EXPECT_TRUE(rate->mHasMin); EXPECT_TRUE(rate->mHasMax);
	EXPECT_DOUBLE_EQ(rate->mMin, 0.5); EXPECT_DOUBLE_EQ(rate->mMax, 9.5);
	EXPECT_TRUE(rate->mRequired);

	const ValueInfo* count = FindCmd(reg, "spawn.count");
	ASSERT_NE(count, nullptr);
	EXPECT_EQ(count->mType, ValueType::Int);
	EXPECT_FALSE(count->mRequired);
	EXPECT_FALSE(count->mHasMin);
	EXPECT_EQ(FindCmd(reg, "spawn.flag")->mType, ValueType::Bool);
	EXPECT_EQ(FindCmd(reg, "spawn.note")->mType, ValueType::String);

	const ValueInfo* mode = FindCmd(reg, "spawn.mode");
	ASSERT_NE(mode, nullptr);
	EXPECT_EQ(mode->mType, ValueType::Enum);
	ASSERT_EQ(mode->mEnumValues.Size(), 2u);
	EXPECT_STREQ(mode->mEnumValues[0].mId.AsCStr(), "weather");
	EXPECT_STREQ(mode->mEnumValues[0].mLabel.AsCStr(), "weather");
	EXPECT_STREQ(mode->mEnumValues[0].mDescription.AsCStr(), "");
	EXPECT_STREQ(mode->mEnumValues[1].mId.AsCStr(), "ecology");

	EXPECT_EQ(rep.mArgsCollected, 5u);
	EXPECT_EQ(rep.mArgsDescribed, 5u);
	EXPECT_EQ(rep.mArgsUndescribed, 0u);
	EXPECT_EQ(rep.mWarningCount, 0u);
}

TEST(DiaValueSchemaCodeSide, UnknownTypeBecomesStringWithOneWarningPerCommand)
{
	static const ValueParam params[] = {
		{ "a", "vec3", false, "bad a" },
		{ "b", "quaternion", false, "bad b" },
		{ "c", nullptr, false, "no type" },
	};
	static const ValueParam params2[] = { { "z", "colour", false, "bad z" } };
	FakeCodeSideSource src;
	src.AddCommand("bad", "X", "d", params, 3);
	src.AddCommand("bad2", "X", "d", params2, 1);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	EXPECT_EQ(FindCmd(reg, "bad.a")->mType, ValueType::String);
	EXPECT_EQ(FindCmd(reg, "bad.b")->mType, ValueType::String);
	EXPECT_EQ(FindCmd(reg, "bad.c")->mType, ValueType::String);
	EXPECT_EQ(FindCmd(reg, "bad2.z")->mType, ValueType::String);
	EXPECT_EQ(rep.mWarningCount, 2u);
}

TEST(DiaValueSchemaCodeSide, NoArgsCommandHasEntryAndIsNotUndescribed)
{
	FakeCodeSideSource src;
	src.AddCommand("reset", "Core", "Resets", nullptr, 0, true, "debug", "reset now");
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindCmd(reg, "reset");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mKind, ValueKind::Command);
	EXPECT_STREQ(v->mLabel.AsCStr(), "reset");
	EXPECT_STREQ(v->mDescription.AsCStr(), "Resets");
	EXPECT_EQ(v->mRelated.Size(), 0u);
	EXPECT_EQ(rep.mCommandsCollected, 1u);
	EXPECT_EQ(rep.mUndescribedCommands.Size(), 0u);
}

TEST(DiaValueSchemaCodeSide, CommandEntryCarriesOwnerCategoryExample)
{
	FakeCodeSideSource src;
	src.AddCommand("reset", "Core", "Resets", nullptr, 0, true, "debug", "reset now");
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindCmd(reg, "reset");
	ASSERT_NE(v, nullptr);
	EXPECT_EQ(v->mUi["owner"].asString(), "Core");
	EXPECT_EQ(v->mUi["category"].asString(), "debug");
	EXPECT_EQ(v->mUi["example"].asString(), "reset now");
}

TEST(DiaValueSchemaCodeSide, CommandWithNeitherParamsNorNoArgsIsUndescribed)
{
	FakeCodeSideSource src;
	src.AddCommand("legacy", "Old", "Legacy command", nullptr, 0, false);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	EXPECT_NE(FindCmd(reg, "legacy"), nullptr);
	ASSERT_EQ(rep.mUndescribedCommands.Size(), 1u);
	EXPECT_STREQ(rep.mUndescribedCommands[0].mName.AsCStr(), "legacy");
	EXPECT_STREQ(rep.mUndescribedCommands[0].mOwner.AsCStr(), "Old");
}

TEST(DiaValueSchemaCodeSide, ParamWithoutDescriptionIsUndescribed)
{
	static const ValueParam params[] = {
		{ "good", "int", false, "ok" },
		{ "silent", "int", false, nullptr },
		{ "empty", "int", false, "" },
	};
	FakeCodeSideSource src;
	src.AddCommand("cmd", "Own", "d", params, 3);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	ASSERT_EQ(rep.mUndescribedParams.Size(), 2u);
	EXPECT_STREQ(rep.mUndescribedParams[0].mName.AsCStr(), "cmd.silent");
	EXPECT_STREQ(rep.mUndescribedParams[1].mName.AsCStr(), "cmd.empty");
	EXPECT_STREQ(rep.mUndescribedParams[0].mOwner.AsCStr(), "Own");
	EXPECT_EQ(rep.mArgsDescribed, 1u);
	EXPECT_EQ(rep.mArgsUndescribed, 2u);
}

TEST(DiaValueSchemaCodeSide, ArgKeysListedInOrderOnCommandEntry)
{
	static const ValueParam params[] = {
		{ "zeta", "int", false, "z" }, { "alpha", "int", false, "a" }, { "mid", "int", false, "m" },
	};
	FakeCodeSideSource src;
	src.AddCommand("order", "O", "d", params, 3);
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindCmd(reg, "order");
	ASSERT_NE(v, nullptr);
	ASSERT_EQ(v->mRelated.Size(), 3u);
	EXPECT_STREQ(v->mRelated[0].AsCStr(), "order.zeta");
	EXPECT_STREQ(v->mRelated[1].AsCStr(), "order.alpha");
	EXPECT_STREQ(v->mRelated[2].AsCStr(), "order.mid");
}

TEST(DiaValueSchemaCodeSide, JsonCommandsAreIncludedViaSource)
{
	// The source reports plain and JSON commands through the same listing; both reach the registry.
	static const ValueParam params[] = { { "p", "string", false, "param" } };
	FakeCodeSideSource src;
	src.AddCommand("plain.cmd", "A", "d", params, 1);
	src.AddCommand("json.cmd", "B", "d", params, 1);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);
	EXPECT_NE(FindCmd(reg, "plain.cmd"), nullptr);
	EXPECT_NE(FindCmd(reg, "json.cmd.p"), nullptr);
	EXPECT_EQ(rep.mCommandsCollected, 2u);
}

TEST(DiaValueSchemaCodeSide, SameNameFirstWinsAndDuplicatesCounted)
{
	static const ValueParam first[] = { { "a", "int", false, "first" } };
	static const ValueParam second[] = { { "b", "int", false, "second" } };
	FakeCodeSideSource src;
	src.AddCommand("dup", "One", "first command", first, 1);
	src.AddCommand("dup", "Two", "second command", second, 1);
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);

	const ValueInfo* v = FindCmd(reg, "dup");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ(v->mDescription.AsCStr(), "first command");
	EXPECT_NE(FindCmd(reg, "dup.a"), nullptr);
	EXPECT_EQ(FindCmd(reg, "dup.b"), nullptr);
	EXPECT_EQ(rep.mCommandsCollected, 1u);
	EXPECT_EQ(rep.mDuplicateCount, 1u);
	EXPECT_EQ(rep.mRejectedCount, 0u);
}

TEST(DiaValueSchemaCodeSide, RegistryRejectionIsCountedNotFatal)
{
	FakeCodeSideSource src;
	src.AddMetric("m.one", "One", "d", "u");
	ValueSchemaRegistry reg;
	reg.Publish();
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);
	EXPECT_EQ(rep.mMetricsCollected, 0u);
	EXPECT_EQ(rep.mRejectedCount, 1u);
	EXPECT_EQ(reg.Count(), 0u);
}

TEST(DiaValueSchemaCodeSide, OversizedDescriptionRejectedNotTruncated)
{
	std::string longDesc(ValueInfo::kMaxDescriptionLength + 1, 'x');
	FakeCodeSideSource src;
	src.AddMetric("big", "Big", longDesc.c_str(), "u");
	src.AddMetric("ok", "Ok", "fine", "u");
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);
	EXPECT_EQ(FindMetric(reg, "big"), nullptr);
	EXPECT_NE(FindMetric(reg, "ok"), nullptr);
	EXPECT_EQ(rep.mRejectedCount, 1u);
	EXPECT_EQ(rep.mMetricsCollected, 1u);
}

TEST(DiaValueSchemaCodeSide, RecordsSourceGenerations)
{
	FakeCodeSideSource src;
	src.mMetricGen = 7;
	src.mCommandGen = 12;
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(src, reg);
	EXPECT_EQ(rep.mMetricGeneration, 7u);
	EXPECT_EQ(rep.mCommandGeneration, 12u);
}

TEST(DiaValueSchemaCodeSide, LiveSourceCollectsRealRegistries)
{
	using namespace Dia::Observation::Metric;
	static const MetricInfo info = { "Live Gauge", "A live test gauge", "u" };
	MetricRegistry::Instance().RegisterGauge(StringCRC("vsc_live_test.codeside.gauge"), info);

	static const ValueParam params[] = { { "level", "int", true, "Level to set", nullptr, true, 1.0f, 5.0f, nullptr } };
	Dia::API::CommandInfo ci;
	ci.name = StringCRC("vsc_live_test_cmd");
	ci.description = "Live test command";
	ci.category = StringCRC("test");
	ci.owner = "VscLiveTest";
	ci.version = "1.0.0";
	ci.example = "vsc_live_test_cmd --level=2";
	ci.callback = [](const Dia::API::CommandArgs&) { return 0; };
	ci.params = params;
	ci.paramCount = 1;
	const bool wasInitialized = Dia::API::IsInitialized();
	if (!wasInitialized) { Dia::API::Initialize(); }
	const unsigned int genBefore = Dia::API::Generation();
	const bool registered = Dia::API::RegisterCommand(ci);

	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	const ValueInfo* m = FindMetric(reg, "vsc_live_test.codeside.gauge");
	ASSERT_NE(m, nullptr);
	EXPECT_STREQ(m->mLabel.AsCStr(), "Live Gauge");
	EXPECT_STREQ(m->mDescription.AsCStr(), "A live test gauge");
	EXPECT_EQ(rep.mMetricGeneration, MetricRegistry::Instance().Generation());
	EXPECT_EQ(rep.mCommandGeneration, Dia::API::Generation());

	if (registered)
	{
		EXPECT_GT(Dia::API::Generation(), genBefore);
		const ValueInfo* c = FindCmd(reg, "vsc_live_test_cmd");
		ASSERT_NE(c, nullptr);
		EXPECT_EQ(c->mUi["owner"].asString(), "VscLiveTest");
		const ValueInfo* a = FindCmd(reg, "vsc_live_test_cmd.level");
		ASSERT_NE(a, nullptr);
		EXPECT_EQ(a->mType, ValueType::Int);
		EXPECT_TRUE(a->mRequired);
		Dia::API::UnregisterCommand(StringCRC("vsc_live_test_cmd"));
	}
	else
	{
		ADD_FAILURE() << "RegisterCommand failed";
	}
	if (!wasInitialized) { Dia::API::Shutdown(); }
}
