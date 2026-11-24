#include <gtest/gtest.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/ValueSchemaQuery.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include "RampTestSupport.h"
#include <string>

// Feature 6 task 2 (AC 8): ValueSchemaService::ExportSubset.

using namespace Dia::ValueSchema;
using Dia::Core::StringCRC;

namespace
{
	void AddRef(ValueList<Dia::Core::Containers::String128>& list, const char* text)
	{
		Dia::Core::Containers::String128 s;
		SetText(s, text);
		list.Reserve(list.Size() + 1);
		list.Add(s);
	}

	ValueInfo Entry(const char* key, const char* ramp = nullptr, const char* deltaRamp = nullptr)
	{
		ValueInfo info = Dia::ValueSchema::Testing::MakeValueInfo("sub", key);
		if (ramp) { info.mUi["ramp"] = ramp; }
		if (deltaRamp) { info.mUi["delta_ramp"] = deltaRamp; }
		return info;
	}

	StringCRC K(const char* key) { return StringCRC((std::string("sub/") + key).c_str()); }

	bool HasEntry(const Json::Value& out, const char* key)
	{
		for (Json::ArrayIndex i = 0; i < out["entries"].size(); ++i)
		{
			if (out["entries"][i]["key"].asString() == key) { return true; }
		}
		return false;
	}

	bool HasRamp(const Json::Value& out, const char* id)
	{
		for (Json::ArrayIndex i = 0; i < out["ramps"].size(); ++i)
		{
			if (out["ramps"][i]["id"].asString() == id) { return true; }
		}
		return false;
	}

	struct SubsetFixture
	{
		VsTest::RampFixture mFx;
		SubsetFixture() : mFx(true, false) {}
		void Add(ValueInfo info) { mFx.mSchema.Register(info); }
		const ValueSchemaService& Done() { mFx.Finish(); return mFx.Svc(); }
	};
}

TEST(DiaValueSchemaExportSubset, RequestedEntriesPresentInOrder)
{
	SubsetFixture f;
	f.Add(Entry("a")); f.Add(Entry("b")); f.Add(Entry("c"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("c"), K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 2, out));
	ASSERT_EQ(out["entries"].size(), 2u);
	EXPECT_EQ(out["entries"][0]["key"].asString(), "c");
	EXPECT_EQ(out["entries"][1]["key"].asString(), "a");
}

TEST(DiaValueSchemaExportSubset, EntryJsonEqualsExportEntryJson)
{
	SubsetFixture f;
	f.Add(Entry("a", "sequential.grey"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	ASSERT_EQ(out["entries"].size(), 1u);
	EXPECT_EQ(out["entries"][0], ValueInfoToExportJson(*svc.Find(K("a"))));
	EXPECT_TRUE(out["entries"][0].isMember("origin"));
}

TEST(DiaValueSchemaExportSubset, OnlyUsedRampsIncluded)
{
	SubsetFixture f;
	f.Add(Entry("a", "sequential.grey", "diverging.blue_red"));
	f.Add(Entry("b", "sequential.heat"));
	const ValueSchemaService& svc = f.Done();
	ASSERT_NE(svc.FindRamp(StringCRC("sequential.grey")), nullptr);
	ASSERT_NE(svc.FindRamp(StringCRC("diverging.blue_red")), nullptr);
	ASSERT_GT(svc.RampCount(), 2u);
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	EXPECT_EQ(out["ramps"].size(), 2u);
	EXPECT_TRUE(HasRamp(out, "sequential.grey"));
	EXPECT_TRUE(HasRamp(out, "diverging.blue_red"));
}

TEST(DiaValueSchemaExportSubset, RampsFollowRegistrationOrderAndMatchRampToJson)
{
	SubsetFixture f;
	f.Add(Entry("a", "diverging.blue_red", "sequential.grey"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	ASSERT_EQ(out["ramps"].size(), 2u);
	unsigned first = 0, second = 0;
	for (unsigned i = 0; i < svc.RampCount(); ++i)
	{
		const std::string n = svc.RampAt(i)->mName.AsCStr();
		if (n == "sequential.grey") { first = i; }
		if (n == "diverging.blue_red") { second = i; }
	}
	const unsigned lo = first < second ? first : second;
	EXPECT_EQ(out["ramps"][0], RampToJson(*svc.RampAt(lo)));
}

TEST(DiaValueSchemaExportSubset, AffectsAndRelatedFollowedOneLevel)
{
	SubsetFixture f;
	ValueInfo a = Entry("a"); AddRef(a.mAffects, "b"); AddRef(a.mRelated, "sub/c");
	ValueInfo b = Entry("b"); AddRef(b.mAffects, "d");
	f.Add(a); f.Add(b); f.Add(Entry("c")); f.Add(Entry("d"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	ASSERT_EQ(out["entries"].size(), 3u);
	EXPECT_EQ(out["entries"][0]["key"].asString(), "a");
	EXPECT_EQ(out["entries"][1]["key"].asString(), "b");
	EXPECT_EQ(out["entries"][2]["key"].asString(), "c");
	EXPECT_FALSE(HasEntry(out, "d"));
}

TEST(DiaValueSchemaExportSubset, ReferencedEntriesRampsIncluded)
{
	SubsetFixture f;
	ValueInfo a = Entry("a"); AddRef(a.mRelated, "b");
	f.Add(a); f.Add(Entry("b", "sequential.heat"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	EXPECT_TRUE(HasRamp(out, "sequential.heat"));
}

TEST(DiaValueSchemaExportSubset, CycleTerminates)
{
	SubsetFixture f;
	ValueInfo a = Entry("a"); AddRef(a.mAffects, "b");
	ValueInfo b = Entry("b"); AddRef(b.mAffects, "a");
	f.Add(a); f.Add(b);
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	EXPECT_EQ(out["entries"].size(), 2u);
}

TEST(DiaValueSchemaExportSubset, DuplicatesAppearOnce)
{
	SubsetFixture f;
	ValueInfo a = Entry("a"); AddRef(a.mAffects, "b"); AddRef(a.mRelated, "b");
	f.Add(a); f.Add(Entry("b"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a"), K("b"), K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 3, out));
	EXPECT_EQ(out["entries"].size(), 2u);
}

TEST(DiaValueSchemaExportSubset, UnknownKeySkippedAndCountedOncePerKey)
{
	SubsetFixture f;
	f.Add(Entry("a"));
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("nope"), K("a"), K("nope") };
	Json::Value out;
	SubsetReport report;
	ASSERT_TRUE(svc.ExportSubset(keys, 3, out, &report));
	EXPECT_EQ(out["entries"].size(), 1u);
	EXPECT_EQ(report.mSkippedUnknown, 1u);
	EXPECT_EQ(report.mReferencedUnresolved, 0u);
}

TEST(DiaValueSchemaExportSubset, UnresolvedReferenceSkippedAndCounted)
{
	SubsetFixture f;
	ValueInfo a = Entry("a"); AddRef(a.mAffects, "missing");
	f.Add(a);
	const ValueSchemaService& svc = f.Done();
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	SubsetReport report;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out, &report));
	EXPECT_EQ(out["entries"].size(), 1u);
	EXPECT_EQ(report.mReferencedUnresolved, 1u);
	EXPECT_EQ(report.mSkippedUnknown, 0u);
}

TEST(DiaValueSchemaExportSubset, EmptyKeyListGivesEmptyArrays)
{
	SubsetFixture f;
	f.Add(Entry("a"));
	const ValueSchemaService& svc = f.Done();
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(nullptr, 0, out));
	ASSERT_TRUE(out["entries"].isArray());
	ASSERT_TRUE(out["ramps"].isArray());
	EXPECT_EQ(out["entries"].size(), 0u);
	EXPECT_EQ(out["ramps"].size(), 0u);
}

TEST(DiaValueSchemaExportSubset, NullKeysWithCountFails)
{
	SubsetFixture f;
	const ValueSchemaService& svc = f.Done();
	Json::Value out;
	EXPECT_FALSE(svc.ExportSubset(nullptr, 2, out));
}

TEST(DiaValueSchemaExportSubset, ServiceWithoutRampRegistryGivesEmptyRamps)
{
	ValueSchemaRegistry schema;
	schema.Register(Entry("a", "sequential.grey"));
	schema.Publish();
	ValueSchemaService svc(schema);
	const StringCRC keys[] = { K("a") };
	Json::Value out;
	ASSERT_TRUE(svc.ExportSubset(keys, 1, out));
	EXPECT_EQ(out["entries"].size(), 1u);
	ASSERT_TRUE(out["ramps"].isArray());
	EXPECT_EQ(out["ramps"].size(), 0u);
}
