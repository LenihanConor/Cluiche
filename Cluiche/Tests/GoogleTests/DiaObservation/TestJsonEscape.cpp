#include <gtest/gtest.h>
#include <DiaObservation/JsonEscape.h>

#include <cstring>

using Dia::Observation::EscapeJsonString;

TEST(JsonEscape, EmptyString_ProducesEmpty)
{
    char dst[64];
    EscapeJsonString("", dst, sizeof(dst));
    EXPECT_STREQ(dst, "");
}

TEST(JsonEscape, NullSrc_ProducesEmpty)
{
    char dst[64] = "garbage";
    EscapeJsonString(nullptr, dst, sizeof(dst));
    EXPECT_STREQ(dst, "");
}

TEST(JsonEscape, NoSpecialChars_Passthrough)
{
    char dst[64];
    EscapeJsonString("hello world 123", dst, sizeof(dst));
    EXPECT_STREQ(dst, "hello world 123");
}

TEST(JsonEscape, QuotesEscaped)
{
    char dst[64];
    EscapeJsonString("say \"hi\"", dst, sizeof(dst));
    EXPECT_STREQ(dst, "say \\\"hi\\\"");
}

TEST(JsonEscape, BackslashEscaped)
{
    char dst[64];
    EscapeJsonString("a\\b", dst, sizeof(dst));
    EXPECT_STREQ(dst, "a\\\\b");
}

TEST(JsonEscape, NewlineEscaped)
{
    char dst[64];
    EscapeJsonString("line1\nline2", dst, sizeof(dst));
    EXPECT_STREQ(dst, "line1\\nline2");
}

TEST(JsonEscape, CarriageReturnEscaped)
{
    char dst[64];
    EscapeJsonString("a\rb", dst, sizeof(dst));
    EXPECT_STREQ(dst, "a\\rb");
}

TEST(JsonEscape, TabEscaped)
{
    char dst[64];
    EscapeJsonString("a\tb", dst, sizeof(dst));
    EXPECT_STREQ(dst, "a\\tb");
}

TEST(JsonEscape, MixedSpecialChars)
{
    char dst[128];
    EscapeJsonString("\"hello\"\n\\world\t", dst, sizeof(dst));
    EXPECT_STREQ(dst, "\\\"hello\\\"\\n\\\\world\\t");
}

TEST(JsonEscape, BufferTruncation_NullTerminated)
{
    char dst[8];
    EscapeJsonString("abcdefghij", dst, sizeof(dst));
    EXPECT_EQ(strlen(dst), 7u);
    EXPECT_EQ(dst[7], '\0');
}

TEST(JsonEscape, BufferTruncation_EscapeDoesNotOverrun)
{
    // Buffer can hold "ab\" but not "ab\\"
    char dst[5];
    EscapeJsonString("ab\"c", dst, sizeof(dst));
    // Should write "ab\\"" but truncate before the escape pair doesn't fit
    // With 5 bytes: a(1) b(2) \"(3,4) \0 — but "c" doesn't fit
    EXPECT_TRUE(strlen(dst) <= 4);
    EXPECT_EQ(dst[strlen(dst)], '\0');
}

TEST(JsonEscape, ZeroDstSize_NoWrite)
{
    char dst[1] = {'X'};
    EscapeJsonString("hello", dst, 0);
    EXPECT_EQ(dst[0], 'X');
}
