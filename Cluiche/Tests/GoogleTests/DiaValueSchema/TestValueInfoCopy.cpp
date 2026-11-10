#include <gtest/gtest.h>
#include <DiaValueSchema/ValueInfo.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaValueSchema/CategoricalColours.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include "ValueSchemaTestSupport.h"
#include <string>

// ValueInfo / ValueList exhaustive tests: capacity constants, copy / assign semantics, deep copy.

using namespace Dia::ValueSchema;
using namespace Dia::Core::Containers;

namespace
{
	template<class T> unsigned CapOf(const T&) { return T::Capacity() - 1; }

	ValueList<String128> MakeStrings(unsigned n)
	{
		ValueList<String128> l;
		if (n > 0) { l.Reserve(n); }
		for (unsigned i = 0; i < n; ++i)
		{
			String128 s;
			s.Format("item-%u", i);
			l.Add(s);
		}
		return l;
	}

	void ExpectSameStrings(const ValueList<String128>& a, const ValueList<String128>& b)
	{
		ASSERT_EQ(a.Size(), b.Size());
		for (unsigned i = 0; i < a.Size(); ++i) { EXPECT_STREQ(a[i].AsCStr(), b[i].AsCStr()); }
	}

	ValueInfo MakeFull()
	{
		ValueInfo v;
		v.mFullKey = "fmt/rec[].k";
		v.mFor = "fmt";
		v.mKey = "rec[].k";
		v.mKind = ValueKind::Event;
		v.mLabel = "Label";
		v.mDescription = "Description";
		v.mHint = "Hint";
		v.mUnit = "m/s";
		v.mType = ValueType::Enum;
		v.mHasMin = true; v.mMin = -2.5;
		v.mHasMax = true; v.mMax = 9.5;
		v.mHasDefault = true; v.mDefaultNumber = 3.25;
		v.mDefaultString = "dflt";
		v.mSource = "src";
		v.mUnique = true;
		v.mRequired = true;
		v.mRef = "ref";
		v.mMapKeyRef = "keyref";
		v.mMapValueType = ValueType::Bool;
		v.mSourceFile = "file.diavalues";

		v.mEnumValues.Reserve(2);
		for (int i = 0; i < 2; ++i)
		{
			EnumValue e;
			e.mId.Format("id%d", i);
			e.mLabel.Format("label%d", i);
			e.mDescription.Format("desc%d", i);
			e.mColour = "#112233";
			e.mHasColour = (i == 0);
			v.mEnumValues.Add(e);
		}
		v.mAffects = MakeStrings(3);
		v.mRelated = MakeStrings(2);
		v.mPathSegments.Reserve(3);
		{
			String64 s; s = "rec"; v.mPathSegments.Add(s);
			s = "[]"; v.mPathSegments.Add(s);
			s = "k"; v.mPathSegments.Add(s);
		}
		v.mUi["group"] = "G";
		v.mUi["nested"]["deep"]["x"] = 5;
		v.mHistory["track"] = true;
		v.mPayload.Reserve(1);
		{
			PayloadField f;
			f.mName = "pname"; f.mType = ValueType::Int; f.mDescription = "pdesc"; f.mUnit = "pu";
			v.mPayload.Add(f);
		}
		v.mVocabulary.Reserve(1);
		{
			VocabularyEntry f;
			f.mId = "vid"; f.mLabel = "vlabel"; f.mDescription = "vdesc"; f.mUnit = "vu";
			v.mVocabulary.Add(f);
		}
		return v;
	}

	void ExpectSameFull(const ValueInfo& a, const ValueInfo& b)
	{
		EXPECT_STREQ(a.mFullKey.AsCStr(), b.mFullKey.AsCStr());
		EXPECT_STREQ(a.mFor.AsCStr(), b.mFor.AsCStr());
		EXPECT_STREQ(a.mKey.AsCStr(), b.mKey.AsCStr());
		EXPECT_EQ(a.mKind, b.mKind);
		EXPECT_STREQ(a.mLabel.AsCStr(), b.mLabel.AsCStr());
		EXPECT_STREQ(a.mDescription.AsCStr(), b.mDescription.AsCStr());
		EXPECT_STREQ(a.mHint.AsCStr(), b.mHint.AsCStr());
		EXPECT_STREQ(a.mUnit.AsCStr(), b.mUnit.AsCStr());
		EXPECT_EQ(a.mType, b.mType);
		EXPECT_EQ(a.mHasMin, b.mHasMin); EXPECT_EQ(a.mMin, b.mMin);
		EXPECT_EQ(a.mHasMax, b.mHasMax); EXPECT_EQ(a.mMax, b.mMax);
		EXPECT_EQ(a.mHasDefault, b.mHasDefault); EXPECT_EQ(a.mDefaultNumber, b.mDefaultNumber);
		EXPECT_STREQ(a.mDefaultString.AsCStr(), b.mDefaultString.AsCStr());
		EXPECT_STREQ(a.mSource.AsCStr(), b.mSource.AsCStr());
		EXPECT_EQ(a.mUnique, b.mUnique); EXPECT_EQ(a.mRequired, b.mRequired);
		EXPECT_STREQ(a.mRef.AsCStr(), b.mRef.AsCStr());
		EXPECT_STREQ(a.mMapKeyRef.AsCStr(), b.mMapKeyRef.AsCStr());
		EXPECT_EQ(a.mMapValueType, b.mMapValueType);
		EXPECT_STREQ(a.mSourceFile.AsCStr(), b.mSourceFile.AsCStr());
		EXPECT_EQ(Json::FastWriter().write(a.mUi), Json::FastWriter().write(b.mUi));
		EXPECT_EQ(Json::FastWriter().write(a.mHistory), Json::FastWriter().write(b.mHistory));
		ExpectSameStrings(a.mAffects, b.mAffects);
		ExpectSameStrings(a.mRelated, b.mRelated);
		ASSERT_EQ(a.mEnumValues.Size(), b.mEnumValues.Size());
		for (unsigned i = 0; i < a.mEnumValues.Size(); ++i)
		{
			EXPECT_STREQ(a.mEnumValues[i].mId.AsCStr(), b.mEnumValues[i].mId.AsCStr());
			EXPECT_STREQ(a.mEnumValues[i].mLabel.AsCStr(), b.mEnumValues[i].mLabel.AsCStr());
			EXPECT_STREQ(a.mEnumValues[i].mDescription.AsCStr(), b.mEnumValues[i].mDescription.AsCStr());
			EXPECT_STREQ(a.mEnumValues[i].mColour.AsCStr(), b.mEnumValues[i].mColour.AsCStr());
			EXPECT_EQ(a.mEnumValues[i].mHasColour, b.mEnumValues[i].mHasColour);
		}
		ASSERT_EQ(a.mPathSegments.Size(), b.mPathSegments.Size());
		for (unsigned i = 0; i < a.mPathSegments.Size(); ++i) { EXPECT_STREQ(a.mPathSegments[i].AsCStr(), b.mPathSegments[i].AsCStr()); }
		ASSERT_EQ(a.mPayload.Size(), b.mPayload.Size());
		for (unsigned i = 0; i < a.mPayload.Size(); ++i)
		{
			EXPECT_STREQ(a.mPayload[i].mName.AsCStr(), b.mPayload[i].mName.AsCStr());
			EXPECT_EQ(a.mPayload[i].mType, b.mPayload[i].mType);
			EXPECT_STREQ(a.mPayload[i].mDescription.AsCStr(), b.mPayload[i].mDescription.AsCStr());
			EXPECT_STREQ(a.mPayload[i].mUnit.AsCStr(), b.mPayload[i].mUnit.AsCStr());
		}
		ASSERT_EQ(a.mVocabulary.Size(), b.mVocabulary.Size());
		for (unsigned i = 0; i < a.mVocabulary.Size(); ++i)
		{
			EXPECT_STREQ(a.mVocabulary[i].mId.AsCStr(), b.mVocabulary[i].mId.AsCStr());
			EXPECT_STREQ(a.mVocabulary[i].mLabel.AsCStr(), b.mVocabulary[i].mLabel.AsCStr());
			EXPECT_STREQ(a.mVocabulary[i].mDescription.AsCStr(), b.mVocabulary[i].mDescription.AsCStr());
			EXPECT_STREQ(a.mVocabulary[i].mUnit.AsCStr(), b.mVocabulary[i].mUnit.AsCStr());
		}
	}
}

TEST(DiaValueSchemaValueInfo, MaxLengthConstantsMatchStringCapacities)
{
	ValueInfo v;
	EXPECT_EQ(ValueInfo::kMaxForLength, CapOf(v.mFor));
	EXPECT_EQ(ValueInfo::kMaxKeyLength, CapOf(v.mKey));
	EXPECT_EQ(ValueInfo::kMaxFullKeyLength, CapOf(v.mFullKey));
	EXPECT_EQ(ValueInfo::kMaxLabelLength, CapOf(v.mLabel));
	EXPECT_EQ(ValueInfo::kMaxDescriptionLength, CapOf(v.mDescription));
	EXPECT_EQ(ValueInfo::kMaxHintLength, CapOf(v.mHint));
	EXPECT_EQ(ValueInfo::kMaxUnitLength, CapOf(v.mUnit));
	EXPECT_EQ(ValueInfo::kMaxRefLength, CapOf(v.mRef));
	EXPECT_EQ(ValueInfo::kMaxSourceLength, CapOf(v.mSource));
	EXPECT_EQ(ValueInfo::kMaxSourceFileLength, CapOf(v.mSourceFile));
	// Fields without a named constant still follow the documented policy.
	EXPECT_EQ(127u, CapOf(v.mDefaultString));
	EXPECT_EQ(127u, CapOf(v.mMapKeyRef));
}

TEST(DiaValueSchemaValueInfo, FullKeyCapacityHoldsMaxForAndKey)
{
	// "<for>/<key>" with the longest for and key must fit in mFullKey.
	EXPECT_LE(ValueInfo::kMaxForLength + 1 + ValueInfo::kMaxKeyLength, ValueInfo::kMaxFullKeyLength);
}

TEST(DiaValueSchemaValueInfo, DefaultConstructedIsEmpty)
{
	ValueInfo v;
	EXPECT_STREQ(v.mKey.AsCStr(), "");
	EXPECT_EQ(v.mEnumValues.Size(), 0u);
	EXPECT_EQ(v.mPayload.Size(), 0u);
	EXPECT_TRUE(v.mUi.isNull());
	EXPECT_TRUE(v.mHistory.isNull());
	EXPECT_FALSE(v.mHasMin);
	EXPECT_FALSE(v.mHasDefault);
}

TEST(DiaValueSchemaValueList, CopyConstructFromSizes)
{
	for (unsigned n : { 0u, 1u, 100u })
	{
		SCOPED_TRACE(n);
		const ValueList<String128> src = MakeStrings(n);
		ValueList<String128> copy(src);
		ExpectSameStrings(src, copy);
		if (n > 0)
		{
			String128 changed; changed = "changed";
			copy[0] = changed;
			EXPECT_STRNE(src[0].AsCStr(), "changed") << "copy must be independent";
		}
	}
}

TEST(DiaValueSchemaValueList, AssignFromSizesOverEmptyList)
{
	for (unsigned n : { 0u, 1u, 100u })
	{
		SCOPED_TRACE(n);
		const ValueList<String128> src = MakeStrings(n);
		ValueList<String128> dst;
		dst = src;
		ExpectSameStrings(src, dst);
	}
}

TEST(DiaValueSchemaValueList, AssignOverNonEmptyListReplacesContent)
{
	for (unsigned dstN : { 1u, 5u, 100u })
	{
		for (unsigned srcN : { 0u, 1u, 3u, 100u, 150u })
		{
			SCOPED_TRACE(::testing::Message() << dstN << " <- " << srcN);
			ValueList<String128> dst = MakeStrings(dstN);
			const ValueList<String128> src = MakeStrings(srcN);
			dst = src;
			ExpectSameStrings(src, dst);
		}
	}
}

TEST(DiaValueSchemaValueList, SelfAssignKeepsContent)
{
	ValueList<String128> l = MakeStrings(7);
	ValueList<String128>& alias = l;
	l = alias;
	ASSERT_EQ(l.Size(), 7u);
	EXPECT_STREQ(l[6].AsCStr(), "item-6");

	ValueList<String128> empty;
	ValueList<String128>& emptyAlias = empty;
	empty = emptyAlias;
	EXPECT_EQ(empty.Size(), 0u);
}

TEST(DiaValueSchemaValueList, ChainedAssignAndRepeatedAssign)
{
	const ValueList<String128> src = MakeStrings(10);
	ValueList<String128> a, b, c;
	a = b = c = src;
	ExpectSameStrings(src, a);
	ExpectSameStrings(src, b);
	ExpectSameStrings(src, c);
	for (int i = 0; i < 50; ++i) { a = src; }
	ExpectSameStrings(src, a);
}

TEST(DiaValueSchemaValueList, CopyOfValueInfoListWithJsonIsDeep)
{
	ValueList<ValueInfo> src;
	src.Reserve(3);
	for (int i = 0; i < 3; ++i)
	{
		ValueInfo v = MakeFull();
		v.mKey.Format("k%d", i);
		v.mUi["idx"] = i;
		src.Add(v);
	}
	ValueList<ValueInfo> copy(src);
	ASSERT_EQ(copy.Size(), 3u);
	copy[1].mUi["idx"] = 99;
	copy[1].mUi["extra"] = "e";
	EXPECT_EQ(src[1].mUi["idx"].asInt(), 1);
	EXPECT_FALSE(src[1].mUi.isMember("extra"));
	EXPECT_EQ(copy[2].mUi["idx"].asInt(), 2);
}

TEST(DiaValueSchemaValueList, AssignValueInfoListOverNonEmptyListWithJson)
{
	// Regression guard: assigning over a list whose elements own Json payloads (objects/strings) must not
	// double-destroy them (DynamicArray::RemoveAll runs destructors in place).
	ValueList<ValueInfo> dst;
	dst.Reserve(2);
	for (int i = 0; i < 2; ++i) { ValueInfo v = MakeFull(); v.mUi["n"] = i; dst.Add(v); }

	ValueList<ValueInfo> src;
	src.Reserve(3);
	for (int i = 0; i < 3; ++i) { ValueInfo v = MakeFull(); v.mUi["m"] = i + 10; src.Add(v); }

	dst = src;
	ASSERT_EQ(dst.Size(), 3u);
	EXPECT_EQ(dst[2].mUi["m"].asInt(), 12);
	EXPECT_FALSE(dst[0].mUi.isMember("n"));
	dst = ValueList<ValueInfo>();
	EXPECT_EQ(dst.Size(), 0u);
}

TEST(DiaValueSchemaValueList, ReserveGrowthKeepsElementsWithJsonAlive)
{
	// Growing a list that already holds Json-owning elements must copy them, not memcpy-move and double-destroy.
	ValueList<ValueInfo> list;
	list.Reserve(2);
	for (int i = 0; i < 2; ++i) { ValueInfo v = MakeFull(); v.mUi["n"] = i; list.Add(v); }
	list.Reserve(50);
	ASSERT_EQ(list.Size(), 2u);
	EXPECT_GE(list.Capacity(), 50u);
	for (int i = 2; i < 50; ++i) { ValueInfo v = MakeFull(); v.mUi["n"] = i; list.Add(v); }
	ASSERT_EQ(list.Size(), 50u);
	for (int i = 0; i < 50; ++i) { ASSERT_EQ(list[i].mUi["n"].asInt(), i); }
	list.Reserve(1);	// never below the live size
	EXPECT_EQ(list.Size(), 50u);
	EXPECT_GE(list.Capacity(), 50u);
}

TEST(DiaValueSchemaValueList, RemoveAtRemoveAndRemoveAllKeepRemainingElementsIntact)
{
	ValueList<ValueInfo> list;
	list.Reserve(5);
	for (int i = 0; i < 5; ++i) { ValueInfo v = MakeFull(); v.mUi["n"] = i; list.Add(v); }
	list.RemoveAt(1);
	ASSERT_EQ(list.Size(), 4u);
	EXPECT_EQ(list[0].mUi["n"].asInt(), 0);
	EXPECT_EQ(list[1].mUi["n"].asInt(), 2);
	EXPECT_EQ(list[3].mUi["n"].asInt(), 4);
	list.Remove();
	ASSERT_EQ(list.Size(), 3u);
	EXPECT_EQ(list[2].mUi["n"].asInt(), 3);
	list.RemoveAll();
	EXPECT_EQ(list.Size(), 0u);
	for (int i = 0; i < 5; ++i) { ValueInfo v = MakeFull(); v.mUi["m"] = i; list.Add(v); }	// slots are reusable
	EXPECT_EQ(list[4].mUi["m"].asInt(), 4);
	EXPECT_FALSE(list[4].mUi.isMember("n"));
}

TEST(DiaValueSchemaValueInfo, DeepCopyOfFullyPopulatedInfo)
{
	const ValueInfo original = MakeFull();
	ValueInfo copy = original;
	ExpectSameFull(original, copy);

	ValueInfo assigned;
	assigned = original;
	ExpectSameFull(original, assigned);
}

TEST(DiaValueSchemaValueInfo, MutatingCopyLeavesOriginalUntouched)
{
	const ValueInfo original = MakeFull();
	ValueInfo copy = original;

	copy.mKey = "other";
	copy.mEnumValues[0].mId = "mutated";
	copy.mEnumValues[1].mHasColour = true;
	copy.mAffects[0] = String128();
	copy.mRelated.RemoveAll();
	copy.mPathSegments[0] = String64();
	copy.mPayload[0].mName = "mut";
	copy.mVocabulary[0].mId = "mut";
	copy.mUi["nested"]["deep"]["x"] = 6;
	copy.mUi["added"] = true;
	copy.mHistory["track"] = false;
	copy.mMin = 100.0;

	const ValueInfo reference = MakeFull();
	ExpectSameFull(reference, original);
}

TEST(DiaValueSchemaValueInfo, ThousandEnumValuesCopy)
{
	ValueInfo v;
	v.mEnumValues.Reserve(1000);
	for (int i = 0; i < 1000; ++i)
	{
		EnumValue e;
		e.mId.Format("v%d", i);
		e.mLabel.Format("L%d", i);
		e.mColour = GetCategoricalColour(i);
		v.mEnumValues.Add(e);
	}
	ValueInfo copy = v;
	ASSERT_EQ(copy.mEnumValues.Size(), 1000u);
	for (int i = 0; i < 1000; ++i)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "v%d", i);
		ASSERT_STREQ(copy.mEnumValues[i].mId.AsCStr(), buf);
	}
	copy.mEnumValues[999].mId = "changed";
	EXPECT_STREQ(v.mEnumValues[999].mId.AsCStr(), "v999");
}

TEST(DiaValueSchemaValueInfo, EnumListFromLoaderCopiesIntact)
{
	Json::Value e = VsTest::GoodEntry("e");
	e["type"] = "enum";
	e["values"] = Json::Value(Json::arrayValue);
	for (int i = 0; i < 30; ++i)
	{
		Json::Value v(Json::objectValue);
		char id[16]; snprintf(id, sizeof(id), "id%d", i);
		v["id"] = id;
		e["values"].append(v);
	}
	LoadResult r = VsTest::LoadEntry(e);
	ASSERT_TRUE(r.mOk);
	LoadResult copy = r;
	ASSERT_EQ(copy.mEntries.Size(), 1u);
	ASSERT_EQ(copy.mEntries[0].mEnumValues.Size(), 30u);
	EXPECT_STREQ(copy.mEntries[0].mEnumValues[29].mId.AsCStr(), "id29");
	EXPECT_STREQ(copy.mEntries[0].mEnumValues[29].mColour.AsCStr(), GetCategoricalColour(29));
}
