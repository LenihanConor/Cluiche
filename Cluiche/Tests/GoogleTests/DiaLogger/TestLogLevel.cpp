#include <gtest/gtest.h>

#include <DiaLogger/LogLevel.h>

using namespace Dia::Logger;

// ==============================================================================
// LogLevel Tests
// ==============================================================================

TEST(TestLogLevel, LogLevelToString_Trace)
{
	EXPECT_STREQ("trace", LogLevelToString(LogLevel::kTrace));
}

TEST(TestLogLevel, LogLevelToString_Debug)
{
	EXPECT_STREQ("debug", LogLevelToString(LogLevel::kDebug));
}

TEST(TestLogLevel, LogLevelToString_Info)
{
	EXPECT_STREQ("info", LogLevelToString(LogLevel::kInfo));
}

TEST(TestLogLevel, LogLevelToString_Warning)
{
	EXPECT_STREQ("warning", LogLevelToString(LogLevel::kWarning));
}

TEST(TestLogLevel, LogLevelToString_Error)
{
	EXPECT_STREQ("error", LogLevelToString(LogLevel::kError));
}

TEST(TestLogLevel, LevelOrdering_TraceIsLowest)
{
	EXPECT_LT(static_cast<unsigned char>(LogLevel::kTrace),
		static_cast<unsigned char>(LogLevel::kDebug));
}

TEST(TestLogLevel, LevelOrdering_DebugLessThanInfo)
{
	EXPECT_LT(static_cast<unsigned char>(LogLevel::kDebug),
		static_cast<unsigned char>(LogLevel::kInfo));
}

TEST(TestLogLevel, LevelOrdering_InfoLessThanWarning)
{
	EXPECT_LT(static_cast<unsigned char>(LogLevel::kInfo),
		static_cast<unsigned char>(LogLevel::kWarning));
}

TEST(TestLogLevel, LevelOrdering_WarningLessThanError)
{
	EXPECT_LT(static_cast<unsigned char>(LogLevel::kWarning),
		static_cast<unsigned char>(LogLevel::kError));
}

// ==============================================================================
// LogLevelFromString Tests
// ==============================================================================

TEST(TestLogLevel, LogLevelFromString_Trace)
{
	EXPECT_EQ(LogLevel::kTrace, LogLevelFromString("trace"));
}

TEST(TestLogLevel, LogLevelFromString_Debug)
{
	EXPECT_EQ(LogLevel::kDebug, LogLevelFromString("debug"));
}

TEST(TestLogLevel, LogLevelFromString_Info)
{
	EXPECT_EQ(LogLevel::kInfo, LogLevelFromString("info"));
}

TEST(TestLogLevel, LogLevelFromString_Warning)
{
	EXPECT_EQ(LogLevel::kWarning, LogLevelFromString("warning"));
}

TEST(TestLogLevel, LogLevelFromString_Error)
{
	EXPECT_EQ(LogLevel::kError, LogLevelFromString("error"));
}

TEST(TestLogLevel, LogLevelFromString_Null_ReturnsDefault)
{
	EXPECT_EQ(LogLevel::kInfo, LogLevelFromString(nullptr));
}

TEST(TestLogLevel, LogLevelFromString_Unknown_ReturnsDefault)
{
	EXPECT_EQ(LogLevel::kInfo, LogLevelFromString("garbage"));
}

TEST(TestLogLevel, LogLevelFromString_CustomDefault)
{
	EXPECT_EQ(LogLevel::kError, LogLevelFromString("garbage", LogLevel::kError));
}

TEST(TestLogLevel, LogLevelFromString_NullWithCustomDefault)
{
	EXPECT_EQ(LogLevel::kTrace, LogLevelFromString(nullptr, LogLevel::kTrace));
}

TEST(TestLogLevel, LogLevelFromString_Roundtrip)
{
	EXPECT_EQ(LogLevel::kTrace, LogLevelFromString(LogLevelToString(LogLevel::kTrace)));
	EXPECT_EQ(LogLevel::kDebug, LogLevelFromString(LogLevelToString(LogLevel::kDebug)));
	EXPECT_EQ(LogLevel::kInfo, LogLevelFromString(LogLevelToString(LogLevel::kInfo)));
	EXPECT_EQ(LogLevel::kWarning, LogLevelFromString(LogLevelToString(LogLevel::kWarning)));
	EXPECT_EQ(LogLevel::kError, LogLevelFromString(LogLevelToString(LogLevel::kError)));
}
