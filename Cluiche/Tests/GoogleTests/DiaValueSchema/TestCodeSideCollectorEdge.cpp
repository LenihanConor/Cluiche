#include <gtest/gtest.h>
#include <DiaValueSchema/CodeSideCollector.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/Testing/FakeCodeSideSource.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <deque>
#include <limits>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

// Exhaustive edge tests for CodeSideCollector (Feature 4 hardening).
//
// PINNED behaviour:
//  - A rejected/duplicate command entry skips its args entirely (they are neither collected nor counted).
//  - A param with a duplicate name is counted as a duplicate (first wins); the command lists the key once.
//  - LiveCodeSideSource reads metric and command names through StringCRC::AsChar(), which truncates at 63 chars;
//    metrics longer than that collapse onto their 63-char prefix (the second one is a duplicate).
//  - JSON commands have no example (ui.example is "").
//  - The live command listing is capped at 256 plain commands by the DiaAPI registry itself.
//  - hasRange is copied as given (min > max, NaN and inf are not validated).

using namespace Dia::ValueSchema;
using namespace Dia::ValueSchema::Testing;
using Dia::Core::StringCRC;
using Dia::API::ValueParam;

namespace
{
	const ValueInfo* FindCmd(const ValueSchemaRegistry& r, const std::string& key)
	{
		return r.Find(StringCRC("dia.command"), StringCRC(key.c_str()));
	}
	const ValueInfo* FindMetric(const ValueSchemaRegistry& r, const std::string& key)
	{
		return r.Find(StringCRC("dia.metric"), StringCRC(key.c_str()));
	}

	// Keeps ValueParam text alive (deque: stable addresses).
	struct Params
	{
		std::deque<std::string>	mStrings;
		std::vector<ValueParam>	mList;

		const char* Keep(const char* s)
		{
			if (s == nullptr) { return nullptr; }
			mStrings.push_back(s);
			return mStrings.back().c_str();
		}
		Params& Add(const char* name, const char* type = "int", const char* description = "d", const char* unit = nullptr,
			const char* enumValues = nullptr, bool required = false)
		{
			ValueParam p;
			p.name = Keep(name);
			p.type = Keep(type);
			p.required = required;
			p.description = Keep(description);
			p.unit = Keep(unit);
			p.enumValues = Keep(enumValues);
			mList.push_back(p);
			return *this;
		}
		Params& AddRange(const char* name, float mn, float mx, const char* type = "float")
		{
			Add(name, type);
			mList.back().hasRange = true; mList.back().min = mn; mList.back().max = mx;
			return *this;
		}
		const ValueParam* Data() const { return mList.empty() ? nullptr : mList.data(); }
		unsigned Count() const { return static_cast<unsigned>(mList.size()); }
	};

	// Emits arbitrary (null-heavy) metric/command records.
	class RawSource : public ICodeSideSource
	{
	public:
		std::vector<CodeSideMetric>		mMetrics;
		std::vector<CodeSideCommand>	mCommands;
		void ForEachMetric(MetricVisitor v, void* user) override { for (const auto& m : mMetrics) { v(m, user); } }
		void ForEachCommand(CommandVisitor v, void* user) override { for (const auto& c : mCommands) { v(c, user); } }
		unsigned int MetricGeneration() override { return 0; }
		unsigned int CommandGeneration() override { return 0; }
	};

	struct Collected
	{
		ValueSchemaRegistry	mReg;
		CodeSideReport		mRep;
		explicit Collected(ICodeSideSource& src) { mRep = CodeSideCollector().Collect(src, mReg); }
	};

	std::string Rep(char c, size_t n) { return std::string(n, c); }

	// One metric through the fake source; true when it was registered.
	bool MetricAccepted(const std::string& name, const std::string& label, const std::string& desc, const std::string& unit)
	{
		FakeCodeSideSource src;
		src.AddMetric(name.c_str(), label.c_str(), desc.c_str(), unit.c_str());
		Collected c(src);
		const bool ok = c.mRep.mMetricsCollected == 1;
		EXPECT_EQ(ok ? 0u : 1u, c.mRep.mRejectedCount);
		EXPECT_EQ(ok, FindMetric(c.mReg, name) != nullptr);
		return ok;
	}
}

// ---------------------------------------------------------------------------------------------------------
// Capacity boundaries
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdge, MetricNameCapacityBoundary)
{
	EXPECT_TRUE(MetricAccepted(Rep('n', 127), "L", "d", "u"));
	EXPECT_FALSE(MetricAccepted(Rep('n', 128), "L", "d", "u"));
}

TEST(DiaValueSchemaCodeSideEdge, MetricLabelCapacityBoundary)
{
	EXPECT_TRUE(MetricAccepted("m", Rep('l', 127), "d", "u"));
	EXPECT_FALSE(MetricAccepted("m", Rep('l', 128), "d", "u"));
}

TEST(DiaValueSchemaCodeSideEdge, MetricUnitCapacityBoundary)
{
	EXPECT_TRUE(MetricAccepted("m", "L", "d", Rep('u', 31)));
	EXPECT_FALSE(MetricAccepted("m", "L", "d", Rep('u', 32)));
}

TEST(DiaValueSchemaCodeSideEdge, MetricDescriptionCapacityBoundary)
{
	EXPECT_TRUE(MetricAccepted("m", "L", Rep('d', 1023), "u"));
	EXPECT_FALSE(MetricAccepted("m", "L", Rep('d', 1024), "u"));
}

TEST(DiaValueSchemaCodeSideEdge, MetricWithoutInfoAtMaxNameUsesNameAsLabel)
{
	FakeCodeSideSource src;
	const std::string name = Rep('q', 127);
	src.AddMetric(name.c_str());
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mMetricsCollected);
	EXPECT_STREQ(name.c_str(), FindMetric(c.mReg, name)->mLabel.AsCStr());
}

TEST(DiaValueSchemaCodeSideEdge, OversizedMetricDoesNotStopLaterMetrics)
{
	FakeCodeSideSource src;
	src.AddMetric(Rep('n', 128).c_str(), "L", "d", "u");
	src.AddMetric("good", "L", "d", "u");
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mMetricsCollected);
	EXPECT_EQ(1u, c.mRep.mRejectedCount);
	EXPECT_NE(nullptr, FindMetric(c.mReg, "good"));
}

TEST(DiaValueSchemaCodeSideEdge, CommandNameCapacityBoundary)
{
	for (size_t n : { 63u, 64u })
	{
		FakeCodeSideSource src;
		const std::string name = Rep('c', n);
		src.AddCommand(name.c_str(), "O", "d", nullptr, 0, true);
		Collected c(src);
		EXPECT_EQ(n == 63 ? 1u : 0u, c.mRep.mCommandsCollected) << n;
		EXPECT_EQ(n == 63 ? 0u : 1u, c.mRep.mRejectedCount) << n;
		EXPECT_EQ(n == 63, FindCmd(c.mReg, name) != nullptr) << n;
	}
}

TEST(DiaValueSchemaCodeSideEdge, CommandDescriptionCapacityBoundary)
{
	for (size_t n : { 1023u, 1024u })
	{
		FakeCodeSideSource src;
		const std::string desc = Rep('d', n);
		src.AddCommand("cmd", "O", desc.c_str(), nullptr, 0, true);
		Collected c(src);
		EXPECT_EQ(n == 1023 ? 1u : 0u, c.mRep.mCommandsCollected) << n;
		EXPECT_EQ(n == 1023 ? 0u : 1u, c.mRep.mRejectedCount) << n;
	}
}

TEST(DiaValueSchemaCodeSideEdge, OversizedCommandRejectsItsArgsToo)
{
	Params p;
	p.Add("a").Add("b");
	FakeCodeSideSource src;
	src.AddCommand(Rep('c', 64).c_str(), "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(0u, c.mRep.mCommandsCollected);
	EXPECT_EQ(0u, c.mRep.mArgsCollected);
	EXPECT_EQ(1u, c.mRep.mRejectedCount);
	EXPECT_EQ(0u, c.mReg.Count());
}

TEST(DiaValueSchemaCodeSideEdge, ArgKeyCapacityBoundary)
{
	// key = command + "." + arg: 10 + 1 + 116 = 127 fits, 128 does not.
	for (size_t argLen : { 116u, 117u })
	{
		Params p;
		const std::string arg = Rep('a', argLen);
		p.Add(arg.c_str());
		FakeCodeSideSource src;
		src.AddCommand("cmdten0000", "O", "d", p.Data(), p.Count());
		Collected c(src);
		const bool fits = argLen == 116;
		EXPECT_EQ(1u, c.mRep.mCommandsCollected);
		EXPECT_EQ(fits ? 1u : 0u, c.mRep.mArgsCollected) << argLen;
		EXPECT_EQ(fits ? 0u : 1u, c.mRep.mRejectedCount) << argLen;
		const ValueInfo* cmd = FindCmd(c.mReg, "cmdten0000");
		ASSERT_NE(cmd, nullptr);
		EXPECT_EQ(fits ? 1u : 0u, cmd->mRelated.Size()) << argLen;	// the rejected arg is not listed
	}
}

TEST(DiaValueSchemaCodeSideEdge, ArgWithMaxLengthNameAndMaxCommandName)
{
	Params p;
	const std::string arg = Rep('a', 63);
	p.Add(arg.c_str());
	FakeCodeSideSource src;
	const std::string cmd = Rep('c', 63);
	src.AddCommand(cmd.c_str(), "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mArgsCollected);
	const ValueInfo* a = FindCmd(c.mReg, cmd + "." + arg);
	ASSERT_NE(a, nullptr);
	EXPECT_STREQ(arg.c_str(), a->mLabel.AsCStr());
}

TEST(DiaValueSchemaCodeSideEdge, ArgDescriptionAndUnitCapacityBoundary)
{
	struct Case { size_t mDesc; size_t mUnit; bool mOk; };
	for (const Case k : { Case{ 1023, 31, true }, Case{ 1024, 31, false }, Case{ 1023, 32, false } })
	{
		Params p;
		const std::string d = Rep('d', k.mDesc), u = Rep('u', k.mUnit);
		p.Add("a", "int", d.c_str(), u.c_str());
		FakeCodeSideSource src;
		src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
		Collected c(src);
		EXPECT_EQ(k.mOk ? 1u : 0u, c.mRep.mArgsCollected) << k.mDesc << "/" << k.mUnit;
		EXPECT_EQ(k.mOk ? 0u : 1u, c.mRep.mRejectedCount) << k.mDesc << "/" << k.mUnit;
		EXPECT_EQ(1u, c.mRep.mCommandsCollected);
	}
}

TEST(DiaValueSchemaCodeSideEdge, EnumIdCapacityBoundary)
{
	for (size_t n : { 63u, 64u })
	{
		Params p;
		const std::string id = Rep('e', n);
		p.Add("mode", "enum", "d", nullptr, (std::string("ok|") + id).c_str());
		FakeCodeSideSource src;
		src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
		Collected c(src);
		EXPECT_EQ(1u, c.mRep.mCommandsCollected) << n;			// the command always stays
		if (n == 63)
		{
			ASSERT_EQ(1u, c.mRep.mArgsCollected);
			const ValueInfo* a = FindCmd(c.mReg, "cmd.mode");
			ASSERT_EQ(2u, a->mEnumValues.Size());
			EXPECT_STREQ(id.c_str(), a->mEnumValues[1].mId.AsCStr());
		}
		else
		{
			EXPECT_EQ(0u, c.mRep.mArgsCollected);
			EXPECT_EQ(1u, c.mRep.mRejectedCount);
			EXPECT_EQ(nullptr, FindCmd(c.mReg, "cmd.mode"));
			EXPECT_EQ(0u, FindCmd(c.mReg, "cmd")->mRelated.Size());
		}
	}
}

TEST(DiaValueSchemaCodeSideEdge, OversizedOwnerCategoryExampleAreNotCapped)
{
	FakeCodeSideSource src;
	const std::string big = Rep('x', 5000);
	src.AddCommand("cmd", big.c_str(), "d", nullptr, 0, true, big.c_str(), big.c_str());
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mCommandsCollected);
	const ValueInfo* v = FindCmd(c.mReg, "cmd");
	EXPECT_EQ(5000u, v->mUi["owner"].asString().size());
	EXPECT_EQ(5000u, v->mUi["category"].asString().size());
	EXPECT_EQ(5000u, v->mUi["example"].asString().size());
}

// ---------------------------------------------------------------------------------------------------------
// Null / empty inputs
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdge, NullAndEmptyMetricNamesAreRejected)
{
	RawSource src;
	CodeSideMetric nullName; nullName.mName = nullptr;
	CodeSideMetric emptyName; emptyName.mName = "";
	CodeSideMetric ok; ok.mName = "ok";
	src.mMetrics = { nullName, emptyName, ok };
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mMetricsCollected);
	EXPECT_EQ(2u, c.mRep.mRejectedCount);
	EXPECT_EQ(1u, c.mReg.Count());
}

TEST(DiaValueSchemaCodeSideEdge, MetricWithInfoButNullTextFields)
{
	RawSource src;
	CodeSideMetric m;
	m.mName = "m.null";
	m.mHasInfo = true;			// label, description, unit all null
	src.mMetrics = { m };
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mMetricsCollected);
	const ValueInfo* v = FindMetric(c.mReg, "m.null");
	EXPECT_STREQ("m.null", v->mLabel.AsCStr());
	EXPECT_STREQ("", v->mDescription.AsCStr());
	EXPECT_STREQ("", v->mUnit.AsCStr());
	EXPECT_EQ(1u, c.mRep.mMetricsUndescribed);
}

TEST(DiaValueSchemaCodeSideEdge, MetricWithInfoEmptyLabelFallsBackToName)
{
	FakeCodeSideSource src;
	src.AddMetric("m.x", "", "has description", "");
	Collected c(src);
	EXPECT_STREQ("m.x", FindMetric(c.mReg, "m.x")->mLabel.AsCStr());
	EXPECT_EQ(1u, c.mRep.mMetricsDescribed);
}

TEST(DiaValueSchemaCodeSideEdge, MetricInfoIgnoredWhenHasInfoFalse)
{
	RawSource src;
	CodeSideMetric m;
	m.mName = "m.ignored";
	m.mHasInfo = false;
	m.mLabel = "Label"; m.mDescription = "Desc"; m.mUnit = "u";
	src.mMetrics = { m };
	Collected c(src);
	const ValueInfo* v = FindMetric(c.mReg, "m.ignored");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ("m.ignored", v->mLabel.AsCStr());
	EXPECT_STREQ("", v->mDescription.AsCStr());
	EXPECT_EQ(1u, c.mRep.mMetricsUndescribed);
}

TEST(DiaValueSchemaCodeSideEdge, NullAndEmptyCommandNamesAreRejected)
{
	RawSource src;
	CodeSideCommand a; a.mName = nullptr; a.mNoArgs = true;
	CodeSideCommand b; b.mName = ""; b.mNoArgs = true;
	CodeSideCommand ok; ok.mName = "ok"; ok.mNoArgs = true;
	src.mCommands = { a, b, ok };
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mCommandsCollected);
	EXPECT_EQ(2u, c.mRep.mRejectedCount);
}

TEST(DiaValueSchemaCodeSideEdge, CommandWithNullOwnerCategoryExampleDescription)
{
	RawSource src;
	CodeSideCommand cmd;
	cmd.mName = "cmd";			// description, category, owner, example all null
	src.mCommands = { cmd };
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mCommandsCollected);
	const ValueInfo* v = FindCmd(c.mReg, "cmd");
	EXPECT_STREQ("", v->mDescription.AsCStr());
	EXPECT_EQ("", v->mUi["owner"].asString());
	EXPECT_EQ("", v->mUi["category"].asString());
	EXPECT_EQ("", v->mUi["example"].asString());
	ASSERT_EQ(1u, c.mRep.mUndescribedCommands.Size());
	EXPECT_STREQ("", c.mRep.mUndescribedCommands[0].mOwner.AsCStr());
}

TEST(DiaValueSchemaCodeSideEdge, ParamsNullWithPositiveCountIsTreatedAsNoParams)
{
	RawSource src;
	CodeSideCommand cmd;
	cmd.mName = "cmd"; cmd.mDescription = "d"; cmd.mOwner = "O";
	cmd.mParams = nullptr; cmd.mParamCount = 5;
	src.mCommands = { cmd };
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mCommandsCollected);
	EXPECT_EQ(0u, c.mRep.mArgsCollected);
	EXPECT_EQ(0u, FindCmd(c.mReg, "cmd")->mRelated.Size());
	EXPECT_EQ(1u, c.mRep.mUndescribedCommands.Size());	// neither params nor noArgs
}

TEST(DiaValueSchemaCodeSideEdge, ParamsPresentWithCountZeroIsTreatedAsNoParams)
{
	Params p;
	p.Add("a");
	RawSource src;
	CodeSideCommand cmd;
	cmd.mName = "cmd"; cmd.mDescription = "d"; cmd.mOwner = "O";
	cmd.mParams = p.Data(); cmd.mParamCount = 0;
	src.mCommands = { cmd };
	Collected c(src);
	EXPECT_EQ(0u, c.mRep.mArgsCollected);
	EXPECT_EQ(1u, c.mRep.mUndescribedCommands.Size());
	cmd.mNoArgs = true;
	RawSource src2;
	src2.mCommands = { cmd };
	Collected c2(src2);
	EXPECT_EQ(0u, c2.mRep.mUndescribedCommands.Size());
}

TEST(DiaValueSchemaCodeSideEdge, NoArgsFlagWithParamsStillCollectsArgs)
{
	Params p;
	p.Add("a");
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count(), true);
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mArgsCollected);
	EXPECT_EQ(0u, c.mRep.mUndescribedCommands.Size());
}

TEST(DiaValueSchemaCodeSideEdge, TwoHundredParams)
{
	Params p;
	for (int i = 0; i < 200; ++i) { p.Add(("p" + std::to_string(i)).c_str(), "int", ("desc" + std::to_string(i)).c_str()); }
	FakeCodeSideSource src;
	src.AddCommand("big", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(200u, c.mRep.mArgsCollected);
	EXPECT_EQ(200u, c.mRep.mArgsDescribed);
	EXPECT_EQ(0u, c.mRep.mRejectedCount);
	EXPECT_EQ(201u, c.mReg.Count());
	const ValueInfo* cmd = FindCmd(c.mReg, "big");
	ASSERT_EQ(200u, cmd->mRelated.Size());
	for (int i = 0; i < 200; ++i) { EXPECT_STREQ(("big.p" + std::to_string(i)).c_str(), cmd->mRelated[i].AsCStr()) << i; }
}

TEST(DiaValueSchemaCodeSideEdge, ParamWithEmptyOrNullNameIsSkippedAndCounted)
{
	Params p;
	p.Add("first").Add("").Add("last");
	p.mList.push_back(ValueParam());				// null name, null everything
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(2u, c.mRep.mArgsCollected);
	EXPECT_EQ(2u, c.mRep.mRejectedCount);
	const ValueInfo* cmd = FindCmd(c.mReg, "cmd");
	ASSERT_EQ(2u, cmd->mRelated.Size());
	EXPECT_STREQ("cmd.first", cmd->mRelated[0].AsCStr());
	EXPECT_STREQ("cmd.last", cmd->mRelated[1].AsCStr());
}

TEST(DiaValueSchemaCodeSideEdge, NullDescriptionUnitEnumOnParamAreEmpty)
{
	Params p;
	p.Add("a", "int", nullptr, nullptr, nullptr);
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	const ValueInfo* a = FindCmd(c.mReg, "cmd.a");
	ASSERT_NE(a, nullptr);
	EXPECT_STREQ("", a->mDescription.AsCStr());
	EXPECT_STREQ("", a->mUnit.AsCStr());
	EXPECT_EQ(1u, c.mRep.mArgsUndescribed);
}

// ---------------------------------------------------------------------------------------------------------
// Duplicate param names
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdge, DuplicateParamNamesAreListedOnceOnTheCommand)
{
	Params p;
	p.Add("x", "int", "first").Add("y", "int", "y").Add("x", "float", "second");
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	const ValueInfo* cmd = FindCmd(c.mReg, "cmd");
	ASSERT_NE(cmd, nullptr);
	ASSERT_EQ(2u, cmd->mRelated.Size());
	EXPECT_STREQ("cmd.x", cmd->mRelated[0].AsCStr());
	EXPECT_STREQ("cmd.y", cmd->mRelated[1].AsCStr());
	EXPECT_EQ(2u, c.mRep.mArgsCollected);
	EXPECT_EQ(1u, c.mRep.mDuplicateCount);
	EXPECT_EQ(0u, c.mRep.mRejectedCount);
	EXPECT_EQ(ValueType::Int, FindCmd(c.mReg, "cmd.x")->mType);		// first wins
	EXPECT_STREQ("first", FindCmd(c.mReg, "cmd.x")->mDescription.AsCStr());
}

TEST(DiaValueSchemaCodeSideEdge, ManyDuplicateParamsListEachKeyOnce)
{
	Params p;
	for (int i = 0; i < 9; ++i) { p.Add("same", "int", "d"); }
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(1u, FindCmd(c.mReg, "cmd")->mRelated.Size());
	EXPECT_EQ(1u, c.mRep.mArgsCollected);
	EXPECT_EQ(8u, c.mRep.mDuplicateCount);
}

TEST(DiaValueSchemaCodeSideEdge, DuplicateParamUndescribedListedOnce)
{
	Params p;
	p.Add("x", "int", nullptr).Add("x", "int", nullptr);
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mArgsUndescribed);
	EXPECT_EQ(1u, c.mRep.mUndescribedParams.Size());
}

// ---------------------------------------------------------------------------------------------------------
// Enum parsing
// ---------------------------------------------------------------------------------------------------------

namespace
{
	std::vector<std::string> EnumIds(const char* spec, const char* type = "enum")
	{
		Params p;
		p.Add("e", type, "d", nullptr, spec);
		FakeCodeSideSource src;
		src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
		Collected c(src);
		std::vector<std::string> out;
		const ValueInfo* a = FindCmd(c.mReg, "cmd.e");
		EXPECT_NE(a, nullptr) << spec;
		if (a == nullptr) { return out; }
		for (unsigned i = 0; i < a->mEnumValues.Size(); ++i)
		{
			out.push_back(a->mEnumValues[i].mId.AsCStr());
			EXPECT_STREQ(a->mEnumValues[i].mId.AsCStr(), a->mEnumValues[i].mLabel.AsCStr());
		}
		return out;
	}
}

TEST(DiaValueSchemaCodeSideEdge, EnumParsingSeparators)
{
	typedef std::vector<std::string> V;
	EXPECT_EQ(V(), EnumIds(""));
	EXPECT_EQ(V(), EnumIds("|"));
	EXPECT_EQ(V(), EnumIds("|||"));
	EXPECT_EQ((V{ "a", "b" }), EnumIds("a||b"));
	EXPECT_EQ((V{ "a", "b" }), EnumIds("a|b|"));
	EXPECT_EQ((V{ "a", "b" }), EnumIds("|a|b"));
	EXPECT_EQ((V{ "a" }), EnumIds("a"));
	EXPECT_EQ((V{ " a ", "b" }), EnumIds(" a |b"));			// whitespace is part of the id
	EXPECT_EQ((V{ "a", "a" }), EnumIds("a|a"));				// duplicates are kept
}

TEST(DiaValueSchemaCodeSideEdge, EnumWithoutEnumValuesHasNoValues)
{
	Params p;
	p.Add("e", "enum", "d", nullptr, nullptr);
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	const ValueInfo* a = FindCmd(c.mReg, "cmd.e");
	ASSERT_NE(a, nullptr);
	EXPECT_EQ(ValueType::Enum, a->mType);
	EXPECT_EQ(0u, a->mEnumValues.Size());
}

TEST(DiaValueSchemaCodeSideEdge, EnumValuesOnNonEnumAreIgnored)
{
	for (const char* type : { "int", "float", "bool", "string" })
	{
		EXPECT_TRUE(EnumIds("a|b|c", type).empty()) << type;
	}
}

TEST(DiaValueSchemaCodeSideEdge, HundredEnumValues)
{
	std::string spec;
	std::vector<std::string> expect;
	for (int i = 0; i < 100; ++i)
	{
		expect.push_back("value" + std::to_string(i));
		spec += (i ? "|" : "") + expect.back();
	}
	EXPECT_EQ(expect, EnumIds(spec.c_str()));
}

TEST(DiaValueSchemaCodeSideEdge, SixtyFourCharEnumValueRejectsArgButKeepsCommand)
{
	Params p;
	p.Add("good", "int").Add("bad", "enum", "d", nullptr, Rep('v', 64).c_str());
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mCommandsCollected);
	EXPECT_EQ(1u, c.mRep.mArgsCollected);
	EXPECT_EQ(1u, c.mRep.mRejectedCount);
	const ValueInfo* cmd = FindCmd(c.mReg, "cmd");
	ASSERT_EQ(1u, cmd->mRelated.Size());
	EXPECT_STREQ("cmd.good", cmd->mRelated[0].AsCStr());
}

// ---------------------------------------------------------------------------------------------------------
// Types, range, required, unit
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdge, TypeNamesAreCaseSensitive)
{
	Params p;
	p.Add("a", "INT").Add("b", "Float").Add("c", " int").Add("d", "");
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	for (const char* k : { "cmd.a", "cmd.b", "cmd.c", "cmd.d" }) { EXPECT_EQ(ValueType::String, FindCmd(c.mReg, k)->mType) << k; }
	EXPECT_EQ(1u, c.mRep.mWarningCount);	// one warning per command, not per param
}

TEST(DiaValueSchemaCodeSideEdge, RangeIsCopiedAsGivenWithoutValidation)
{
	const float inf = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Params p;
	p.AddRange("inverted", 10.0f, -10.0f).AddRange("nan", nan, 1.0f).AddRange("inf", -inf, inf).AddRange("onstring", 1.0f, 2.0f, "string");
	p.Add("norange");
	p.mList.back().min = 5.0f; p.mList.back().max = 6.0f;		// hasRange false: min/max ignored
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	const ValueInfo* inv = FindCmd(c.mReg, "cmd.inverted");
	ASSERT_NE(inv, nullptr);
	EXPECT_TRUE(inv->mHasMin && inv->mHasMax);
	EXPECT_EQ(10.0, inv->mMin);
	EXPECT_EQ(-10.0, inv->mMax);
	const ValueInfo* n = FindCmd(c.mReg, "cmd.nan");
	ASSERT_NE(n, nullptr);
	EXPECT_TRUE(std::isnan(n->mMin));
	EXPECT_EQ(1.0, n->mMax);
	const ValueInfo* i = FindCmd(c.mReg, "cmd.inf");
	ASSERT_NE(i, nullptr);
	EXPECT_TRUE(std::isinf(i->mMin) && i->mMin < 0);
	EXPECT_TRUE(std::isinf(i->mMax) && i->mMax > 0);
	EXPECT_TRUE(FindCmd(c.mReg, "cmd.onstring")->mHasMin);
	EXPECT_FALSE(FindCmd(c.mReg, "cmd.norange")->mHasMin);
	EXPECT_FALSE(FindCmd(c.mReg, "cmd.norange")->mHasMax);
	EXPECT_EQ(5u, c.mRep.mArgsCollected);
}

TEST(DiaValueSchemaCodeSideEdge, RequiredAndUnitAreCopied)
{
	Params p;
	p.Add("req", "float", "d", "m/s", nullptr, true).Add("opt", "float", "d", "", nullptr, false);
	FakeCodeSideSource src;
	src.AddCommand("cmd", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_TRUE(FindCmd(c.mReg, "cmd.req")->mRequired);
	EXPECT_STREQ("m/s", FindCmd(c.mReg, "cmd.req")->mUnit.AsCStr());
	EXPECT_FALSE(FindCmd(c.mReg, "cmd.opt")->mRequired);
	EXPECT_STREQ("", FindCmd(c.mReg, "cmd.opt")->mUnit.AsCStr());
}

// ---------------------------------------------------------------------------------------------------------
// Owner derivation for undescribed metrics
// ---------------------------------------------------------------------------------------------------------

namespace
{
	std::string OwnerOf(const std::string& metricName)
	{
		FakeCodeSideSource src;
		src.AddMetric(metricName.c_str());
		Collected c(src);
		EXPECT_EQ(1u, c.mRep.mUndescribedMetrics.Size()) << metricName;
		if (c.mRep.mUndescribedMetrics.Size() != 1) { return "<none>"; }
		EXPECT_STREQ(metricName.c_str(), c.mRep.mUndescribedMetrics[0].mName.AsCStr());
		return c.mRep.mUndescribedMetrics[0].mOwner.AsCStr();
	}
}

TEST(DiaValueSchemaCodeSideEdge, OwnerIsPrefixBeforeFirstDot)
{
	EXPECT_EQ("cow", OwnerOf("cow.ecology.herbivores"));
	EXPECT_EQ("a", OwnerOf("a.b"));
	EXPECT_EQ("plain", OwnerOf("plain"));
	EXPECT_EQ("x", OwnerOf("x."));
}

TEST(DiaValueSchemaCodeSideEdge, OwnerOfLeadingDotIsEmpty)
{
	EXPECT_EQ("", OwnerOf(".foo"));
	EXPECT_EQ("", OwnerOf("."));
}

TEST(DiaValueSchemaCodeSideEdge, OwnerIsTruncatedToSixtyThreeChars)
{
	EXPECT_EQ(Rep('o', 63), OwnerOf(Rep('o', 100)));
	EXPECT_EQ(Rep('o', 63), OwnerOf(Rep('o', 63)));
	EXPECT_EQ(Rep('o', 63), OwnerOf(Rep('o', 64)));
	EXPECT_EQ(Rep('o', 63), OwnerOf(Rep('o', 90) + ".tail"));
	EXPECT_EQ(Rep('o', 62), OwnerOf(Rep('o', 62) + ".tail"));
	EXPECT_EQ(Rep('o', 63), OwnerOf(Rep('o', 127)));
}

TEST(DiaValueSchemaCodeSideEdge, UndescribedCommandOwnerLongerThanBufferIsTruncated)
{
	FakeCodeSideSource src;
	src.AddCommand("cmd", Rep('O', 100).c_str(), "d", nullptr, 0, false);
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mUndescribedCommands.Size());
	EXPECT_EQ(Rep('O', 63), std::string(c.mRep.mUndescribedCommands[0].mOwner.AsCStr()));
}

// ---------------------------------------------------------------------------------------------------------
// printf-format injection
// ---------------------------------------------------------------------------------------------------------

namespace
{
	const char* const kEvil[] = { "%", "100%", "%s", "%d", "%n", "%s%s%s%s%s%n", "%%", "%x %p %5000d", "a%", "%1$s", "%.3s", "%-5d%c" };
}

TEST(DiaValueSchemaCodeSideInjection, MetricTextIsStoredVerbatim)
{
	for (const char* evil : kEvil)
	{
		const std::string name = std::string("m.") + evil;
		FakeCodeSideSource src;
		src.AddMetric(name.c_str(), (std::string("L") + evil).c_str(), (std::string("D") + evil).c_str(), evil);
		Collected c(src);
		ASSERT_EQ(1u, c.mRep.mMetricsCollected) << evil;
		const ValueInfo* v = FindMetric(c.mReg, name);
		ASSERT_NE(v, nullptr) << evil;
		EXPECT_STREQ(name.c_str(), v->mKey.AsCStr()) << evil;
		EXPECT_EQ(std::string("L") + evil, v->mLabel.AsCStr()) << evil;
		EXPECT_EQ(std::string("D") + evil, v->mDescription.AsCStr()) << evil;
		EXPECT_STREQ(evil, v->mUnit.AsCStr()) << evil;
	}
}

TEST(DiaValueSchemaCodeSideInjection, MetricNameAsLabelWhenNoInfo)
{
	for (const char* evil : kEvil)
	{
		FakeCodeSideSource src;
		src.AddMetric(evil);
		Collected c(src);
		ASSERT_EQ(1u, c.mRep.mMetricsCollected) << evil;
		EXPECT_STREQ(evil, FindMetric(c.mReg, evil)->mLabel.AsCStr()) << evil;
		ASSERT_EQ(1u, c.mRep.mUndescribedMetrics.Size());
		EXPECT_STREQ(evil, c.mRep.mUndescribedMetrics[0].mName.AsCStr()) << evil;
	}
}

TEST(DiaValueSchemaCodeSideInjection, UndescribedMetricOwnerKeepsFormatTokens)
{
	FakeCodeSideSource src;
	src.AddMetric("%s%n.rest");
	src.AddMetric("100%.rest");
	Collected c(src);
	ASSERT_EQ(2u, c.mRep.mUndescribedMetrics.Size());
	EXPECT_STREQ("%s%n.rest", c.mRep.mUndescribedMetrics[0].mName.AsCStr());
	EXPECT_STREQ("%s%n", c.mRep.mUndescribedMetrics[0].mOwner.AsCStr());
	EXPECT_STREQ("100%", c.mRep.mUndescribedMetrics[1].mOwner.AsCStr());
}

TEST(DiaValueSchemaCodeSideInjection, CommandAndArgTextIsStoredVerbatim)
{
	for (const char* evil : kEvil)
	{
		Params p;
		const std::string argName = std::string("p") + evil;
		p.Add(argName.c_str(), "enum", (std::string("D") + evil).c_str(), evil,
			(std::string("a") + evil + "|b" + evil).c_str());
		const std::string cmdName = std::string("c") + evil;
		FakeCodeSideSource src;
		src.AddCommand(cmdName.c_str(), (std::string("O") + evil).c_str(), (std::string("CD") + evil).c_str(), p.Data(), p.Count(),
			false, (std::string("K") + evil).c_str(), (std::string("E") + evil).c_str());
		Collected c(src);
		ASSERT_EQ(1u, c.mRep.mCommandsCollected) << evil;
		ASSERT_EQ(1u, c.mRep.mArgsCollected) << evil;

		const ValueInfo* cmd = FindCmd(c.mReg, cmdName);
		ASSERT_NE(cmd, nullptr) << evil;
		EXPECT_STREQ(cmdName.c_str(), cmd->mKey.AsCStr()) << evil;
		EXPECT_STREQ(cmdName.c_str(), cmd->mLabel.AsCStr()) << evil;
		EXPECT_EQ(std::string("CD") + evil, cmd->mDescription.AsCStr()) << evil;
		EXPECT_EQ(std::string("O") + evil, cmd->mUi["owner"].asString()) << evil;
		EXPECT_EQ(std::string("K") + evil, cmd->mUi["category"].asString()) << evil;
		EXPECT_EQ(std::string("E") + evil, cmd->mUi["example"].asString()) << evil;
		ASSERT_EQ(1u, cmd->mRelated.Size());
		EXPECT_EQ(cmdName + "." + argName, cmd->mRelated[0].AsCStr()) << evil;

		const ValueInfo* arg = FindCmd(c.mReg, cmdName + "." + argName);
		ASSERT_NE(arg, nullptr) << evil;
		EXPECT_EQ(cmdName + "." + argName, arg->mKey.AsCStr()) << evil;
		EXPECT_EQ(argName, arg->mLabel.AsCStr()) << evil;
		EXPECT_EQ(std::string("D") + evil, arg->mDescription.AsCStr()) << evil;
		EXPECT_STREQ(evil, arg->mUnit.AsCStr()) << evil;
		ASSERT_EQ(2u, arg->mEnumValues.Size()) << evil;
		EXPECT_EQ(std::string("a") + evil, arg->mEnumValues[0].mId.AsCStr()) << evil;
		EXPECT_EQ(std::string("a") + evil, arg->mEnumValues[0].mLabel.AsCStr()) << evil;
		EXPECT_EQ(std::string("b") + evil, arg->mEnumValues[1].mId.AsCStr()) << evil;
	}
}

TEST(DiaValueSchemaCodeSideInjection, UndescribedCommandAndParamNamesKeepFormatTokens)
{
	Params p;
	p.Add("%s%n", "int", nullptr);
	FakeCodeSideSource src;
	src.AddCommand("c%d", "o%s", "d", nullptr, 0, false);
	src.AddCommand("with%", "100%", "d", p.Data(), p.Count());
	Collected c(src);
	ASSERT_EQ(1u, c.mRep.mUndescribedCommands.Size());
	EXPECT_STREQ("c%d", c.mRep.mUndescribedCommands[0].mName.AsCStr());
	EXPECT_STREQ("o%s", c.mRep.mUndescribedCommands[0].mOwner.AsCStr());
	ASSERT_EQ(1u, c.mRep.mUndescribedParams.Size());
	EXPECT_STREQ("with%.%s%n", c.mRep.mUndescribedParams[0].mName.AsCStr());
	EXPECT_STREQ("100%", c.mRep.mUndescribedParams[0].mOwner.AsCStr());
}

TEST(DiaValueSchemaCodeSideInjection, SourceFileMarkersAreIntact)
{
	FakeCodeSideSource src;
	src.AddMetric("m%s", "L", "D", "u");
	Collected c(src);
	EXPECT_STREQ("code:MetricRegistry", FindMetric(c.mReg, "m%s")->mSourceFile.AsCStr());
}

// ---------------------------------------------------------------------------------------------------------
// Registry interaction
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdge, PublishedRegistryRejectsEveryEntryWithoutCrashing)
{
	Params p;
	p.Add("a").Add("b").Add("c");
	FakeCodeSideSource src;
	src.AddMetric("m.one", "L", "d", "u");
	src.AddMetric("m.two");
	src.AddCommand("cmd1", "O", "d", p.Data(), p.Count());
	src.AddCommand("cmd2", "O", "d", nullptr, 0, true);
	ValueSchemaRegistry reg;
	reg.Publish();
	const CodeSideReport rep = CodeSideCollector().Collect(src, reg);
	EXPECT_EQ(4u, rep.mRejectedCount);			// 2 metrics + 2 commands; args are never attempted
	EXPECT_EQ(0u, rep.mMetricsCollected);
	EXPECT_EQ(0u, rep.mCommandsCollected);
	EXPECT_EQ(0u, rep.mArgsCollected);
	EXPECT_EQ(0u, rep.mDuplicateCount);
	EXPECT_EQ(0u, rep.mUndescribedMetrics.Size());
	EXPECT_EQ(0u, reg.Count());
}

TEST(DiaValueSchemaCodeSideEdge, CollectTwiceMakesEverythingDuplicate)
{
	Params p;
	p.Add("a", "int", "d").Add("b", "int", "d");
	FakeCodeSideSource src;
	src.AddMetric("m.one", "L", "d", "u");
	src.AddMetric("m.two");
	src.AddCommand("cmd1", "O", "d", p.Data(), p.Count());
	src.AddCommand("cmd2", "O", "d", nullptr, 0, true);
	ValueSchemaRegistry reg;
	const CodeSideReport first = CodeSideCollector().Collect(src, reg);
	const unsigned countAfterFirst = reg.Count();
	EXPECT_EQ(2u + 2u + 2u, countAfterFirst);
	EXPECT_EQ(0u, first.mDuplicateCount);

	const CodeSideReport second = CodeSideCollector().Collect(src, reg);
	EXPECT_EQ(countAfterFirst, reg.Count());
	EXPECT_EQ(4u, second.mDuplicateCount);		// 2 metrics + 2 commands; args are skipped with their command
	EXPECT_EQ(0u, second.mRejectedCount);
	EXPECT_EQ(0u, second.mMetricsCollected);
	EXPECT_EQ(0u, second.mCommandsCollected);
	EXPECT_EQ(0u, second.mArgsCollected);
	EXPECT_EQ(0u, second.mUndescribedMetrics.Size());
}

TEST(DiaValueSchemaCodeSideEdge, MetricNamedLikeCommandArgCoexists)
{
	Params p;
	p.Add("rate", "float", "d");
	FakeCodeSideSource src;
	src.AddMetric("spawn.rate", "L", "d", "u");
	src.AddCommand("spawn", "O", "d", p.Data(), p.Count());
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mMetricsCollected);
	EXPECT_EQ(1u, c.mRep.mArgsCollected);
	EXPECT_EQ(0u, c.mRep.mDuplicateCount);
	const ValueInfo* m = FindMetric(c.mReg, "spawn.rate");
	const ValueInfo* a = FindCmd(c.mReg, "spawn.rate");
	ASSERT_NE(m, nullptr);
	ASSERT_NE(a, nullptr);
	EXPECT_NE(m, a);
	EXPECT_EQ(ValueKind::Metric, m->mKind);
	EXPECT_EQ(ValueKind::Arg, a->mKind);
	EXPECT_NE(nullptr, c.mReg.Find(StringCRC("dia.metric/spawn.rate")));
	EXPECT_NE(nullptr, c.mReg.Find(StringCRC("dia.command/spawn.rate")));
}

TEST(DiaValueSchemaCodeSideEdge, SameNameMetricAndCommandAreDifferentFormats)
{
	FakeCodeSideSource src;
	src.AddMetric("shared", "L", "d", "u");
	src.AddCommand("shared", "O", "d", nullptr, 0, true);
	Collected c(src);
	EXPECT_EQ(1u, c.mRep.mMetricsCollected);
	EXPECT_EQ(1u, c.mRep.mCommandsCollected);
	EXPECT_EQ(0u, c.mRep.mDuplicateCount);
}

TEST(DiaValueSchemaCodeSideEdge, EmptySourceGivesEmptyReport)
{
	FakeCodeSideSource src;
	Collected c(src);
	EXPECT_EQ(0u, c.mReg.Count());
	EXPECT_EQ(0u, c.mRep.mMetricsCollected + c.mRep.mCommandsCollected + c.mRep.mArgsCollected);
	EXPECT_EQ(0u, c.mRep.mRejectedCount + c.mRep.mDuplicateCount + c.mRep.mWarningCount);
}

// ---------------------------------------------------------------------------------------------------------
// Invariants over random sources
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideEdgeProperty, ReportInvariantsHoldOverRandomSources)
{
	std::mt19937 rng(1234);
	auto chance = [&](unsigned percent) { return rng() % 100 < percent; };
	const char* const types[] = { "int", "float", "bool", "string", "enum", "vec3", "INT", nullptr };

	for (int iter = 0; iter < 150; ++iter)
	{
		std::vector<std::unique_ptr<Params>> keep;
		FakeCodeSideSource src;
		const unsigned metricCount = rng() % 40;
		for (unsigned i = 0; i < metricCount; ++i)
		{
			const std::string name = "m" + std::to_string(rng() % 25) + (chance(30) ? ".sub" : "");
			if (chance(35)) { src.AddMetric(chance(3) ? Rep('z', 130).c_str() : name.c_str()); continue; }
			const std::string desc = chance(40) ? "" : "described";
			src.AddMetric(name.c_str(), chance(30) ? "" : "Label", chance(2) ? Rep('d', 1030).c_str() : desc.c_str(), "u");
		}
		const unsigned cmdCount = rng() % 15;
		for (unsigned i = 0; i < cmdCount; ++i)
		{
			keep.emplace_back(new Params());
			Params& p = *keep.back();
			const unsigned paramCount = rng() % 7;
			for (unsigned k = 0; k < paramCount; ++k)
			{
				const std::string pname = chance(5) ? "" : "p" + std::to_string(rng() % 4);
				const char* type = types[rng() % 8];
				const std::string enumSpec = chance(5) ? Rep('v', 64) : std::string("x|y||z");
				p.Add(pname.c_str(), type, chance(40) ? nullptr : "pd", chance(20) ? "u" : nullptr, enumSpec.c_str());
			}
			const std::string name = "c" + std::to_string(rng() % 12);
			src.AddCommand(name.c_str(), "Own", chance(2) ? Rep('D', 1030).c_str() : "desc", p.Data(), p.Count(), chance(30));
		}

		Collected c(src);
		const CodeSideReport& r = c.mRep;
		ASSERT_EQ(r.mMetricsCollected, r.mMetricsDescribed + r.mMetricsUndescribed) << iter;
		ASSERT_EQ(r.mArgsCollected, r.mArgsDescribed + r.mArgsUndescribed) << iter;
		ASSERT_EQ(r.mMetricsCollected + r.mCommandsCollected + r.mArgsCollected, c.mReg.Count()) << iter;
		ASSERT_EQ(r.mMetricsUndescribed, r.mUndescribedMetrics.Size()) << iter;
		ASSERT_EQ(r.mArgsUndescribed, r.mUndescribedParams.Size()) << iter;
		ASSERT_LE(r.mUndescribedCommands.Size(), r.mCommandsCollected) << iter;
		ASSERT_LE(r.mMetricsCollected, metricCount) << iter;
		ASSERT_LE(r.mCommandsCollected, cmdCount) << iter;
		for (unsigned i = 0; i < r.mUndescribedParams.Size(); ++i)
		{
			ASSERT_NE(nullptr, c.mReg.Find(StringCRC("dia.command"), StringCRC(r.mUndescribedParams[i].mName.AsCStr()))) << iter;
		}
		for (unsigned i = 0; i < r.mUndescribedMetrics.Size(); ++i)
		{
			ASSERT_NE(nullptr, c.mReg.Find(StringCRC("dia.metric"), StringCRC(r.mUndescribedMetrics[i].mName.AsCStr()))) << iter;
		}
		// Every key a command lists is a registered arg of that command, exactly once.
		const auto span = c.mReg.ForFormat(StringCRC("dia.command"));
		for (unsigned i = 0; i < span.mCount; ++i)
		{
			const ValueInfo* v = span.mItems[i];
			if (v->mKind != ValueKind::Command) { continue; }
			std::set<std::string> seen;
			for (unsigned k = 0; k < v->mRelated.Size(); ++k)
			{
				ASSERT_TRUE(seen.insert(v->mRelated[k].AsCStr()).second) << iter << " " << v->mRelated[k].AsCStr();
				ASSERT_NE(nullptr, c.mReg.Find(StringCRC("dia.command"), StringCRC(v->mRelated[k].AsCStr()))) << iter;
			}
		}
	}
}

TEST(DiaValueSchemaCodeSideEdgeProperty, EveryMetricIsCollectedDuplicateOrRejected)
{
	std::mt19937 rng(1234);
	for (int iter = 0; iter < 100; ++iter)
	{
		FakeCodeSideSource src;
		const unsigned n = rng() % 60;
		for (unsigned i = 0; i < n; ++i)
		{
			const std::string name = rng() % 50 == 0 ? Rep('n', 128) : "m" + std::to_string(rng() % 20);
			src.AddMetric(name.c_str(), "L", rng() % 2 ? "d" : "", "u");
		}
		Collected c(src);
		ASSERT_EQ(n, c.mRep.mMetricsCollected + c.mRep.mDuplicateCount + c.mRep.mRejectedCount) << iter;
	}
}

// ---------------------------------------------------------------------------------------------------------
// Stress
// ---------------------------------------------------------------------------------------------------------

TEST(DiaValueSchemaCodeSideStress, TenThousandMetricsAndThousandCommandsWithEightArgs)
{
	Params p;
	for (int a = 0; a < 8; ++a) { p.Add(("arg" + std::to_string(a)).c_str(), "float", "an argument", "u"); }
	FakeCodeSideSource src;
	for (int i = 0; i < 10000; ++i)
	{
		const std::string name = "stress.metric." + std::to_string(i);
		if (i % 2) { src.AddMetric(name.c_str(), "Label", "Description", "u"); } else { src.AddMetric(name.c_str()); }
	}
	for (int i = 0; i < 1000; ++i) { src.AddCommand(("stress-cmd-" + std::to_string(i)).c_str(), "Owner", "Command", p.Data(), p.Count()); }

	const auto start = std::chrono::steady_clock::now();
	Collected c(src);
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

	EXPECT_EQ(10000u, c.mRep.mMetricsCollected);
	EXPECT_EQ(5000u, c.mRep.mMetricsDescribed);
	EXPECT_EQ(5000u, c.mRep.mMetricsUndescribed);
	EXPECT_EQ(1000u, c.mRep.mCommandsCollected);
	EXPECT_EQ(8000u, c.mRep.mArgsCollected);
	EXPECT_EQ(8000u, c.mRep.mArgsDescribed);
	EXPECT_EQ(0u, c.mRep.mRejectedCount + c.mRep.mDuplicateCount);
	EXPECT_EQ(19000u, c.mReg.Count());
	EXPECT_EQ(5000u, c.mRep.mUndescribedMetrics.Size());
	EXPECT_NE(nullptr, FindMetric(c.mReg, "stress.metric.9999"));
	const ValueInfo* last = FindCmd(c.mReg, "stress-cmd-999");
	ASSERT_NE(last, nullptr);
	EXPECT_EQ(8u, last->mRelated.Size());
	EXPECT_NE(nullptr, FindCmd(c.mReg, "stress-cmd-999.arg7"));
	EXPECT_LT(seconds, 60.0);	// generous: guards against quadratic behaviour, not speed
}

// ---------------------------------------------------------------------------------------------------------
// Live sources (global MetricRegistry / DiaAPI command registry)
// ---------------------------------------------------------------------------------------------------------

namespace
{
	struct ApiGuard
	{
		bool mInitialized = false;
		ApiGuard()
		{
			if (!Dia::API::IsInitialized()) { Dia::API::Initialize(); mInitialized = true; }
		}
		~ApiGuard()
		{
			if (mInitialized) { Dia::API::Shutdown(); }
		}
	};

	unsigned PlainCommandCount()
	{
		auto* list = new Dia::Core::Containers::DynamicArrayC<Dia::API::CommandInfo, 256>();
		Dia::API::ListCommandInfos(*list);
		const unsigned n = list->Size();
		delete list;
		return n;
	}

	Dia::API::CommandInfo MakeCommand(const std::string& name)
	{
		Dia::API::CommandInfo ci;
		ci.name = StringCRC(name.c_str());
		ci.description = "live edge test command";
		ci.category = StringCRC("test");
		ci.owner = "VscLiveEdge";
		ci.version = "1.0.0";
		ci.example = "example text";
		ci.callback = [](const Dia::API::CommandArgs&) { return 0; };
		ci.noArgs = true;
		return ci;
	}
}

TEST(DiaValueSchemaCodeSideLive, LongMetricNamesAreTruncatedToSixtyThreeCharsAndCollapse)
{
	using namespace Dia::Observation::Metric;
	static const MetricInfo info = { "Long", "A long-named metric", "u" };
	const std::string prefix = "vsc_live_edge_long_metric_" + Rep('x', 37);		// 26 + 37 = 63 chars
	ASSERT_EQ(63u, prefix.size());
	const std::string nameA = prefix + "_A_unique_suffix_aaaa";
	const std::string nameB = prefix + "_B_unique_suffix_bbbb";
	MetricRegistry::Instance().RegisterGauge(StringCRC(nameA.c_str()), info);
	MetricRegistry::Instance().RegisterGauge(StringCRC(nameB.c_str()), info);

	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);

	// Both metrics are visited under the same 63-char prefix: one entry, the other a duplicate.
	unsigned entriesWithPrefix = 0;
	const auto span = reg.ForFormat(StringCRC("dia.metric"));
	for (unsigned i = 0; i < span.mCount; ++i)
	{
		if (strncmp(span.mItems[i]->mKey.AsCStr(), prefix.c_str(), prefix.size()) == 0)
		{
			++entriesWithPrefix;
			EXPECT_EQ(63u, strlen(span.mItems[i]->mKey.AsCStr()));
		}
	}
	EXPECT_EQ(1u, entriesWithPrefix);
	EXPECT_NE(nullptr, FindMetric(reg, prefix));
	EXPECT_EQ(nullptr, FindMetric(reg, nameA));
	EXPECT_EQ(nullptr, FindMetric(reg, nameB));
	EXPECT_GE(rep.mDuplicateCount, 1u);
}

TEST(DiaValueSchemaCodeSideLive, MetricNameOfExactlySixtyThreeCharsIsNotTruncated)
{
	using namespace Dia::Observation::Metric;
	static const MetricInfo info = { "Exact", "A 63-char metric", "u" };
	const std::string name = "vsc_live_edge_exact_63_" + Rep('y', 40);
	ASSERT_EQ(63u, name.size());
	MetricRegistry::Instance().RegisterGauge(StringCRC(name.c_str()), info);
	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(live, reg);
	const ValueInfo* v = FindMetric(reg, name);
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ("Exact", v->mLabel.AsCStr());
}

TEST(DiaValueSchemaCodeSideLive, MetricConflictingInfoKeepsFirstAndWarnsOncePerName)
{
	using namespace Dia::Observation::Metric;
	static const MetricInfo first = { "First", "first description", "a" };
	static const MetricInfo second = { "Second", "second description", "b" };
	static const MetricInfo third = { "Third", "third description", "c" };
	MetricRegistry& registry = MetricRegistry::Instance();
	const StringCRC name("vsc_live_edge_ac11.conflict");

	registry.RegisterGauge(name, first);
	const unsigned warningsBefore = registry.ConflictWarningCount();
	registry.RegisterGauge(name, second);
	EXPECT_EQ(warningsBefore + 1, registry.ConflictWarningCount());
	registry.RegisterGauge(name, third);		// still the same name: no further warning
	registry.RegisterGauge(name, second);
	EXPECT_EQ(warningsBefore + 1, registry.ConflictWarningCount());

	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	CodeSideCollector().Collect(live, reg);
	const ValueInfo* v = FindMetric(reg, "vsc_live_edge_ac11.conflict");
	ASSERT_NE(v, nullptr);
	EXPECT_STREQ("First", v->mLabel.AsCStr());
	EXPECT_STREQ("first description", v->mDescription.AsCStr());
	EXPECT_STREQ("a", v->mUnit.AsCStr());
}

TEST(DiaValueSchemaCodeSideLive, JsonCommandHasNoExample)
{
	ApiGuard guard;
	static const ValueParam params[] = { { "level", "int", true, "Level" } };
	Dia::API::CommandInfoJson ci;
	ci.name = StringCRC("vsc-live-edge-json");
	ci.description = "live json command";
	ci.category = StringCRC("test");
	ci.owner = "VscLiveEdgeJson";
	ci.callback = [](const Json::Value&) { return Json::Value(Json::objectValue); };
	ci.params = params;
	ci.paramCount = 1;
	ASSERT_TRUE(Dia::API::RegisterCommandJson(ci));

	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	const ValueInfo* c = FindCmd(reg, "vsc-live-edge-json");
	ASSERT_NE(c, nullptr);
	EXPECT_EQ("", c->mUi["example"].asString());			// PIN: JSON commands carry no example
	EXPECT_EQ("VscLiveEdgeJson", c->mUi["owner"].asString());
	EXPECT_EQ("test", c->mUi["category"].asString());
	EXPECT_NE(nullptr, FindCmd(reg, "vsc-live-edge-json.level"));
	EXPECT_GE(rep.mCommandsCollected, 1u);
	Dia::API::UnregisterCommandJson(StringCRC("vsc-live-edge-json"));
}

TEST(DiaValueSchemaCodeSideLive, FullPlainCommandListingOfTwoHundredFiftySixDoesNotOverflow)
{
	ApiGuard guard;
	const unsigned existing = PlainCommandCount();
	ASSERT_LE(existing, 256u);
	const unsigned toRegister = 256u - existing;		// PIN: 256 is the registry limit; a 257th would assert out of bounds

	std::vector<std::string> names;
	for (unsigned i = 0; i < toRegister; ++i)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "vsc-cap-edge-%03u", i);
		names.push_back(buf);
		ASSERT_TRUE(Dia::API::RegisterCommand(MakeCommand(names.back()))) << names.back();
	}
	ASSERT_EQ(256u, PlainCommandCount());

	LiveCodeSideSource live;
	ValueSchemaRegistry reg;
	const CodeSideReport rep = CodeSideCollector().Collect(live, reg);
	unsigned found = 0;
	for (const std::string& n : names) { found += FindCmd(reg, n) != nullptr ? 1u : 0u; }
	EXPECT_EQ(toRegister, found);
	EXPECT_GE(rep.mCommandsCollected, toRegister);
	const ValueInfo* first = names.empty() ? nullptr : FindCmd(reg, names.front());
	if (first != nullptr) { EXPECT_EQ("VscLiveEdge", first->mUi["owner"].asString()); EXPECT_EQ("example text", first->mUi["example"].asString()); }

	for (const std::string& n : names) { Dia::API::UnregisterCommand(StringCRC(n.c_str())); }
	EXPECT_EQ(existing, PlainCommandCount());
}
