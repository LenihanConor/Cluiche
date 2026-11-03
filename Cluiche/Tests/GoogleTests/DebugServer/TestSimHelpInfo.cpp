#include <gtest/gtest.h>

#include <DiaDebugServer/SimHelpInfo.h>
#include <DiaDebugServer/SimQueryHost.h>

#include <cstring>
#include <string>
#include <vector>

// Sim-Hosted Command Help, Task 1: SimHelpInfo, limits, SimQueryHost help-carrying registration (AC 1, 2, 8).

using namespace Dia::DebugServer;
using Dia::Core::StringCRC;

namespace
{
	Json::Value Ok(const Json::Value&) { return Json::Value(Json::objectValue); }

	SimHelpInfo Info(const char* description)
	{
		SimHelpInfo h;
		h.description = description;
		return h;
	}

	std::string Len(size_t n) { return std::string(n, 'x'); }

	Dia::API::ValueParam Param(const char* name)
	{
		Dia::API::ValueParam p;
		p.name = name;
		p.type = "int";
		return p;
	}

	struct FieldCase
	{
		const char* label;
		uint32_t limit;
		void (*set)(Dia::API::ValueParam&, const char*);
	};
}

// ---- AC 1: description is mandatory ----

TEST(SimHelpInfo, NullDescriptionIsRejectedAndRegistersNothing)
{
	SimQueryHost host;
	SimHelpInfo h;   // description null
	EXPECT_FALSE(host.RegisterQuery(StringCRC("a.q"), h, Ok));
	EXPECT_FALSE(host.RegisterCommand(StringCRC("a.c"), h, Ok));
	EXPECT_FALSE(host.Has(StringCRC("a.q")));
	EXPECT_FALSE(host.Has(StringCRC("a.c")));
	EXPECT_EQ(host.GetHelp(StringCRC("a.q")), nullptr);
}

TEST(SimHelpInfo, EmptyDescriptionIsRejectedAndRegistersNothing)
{
	SimQueryHost host;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("a.q"), Info(""), Ok));
	EXPECT_FALSE(host.RegisterCommand(StringCRC("a.c"), Info(""), Ok));
	EXPECT_FALSE(host.Has(StringCRC("a.q")));
	EXPECT_FALSE(host.Has(StringCRC("a.c")));
}

TEST(SimHelpInfo, RejectedRegistrationDoesNotBlockALaterValidOne)
{
	SimQueryHost host;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("a.q"), Info(""), Ok));
	EXPECT_TRUE(host.RegisterQuery(StringCRC("a.q"), Info("fine"), Ok));
	ASSERT_NE(host.GetHelp(StringCRC("a.q")), nullptr);
	EXPECT_STREQ(host.GetHelp(StringCRC("a.q"))->description, "fine");
}

TEST(SimHelpInfo, NullHandlerStillRejectedWithValidHelp)
{
	SimQueryHost host;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("a.q"), Info("d"), SimQueryHost::QueryHandler()));
	EXPECT_FALSE(host.Has(StringCRC("a.q")));
}

TEST(SimHelpInfo, GetHelpForUnknownNameIsNull)
{
	SimQueryHost host;
	EXPECT_EQ(host.GetHelp(StringCRC("nope")), nullptr);
}

TEST(SimHelpInfo, GetHelpIsGoneAfterUnregister)
{
	SimQueryHost host;
	ASSERT_TRUE(host.RegisterCommand(StringCRC("a.c"), Info("d"), Ok));
	EXPECT_NE(host.GetHelp(StringCRC("a.c")), nullptr);
	EXPECT_TRUE(host.UnregisterQuery(StringCRC("a.c")));
	EXPECT_EQ(host.GetHelp(StringCRC("a.c")), nullptr);
}

// ---- AC 2: strings are copied ----

TEST(SimHelpInfo, StoredHelpSurvivesMutatingAndFreeingCallerStrings)
{
	SimQueryHost host;
	char* description = new char[32]; std::strcpy(description, "original description");
	char* category    = new char[32]; std::strcpy(category, "cells");
	char* owner       = new char[32]; std::strcpy(owner, "DiaCellLayers");
	char* pname       = new char[32]; std::strcpy(pname, "layer");
	char* ptype       = new char[32]; std::strcpy(ptype, "enum");
	char* pdesc       = new char[32]; std::strcpy(pdesc, "which layer");
	char* punit       = new char[32]; std::strcpy(punit, "m");
	char* penum       = new char[32]; std::strcpy(penum, "a|b|c");

	Dia::API::ValueParam p;
	p.name = pname; p.type = ptype; p.required = true; p.description = pdesc; p.unit = punit;
	p.hasRange = true; p.min = -1.5f; p.max = 2.5f; p.enumValues = penum;
	SimHelpInfo h;
	h.description = description; h.category = category; h.owner = owner; h.params = &p; h.paramCount = 1; h.noArgs = false;

	ASSERT_TRUE(host.RegisterQuery(StringCRC("copy.q"), h, Ok));

	char* all[] = { description, category, owner, pname, ptype, pdesc, punit, penum };
	for (char* s : all)
	{
		std::memset(s, 'Z', 8);
		delete[] s;
	}
	p.name = nullptr; p.min = 99.0f;

	const SimHelpStored* st = host.GetHelp(StringCRC("copy.q"));
	ASSERT_NE(st, nullptr);
	EXPECT_STREQ(st->description, "original description");
	EXPECT_STREQ(st->category, "cells");
	EXPECT_STREQ(st->owner, "DiaCellLayers");
	ASSERT_EQ(st->paramCount, 1u);
	EXPECT_STREQ(st->params[0].name, "layer");
	EXPECT_STREQ(st->params[0].type, "enum");
	EXPECT_STREQ(st->params[0].description, "which layer");
	EXPECT_STREQ(st->params[0].unit, "m");
	EXPECT_STREQ(st->params[0].enumValues, "a|b|c");
	EXPECT_TRUE(st->params[0].required);
	EXPECT_TRUE(st->params[0].hasRange);
	EXPECT_FLOAT_EQ(st->params[0].min, -1.5f);
	EXPECT_FLOAT_EQ(st->params[0].max, 2.5f);
}

TEST(SimHelpInfo, NullOptionalFieldsAreStoredAsEmptyAndNoArgsIsKept)
{
	SimQueryHost host;
	SimHelpInfo h = Info("d");
	h.noArgs = true;
	ASSERT_TRUE(host.RegisterCommand(StringCRC("n.c"), h, Ok));
	const SimHelpStored* st = host.GetHelp(StringCRC("n.c"));
	ASSERT_NE(st, nullptr);
	EXPECT_STREQ(st->category, "");
	EXPECT_STREQ(st->owner, "");
	EXPECT_EQ(st->paramCount, 0u);
	EXPECT_TRUE(st->noArgs);
}

TEST(SimHelpInfo, EachNameKeepsItsOwnHelp)
{
	SimQueryHost host;
	ASSERT_TRUE(host.RegisterQuery(StringCRC("one"), Info("first"), Ok));
	ASSERT_TRUE(host.RegisterCommand(StringCRC("two"), Info("second"), Ok));
	EXPECT_STREQ(host.GetHelp(StringCRC("one"))->description, "first");
	EXPECT_STREQ(host.GetHelp(StringCRC("two"))->description, "second");
}

TEST(SimHelpInfo, ParamWithNullOptionalStringsIsStoredAsEmpty)
{
	SimQueryHost host;
	Dia::API::ValueParam p;
	p.name = "x";   // everything else null / default
	SimHelpInfo h = Info("d");
	h.params = &p; h.paramCount = 1;
	ASSERT_TRUE(host.RegisterQuery(StringCRC("p.q"), h, Ok));
	const SimParamStored& s = host.GetHelp(StringCRC("p.q"))->params[0];
	EXPECT_STREQ(s.name, "x");
	EXPECT_STREQ(s.type, "");
	EXPECT_STREQ(s.description, "");
	EXPECT_STREQ(s.unit, "");
	EXPECT_STREQ(s.enumValues, "");
	EXPECT_FALSE(s.required);
	EXPECT_FALSE(s.hasRange);
}

// ---- Duplicates and the cap still behave ----

TEST(SimHelpInfo, DuplicateNameIsStillRejectedAndKeepsFirstHelp)
{
	SimQueryHost host;
	EXPECT_TRUE(host.RegisterQuery(StringCRC("dup"), Info("first"), Ok));
	EXPECT_FALSE(host.RegisterQuery(StringCRC("dup"), Info("second"), Ok));
	EXPECT_FALSE(host.RegisterCommand(StringCRC("dup"), Info("third"), Ok));
	EXPECT_STREQ(host.GetHelp(StringCRC("dup"))->description, "first");
}

TEST(SimHelpInfo, HostCapIsStillEnforcedWithHelp)
{
	SimQueryHost host;
	for (int i = 0; i < 64; ++i)
		EXPECT_TRUE(host.RegisterQuery(StringCRC(("cap" + std::to_string(i)).c_str()), Info("d"), Ok)) << i;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("cap.extra"), Info("d"), Ok));
	EXPECT_EQ(host.GetHelp(StringCRC("cap.extra")), nullptr);
}

// ---- AC 8: limits ----

TEST(SimHelpInfo, DescriptionLimitIs256Bytes)
{
	SimQueryHost host;
	const std::string ok = Len(256), bad = Len(257);
	EXPECT_TRUE(host.RegisterQuery(StringCRC("d.ok"), Info(ok.c_str()), Ok));
	EXPECT_EQ(std::strlen(host.GetHelp(StringCRC("d.ok"))->description), 256u);
	EXPECT_FALSE(host.RegisterQuery(StringCRC("d.bad"), Info(bad.c_str()), Ok));
	EXPECT_FALSE(host.Has(StringCRC("d.bad")));
}

TEST(SimHelpInfo, CategoryAndOwnerLimitsAre64Bytes)
{
	SimQueryHost host;
	SimHelpInfo h = Info("d");
	const std::string ok = Len(64), bad = Len(65);
	h.category = ok.c_str(); h.owner = ok.c_str();
	EXPECT_TRUE(host.RegisterQuery(StringCRC("c.ok"), h, Ok));
	h.category = bad.c_str(); h.owner = ok.c_str();
	EXPECT_FALSE(host.RegisterQuery(StringCRC("c.bad"), h, Ok));
	h.category = ok.c_str(); h.owner = bad.c_str();
	EXPECT_FALSE(host.RegisterQuery(StringCRC("o.bad"), h, Ok));
}

TEST(SimHelpInfo, ParamCountLimitIs16)
{
	SimQueryHost host;
	std::vector<Dia::API::ValueParam> params(17, Param("p"));
	SimHelpInfo h = Info("d");
	h.params = params.data();
	h.paramCount = 16;
	EXPECT_TRUE(host.RegisterQuery(StringCRC("n16"), h, Ok));
	EXPECT_EQ(host.GetHelp(StringCRC("n16"))->paramCount, 16u);
	h.paramCount = 17;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("n17"), h, Ok));
	EXPECT_FALSE(host.Has(StringCRC("n17")));
}

TEST(SimHelpInfo, ParamCountWithNullParamsIsRejected)
{
	SimQueryHost host;
	SimHelpInfo h = Info("d");
	h.paramCount = 1;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("np"), h, Ok));
}

TEST(SimHelpInfo, ParamWithMissingOrEmptyNameIsRejected)
{
	SimQueryHost host;
	Dia::API::ValueParam p;   // name null
	SimHelpInfo h = Info("d");
	h.params = &p; h.paramCount = 1;
	EXPECT_FALSE(host.RegisterQuery(StringCRC("pn1"), h, Ok));
	p.name = "";
	EXPECT_FALSE(host.RegisterQuery(StringCRC("pn2"), h, Ok));
}

TEST(SimHelpInfo, EachParamStringLimitIsEnforcedAtTheBoundary)
{
	const FieldCase cases[] =
	{
		{ "name",        kSimHelpMaxParamName,        [](Dia::API::ValueParam& p, const char* s) { p.name = s; } },
		{ "type",        kSimHelpMaxParamType,        [](Dia::API::ValueParam& p, const char* s) { p.type = s; } },
		{ "description", kSimHelpMaxParamDescription, [](Dia::API::ValueParam& p, const char* s) { p.description = s; } },
		{ "unit",        kSimHelpMaxParamUnit,        [](Dia::API::ValueParam& p, const char* s) { p.unit = s; } },
		{ "enum",        kSimHelpMaxParamEnum,        [](Dia::API::ValueParam& p, const char* s) { p.enumValues = s; } },
	};
	EXPECT_EQ(kSimHelpMaxParamName, 64u);
	EXPECT_EQ(kSimHelpMaxParamType, 16u);
	EXPECT_EQ(kSimHelpMaxParamDescription, 192u);
	EXPECT_EQ(kSimHelpMaxParamUnit, 16u);
	EXPECT_EQ(kSimHelpMaxParamEnum, 128u);

	for (const FieldCase& c : cases)
	{
		SCOPED_TRACE(c.label);
		const std::string atLimit = Len(c.limit), over = Len(c.limit + 1);
		{
			SimQueryHost host;
			Dia::API::ValueParam p = Param("p");
			c.set(p, atLimit.c_str());
			SimHelpInfo h = Info("d");
			h.params = &p; h.paramCount = 1;
			EXPECT_TRUE(host.RegisterQuery(StringCRC("ok"), h, Ok));
		}
		{
			SimQueryHost host;
			Dia::API::ValueParam p = Param("p");
			c.set(p, over.c_str());
			SimHelpInfo h = Info("d");
			h.params = &p; h.paramCount = 1;
			EXPECT_FALSE(host.RegisterQuery(StringCRC("bad"), h, Ok));
			EXPECT_FALSE(host.Has(StringCRC("bad")));
		}
	}
}

TEST(SimHelpInfo, MaximalValidInfoIsStoredUnchanged)
{
	const std::string description = std::string(255, 'd') + "e";
	const std::string category = std::string(63, 'c') + "k";
	const std::string owner = std::string(63, 'o') + "w";
	std::vector<std::string> names, types, descs, units, enums;
	std::vector<Dia::API::ValueParam> params(kSimHelpMaxParams);
	for (uint32_t i = 0; i < kSimHelpMaxParams; ++i)
	{
		const char tail = static_cast<char>('a' + i);
		names.push_back(std::string(63, 'n') + tail);
		types.push_back(std::string(15, 't') + tail);
		descs.push_back(std::string(191, 'p') + tail);
		units.push_back(std::string(15, 'u') + tail);
		enums.push_back(std::string(127, 'e') + tail);
	}
	for (uint32_t i = 0; i < kSimHelpMaxParams; ++i)
	{
		Dia::API::ValueParam& p = params[i];
		p.name = names[i].c_str(); p.type = types[i].c_str(); p.description = descs[i].c_str();
		p.unit = units[i].c_str(); p.enumValues = enums[i].c_str();
		p.required = (i % 2) == 0; p.hasRange = (i % 3) == 0; p.min = static_cast<float>(i); p.max = static_cast<float>(i) + 0.5f;
	}
	SimHelpInfo h;
	h.description = description.c_str(); h.category = category.c_str(); h.owner = owner.c_str();
	h.params = params.data(); h.paramCount = kSimHelpMaxParams; h.noArgs = true;

	SimQueryHost host;
	ASSERT_TRUE(host.RegisterCommand(StringCRC("max"), h, Ok));
	const SimHelpStored* st = host.GetHelp(StringCRC("max"));
	ASSERT_NE(st, nullptr);
	EXPECT_EQ(description, st->description);
	EXPECT_EQ(category, st->category);
	EXPECT_EQ(owner, st->owner);
	EXPECT_TRUE(st->noArgs);
	ASSERT_EQ(st->paramCount, kSimHelpMaxParams);
	for (uint32_t i = 0; i < kSimHelpMaxParams; ++i)
	{
		EXPECT_EQ(names[i], st->params[i].name) << i;
		EXPECT_EQ(types[i], st->params[i].type) << i;
		EXPECT_EQ(descs[i], st->params[i].description) << i;
		EXPECT_EQ(units[i], st->params[i].unit) << i;
		EXPECT_EQ(enums[i], st->params[i].enumValues) << i;
		EXPECT_EQ(st->params[i].required, (i % 2) == 0) << i;
		EXPECT_EQ(st->params[i].hasRange, (i % 3) == 0) << i;
		EXPECT_FLOAT_EQ(st->params[i].min, static_cast<float>(i)) << i;
		EXPECT_FLOAT_EQ(st->params[i].max, static_cast<float>(i) + 0.5f) << i;
	}
}

TEST(SimHelpInfo, CopyHelpLeavesOutputUntouchedOnRejection)
{
	SimHelpStored out;
	std::memset(&out, 0x5A, sizeof(out));
	SimHelpStored before = out;
	const std::string big = Len(300);
	EXPECT_EQ(CopyHelp(Info(big.c_str()), out), SimHelpResult::DescriptionTooLong);
	EXPECT_EQ(std::memcmp(&before, &out, sizeof(out)), 0);
	EXPECT_EQ(CopyHelp(Info(nullptr), out), SimHelpResult::MissingDescription);
	EXPECT_EQ(std::memcmp(&before, &out, sizeof(out)), 0);
}

TEST(SimHelpInfo, EveryResultHasText)
{
	for (uint32_t i = 0; i <= static_cast<uint32_t>(SimHelpResult::ParamEnumTooLong); ++i)
		EXPECT_STRNE(SimHelpResultText(static_cast<SimHelpResult>(i)), "unknown") << i;
}
