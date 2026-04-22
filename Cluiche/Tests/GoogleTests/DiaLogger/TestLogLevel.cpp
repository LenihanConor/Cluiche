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
