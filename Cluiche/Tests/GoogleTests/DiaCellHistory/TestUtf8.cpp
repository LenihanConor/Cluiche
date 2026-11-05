#include <gtest/gtest.h>

#include <DiaCellHistory/Utf8.h>

#include <cstring>

using Dia::CellHistory::Utf8SafeLength;

TEST(Utf8SafeLength, AsciiIsUntouchedAndBoundedByCapAndLength)
{
	EXPECT_EQ(Utf8SafeLength("hello", 10), 5u);
	EXPECT_EQ(Utf8SafeLength("hello", 5), 5u);
	EXPECT_EQ(Utf8SafeLength("hello", 3), 3u);
}

TEST(Utf8SafeLength, NullEmptyAndZeroCap)
{
	EXPECT_EQ(Utf8SafeLength(nullptr, 10), 0u);
	EXPECT_EQ(Utf8SafeLength("", 10), 0u);
	EXPECT_EQ(Utf8SafeLength("abc", 0), 0u);
}

TEST(Utf8SafeLength, TwoByteCharacterStraddlingTheCap)
{
	const char* s = "a\xC3\xA9";		// a, e-acute
	EXPECT_EQ(Utf8SafeLength(s, 3), 3u);
	EXPECT_EQ(Utf8SafeLength(s, 2), 1u);
	EXPECT_EQ(Utf8SafeLength(s, 1), 1u);
	EXPECT_EQ(Utf8SafeLength("\xC3\xA9", 1), 0u);
}

TEST(Utf8SafeLength, ThreeByteCharacterStraddlingTheCapAtEachOffset)
{
	const char* s = "a\xE2\x82\xAC";	// a, euro
	EXPECT_EQ(Utf8SafeLength(s, 4), 4u);
	EXPECT_EQ(Utf8SafeLength(s, 3), 1u);
	EXPECT_EQ(Utf8SafeLength(s, 2), 1u);
	EXPECT_EQ(Utf8SafeLength(s, 1), 1u);
}

TEST(Utf8SafeLength, FourByteCharacterStraddlingTheCapAtEachOffset)
{
	const char* s = "ab\xF0\x9F\x98\x80";	// ab, grinning face
	EXPECT_EQ(Utf8SafeLength(s, 6), 6u);
	EXPECT_EQ(Utf8SafeLength(s, 5), 2u);
	EXPECT_EQ(Utf8SafeLength(s, 4), 2u);
	EXPECT_EQ(Utf8SafeLength(s, 3), 2u);
	EXPECT_EQ(Utf8SafeLength(s, 2), 2u);
}

TEST(Utf8SafeLength, CompleteSequencesBeforeTheCutAreKept)
{
	const char* s = "\xC3\xA9\xC3\xA9\xC3\xA9";
	EXPECT_EQ(Utf8SafeLength(s, 4), 4u);
	EXPECT_EQ(Utf8SafeLength(s, 5), 4u);
	EXPECT_EQ(Utf8SafeLength(s, 6), 6u);
}

TEST(Utf8SafeLength, StrayContinuationBytesDoNotLoopOrOverread)
{
	const char* s = "\x80\x80\x80\x80\x80";
	EXPECT_EQ(Utf8SafeLength(s, 5), 5u);
	EXPECT_EQ(Utf8SafeLength(s, 2), 2u);
	EXPECT_EQ(Utf8SafeLength("a\x80\x80", 3), 3u);
}

TEST(Utf8SafeLength, TruncatedLeadAtTheEndOfTheStringIsDropped)
{
	EXPECT_EQ(Utf8SafeLength("ab\xC3", 10), 2u);
	EXPECT_EQ(Utf8SafeLength("ab\xE2\x82", 10), 2u);
	EXPECT_EQ(Utf8SafeLength("ab\xF0\x9F\x98", 10), 2u);
}

TEST(Utf8SafeLength, InvalidLeadBytesAreLeftAlone)
{
	EXPECT_EQ(Utf8SafeLength("ab\xFF", 10), 3u);
	EXPECT_EQ(Utf8SafeLength("ab\xC0", 10), 2u);		// C0 still reads as a 2-byte lead; dropped when incomplete
}
