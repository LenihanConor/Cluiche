#include <gtest/gtest.h>
#include <DiaCore/Strings/String8.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String128.h>
#include <DiaCore/Strings/String1024.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstring>

using namespace Dia::Core::Containers;

TEST(String, DefaultConstruction_CreatesEmptyString)
{
	String8 str8;
	String32 str32;
	String64 str64;

	EXPECT_EQ(str8.Length(), 0);
	EXPECT_EQ(str8.Size(), 8);
	EXPECT_EQ(str8.Back(), '\0');
	EXPECT_EQ(strlen(str8.AsCStr()), 0);
	EXPECT_TRUE(str8.IsNullTerminating());

	EXPECT_EQ(str32.Length(), 0);
	EXPECT_EQ(str32.Size(), 32);

	EXPECT_EQ(str64.Length(), 0);
	EXPECT_EQ(str64.Size(), 64);
}

TEST(String, ConstructionFromCString_TruncatesIfNeeded)
{
	const char testStr[] = "I like lots of apples";

	String8 str8(testStr);

	EXPECT_EQ(str8.Length(), 7);
	EXPECT_EQ(str8.Size(), 8);
	EXPECT_EQ(strlen(str8.AsCStr()), 7);
	EXPECT_TRUE(str8.IsNullTerminating());
	EXPECT_STREQ(str8.AsCStr(), "I like ");

	String32 str32(testStr);

	EXPECT_EQ(str32.Length(), 21);
	EXPECT_EQ(str32.Size(), 32);
	EXPECT_EQ(strlen(str32.AsCStr()), 21);
	EXPECT_TRUE(str32.IsNullTerminating());
	EXPECT_STREQ(str32.AsCStr(), "I like lots of apples");
}

TEST(String, CopyConstruction_CopiesContent)
{
	const char testStr[] = "I like lots of apples";

	String8 str1(testStr);
	String8 str2(str1);

	EXPECT_EQ(str2.Length(), 7);
	EXPECT_EQ(str2.Size(), 8);
	EXPECT_EQ(strlen(str2.AsCStr()), 7);
	EXPECT_TRUE(str2.IsNullTerminating());
	EXPECT_STREQ(str2.AsCStr(), "I like ");

	String32 str3(testStr);
	String32 str4(str3);

	EXPECT_EQ(str4.Length(), 21);
	EXPECT_STREQ(str4.AsCStr(), "I like lots of apples");
}

TEST(String, ConstructionWithSubstring_ExtractsRange)
{
	const char testStr[] = "I like lots of apples";

	String8 str1(testStr);
	String8 str2(str1, 3, 3);

	EXPECT_EQ(str2.Length(), 3);
	EXPECT_EQ(str2.Size(), 8);
	EXPECT_EQ(strlen(str2.AsCStr()), 3);
	EXPECT_TRUE(str2.IsNullTerminating());
	EXPECT_STREQ(str2.AsCStr(), "ike");

	String32 str3(testStr);
	String32 str4(str3, 7, 4);

	EXPECT_EQ(str4.Length(), 4);
	EXPECT_STREQ(str4.AsCStr(), "lots");
}

TEST(String, ConstructionFromDifferentSize_TruncatesOrCopies)
{
	const char testStr[] = "I like lots of apples";

	String32 str1(testStr);
	String8 str2(str1);

	EXPECT_EQ(str2.Length(), 7);
	EXPECT_EQ(str2.Size(), 8);
	EXPECT_STREQ(str2.AsCStr(), "I like ");

	String8 str3("Hello");
	String32 str4(str3);

	EXPECT_EQ(str4.Length(), 5);
	EXPECT_STREQ(str4.AsCStr(), "Hello");
}

TEST(String, Assignment_OverwritesContent)
{
	const char testStr[] = "I like lots of apples";

	String8 str1(testStr);
	String8 str2;
	str2 = str1;

	EXPECT_EQ(str2.Length(), 7);
	EXPECT_STREQ(str2.AsCStr(), "I like ");

	String32 str3(testStr);
	String32 str4;
	str4 = str3;

	EXPECT_EQ(str4.Length(), 21);
	EXPECT_STREQ(str4.AsCStr(), "I like lots of apples");
}

TEST(String, AssignFromCString_UpdatesContent)
{
	const char testStr1[] = "Hello";
	const char testStr2[] = "World";

	String32 str1(testStr1);

	EXPECT_STREQ(str1.AsCStr(), "Hello");

	str1 = testStr2;

	EXPECT_EQ(str1.Length(), 5);
	EXPECT_STREQ(str1.AsCStr(), "World");
}

TEST(String, ComparisonOperators_CompareCorrectly)
{
	String32 str1("Hello");
	String32 str2("Hello");
	String32 str3("World");

	EXPECT_TRUE(str1 == str2);
	EXPECT_FALSE(str1 == str3);
	EXPECT_TRUE(str1 != str3);
	EXPECT_FALSE(str1 != str2);

	EXPECT_FALSE(str1 < str2);
	EXPECT_TRUE(str1 < str3);
	EXPECT_FALSE(str3 < str1);

	EXPECT_FALSE(str1 > str2);
	EXPECT_FALSE(str1 > str3);
	EXPECT_TRUE(str3 > str1);
}

TEST(String, ElementAccess_WorksCorrectlyWithBoundsChecking)
{
	String32 str1("Hello");

	EXPECT_EQ(str1[0], 'H');
	EXPECT_EQ(str1[1], 'e');
	EXPECT_EQ(str1[2], 'l');
	EXPECT_EQ(str1[3], 'l');
	EXPECT_EQ(str1[4], 'o');

	EXPECT_EQ(str1.At(0), 'H');
	EXPECT_EQ(str1.At(4), 'o');

	EXPECT_EQ(str1.Front(), 'H');
	EXPECT_EQ(str1.Back(), 'o');

	EXPECT_DEATH({ char c = str1[-1]; }, "");
	EXPECT_DEATH({ char c = str1[50]; }, "");
}

TEST(String, Clear_EmptiesString)
{
	String32 str1("Hello World");

	EXPECT_EQ(str1.Length(), 11);

	str1.Clear();

	EXPECT_EQ(str1.Length(), 0);
	EXPECT_STREQ(str1.AsCStr(), "");
	EXPECT_TRUE(str1.IsNullTerminating());
}

TEST(String, Append_AddsToEnd)
{
	String32 str1("Hello");

	str1.Append(" World");

	EXPECT_EQ(str1.Length(), 11);
	EXPECT_STREQ(str1.AsCStr(), "Hello World");

	str1.Append('!');

	EXPECT_EQ(str1.Length(), 12);
	EXPECT_STREQ(str1.AsCStr(), "Hello World!");
}

TEST(String, AppendOperator_AddsToEnd)
{
	String32 str1("Hello");

	str1 += " World";

	EXPECT_EQ(str1.Length(), 11);
	EXPECT_STREQ(str1.AsCStr(), "Hello World");

	String32 str2("!");
	str1 += str2;

	EXPECT_EQ(str1.Length(), 12);
	EXPECT_STREQ(str1.AsCStr(), "Hello World!");
}

TEST(String, AdditionOperator_CreatesNewString)
{
	String32 str1("Hello");
	String32 str2(" World");

	String32 str3((str1 + str2).AsCStr());

	EXPECT_EQ(str3.Length(), 11);
	EXPECT_STREQ(str3.AsCStr(), "Hello World");
	EXPECT_STREQ(str1.AsCStr(), "Hello");
	EXPECT_STREQ(str2.AsCStr(), " World");

	String32 str4((str1 + " Friend").AsCStr());

	EXPECT_EQ(str4.Length(), 12);
	EXPECT_STREQ(str4.AsCStr(), "Hello Friend");
}

TEST(String, Insert_InsertsAtPosition)
{
	String32 str1("Hello World");

	str1.Insert(5, "!!!");

	EXPECT_EQ(str1.Length(), 14);
	EXPECT_STREQ(str1.AsCStr(), "Hello!!! World");

	String32 str2("Test");
	str1.Insert(0, str2.AsCStr());

	EXPECT_STREQ(str1.AsCStr(), "TestHello!!! World");
}

TEST(String, Remove_RemovesRange)
{
	String32 str1("Hello World");

	str1.Remove(5, 6);

	EXPECT_EQ(str1.Length(), 5);
	EXPECT_STREQ(str1.AsCStr(), "Hello");
}

TEST(String, Find_LocatesSubstring)
{
	String32 str1("Hello World Hello");

	EXPECT_EQ(str1.Find("World"), 6);
	EXPECT_EQ(str1.Find("Hello"), 0);
	EXPECT_EQ(str1.Find("Goodbye"), -1);

	EXPECT_EQ(str1.Find('o'), 4);
	EXPECT_EQ(str1.Find('z'), -1);

	EXPECT_EQ(str1.FindLast("Hello"), 12);
	EXPECT_EQ(str1.FindLast('o'), 16);
}

TEST(String, Replace_ReplacesSubstring)
{
	String64 str1("Hello World Hello World");

	str1.Replace("World", "Friend");

	EXPECT_STREQ(str1.AsCStr(), "Hello Friend Hello Friend");

	String32 str2("aaabbbccc");
	str2.Replace("b", "x");

	EXPECT_STREQ(str2.AsCStr(), "aaaxxxccc");
}

TEST(String, Substring_ExtractsSubstring)
{
	String32 str1("Hello World");

	String32 str2(str1.Substring(6, 5).AsCStr());

	EXPECT_EQ(str2.Length(), 5);
	EXPECT_STREQ(str2.AsCStr(), "World");

	String32 str3(str1.Substring(0, 5).AsCStr());

	EXPECT_STREQ(str3.AsCStr(), "Hello");
}

TEST(String, ToLowerUpper_ChangesCase)
{
	String32 str1("Hello World");

	str1.ToLower();

	EXPECT_STREQ(str1.AsCStr(), "hello world");

	str1.ToUpper();

	EXPECT_STREQ(str1.AsCStr(), "HELLO WORLD");
}

TEST(String, Trim_RemovesWhitespace)
{
	String32 str1("  Hello World  ");

	str1.TrimLeft();

	EXPECT_STREQ(str1.AsCStr(), "Hello World  ");

	str1 = "  Hello World  ";
	str1.TrimRight();

	EXPECT_STREQ(str1.AsCStr(), "  Hello World");

	str1 = "  Hello World  ";
	str1.Trim();

	EXPECT_STREQ(str1.AsCStr(), "Hello World");
}

TEST(String, StartsWith_ChecksPrefix)
{
	String32 str1("Hello World");

	EXPECT_TRUE(str1.StartsWith("Hello"));
	EXPECT_TRUE(str1.StartsWith("H"));
	EXPECT_FALSE(str1.StartsWith("World"));
	EXPECT_FALSE(str1.StartsWith("Goodbye"));
}

TEST(String, EndsWith_ChecksSuffix)
{
	String32 str1("Hello World");

	EXPECT_TRUE(str1.EndsWith("World"));
	EXPECT_TRUE(str1.EndsWith("d"));
	EXPECT_FALSE(str1.EndsWith("Hello"));
	EXPECT_FALSE(str1.EndsWith("Goodbye"));
}

TEST(String, IsEmpty_ChecksIfEmpty)
{
	String32 str1;

	EXPECT_TRUE(str1.IsEmpty());

	str1 = "Hello";

	EXPECT_FALSE(str1.IsEmpty());

	str1.Clear();

	EXPECT_TRUE(str1.IsEmpty());
}

TEST(String, Format_FormatsString)
{
	String64 str1;

	str1.Format("Hello %s, you are %d years old", "World", 25);

	EXPECT_STREQ(str1.AsCStr(), "Hello World, you are 25 years old");

	String32 str2;
	str2.Format("Pi is approximately %.2f", 3.14159);

	EXPECT_STREQ(str2.AsCStr(), "Pi is approximately 3.14");
}

TEST(String, Split_SplitsIntoTokens)
{
	String64 str1("Hello,World,Test");

	DynamicArrayC<String32, 10> tokens;
	str1.Split(',', tokens);

	EXPECT_EQ(tokens.Size(), 3);
	EXPECT_STREQ(tokens[0].AsCStr(), "Hello");
	EXPECT_STREQ(tokens[1].AsCStr(), "World");
	EXPECT_STREQ(tokens[2].AsCStr(), "Test");
}

TEST(String, Contains_ChecksForSubstring)
{
	String32 str1("Hello World");

	EXPECT_TRUE(str1.Contains("World"));
	EXPECT_TRUE(str1.Contains("Hello"));
	EXPECT_TRUE(str1.Contains("o"));
	EXPECT_FALSE(str1.Contains("Goodbye"));
	EXPECT_FALSE(str1.Contains("z"));
}

TEST(String, Reverse_ReversesString)
{
	String32 str1("Hello");

	str1.Reverse();

	EXPECT_STREQ(str1.AsCStr(), "olleH");

	String32 str2("12345");
	str2.Reverse();

	EXPECT_STREQ(str2.AsCStr(), "54321");
}

TEST(String, TruncationBehavior_LimitsToCapacity)
{
	String8 str8;

	str8 = "This is a very long string that exceeds capacity";

	EXPECT_EQ(str8.Length(), 7);
	EXPECT_EQ(str8.Size(), 8);
	EXPECT_TRUE(str8.IsNullTerminating());

	String32 str32;
	str32 = "This string fits within 32 characters";

	EXPECT_TRUE(str32.Length() < 32);
	EXPECT_TRUE(str32.IsNullTerminating());
}

TEST(String, DifferentSizes_WorkIndependently)
{
	String8 str8("Short");
	String32 str32("This is a medium length string");
	String64 str64("This is a much longer string that can fit in 64 characters");
	String128 str128("This is an even longer string that requires 128 characters of storage space to hold properly");
	String1024 str1024("This is an extremely long string that might be used for large text buffers or file contents");

	EXPECT_TRUE(str8.Length() <= 7);
	EXPECT_EQ(str8.Size(), 8);

	EXPECT_TRUE(str32.Length() <= 31);
	EXPECT_EQ(str32.Size(), 32);

	EXPECT_TRUE(str64.Length() <= 63);
	EXPECT_EQ(str64.Size(), 64);

	EXPECT_TRUE(str128.Length() <= 127);
	EXPECT_EQ(str128.Size(), 128);

	EXPECT_TRUE(str1024.Length() <= 1023);
	EXPECT_EQ(str1024.Size(), 1024);
}
