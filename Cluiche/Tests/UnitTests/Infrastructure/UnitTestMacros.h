#pragma once

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String1024.h>

namespace UnitTests
{
	class UnitTestAsserts
	{
	public:
		static void AssertDefaultUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...);
		static void AssertExpectedUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...);
		static void AssertUnExpectedUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...);
	};

	class ThrowAssertData
	{
	public:

		ThrowAssertData(){};

		Dia::Core::Containers::String256 mExpression;
		Dia::Core::Containers::String256 mString;
		Dia::Core::Containers::String256 mFilename;
		int mLineNumber;
	};

#define UNIT_TEST_POSITIVE(exp, str) if (!(exp)){this->RecordFail(#exp, str, __FILE__, __LINE__);}
#define UNIT_TEST_NEGATIVE(exp, str) if (exp){this->RecordFail(#exp, str, __FILE__, __LINE__);}

#define UNIT_TEST_ASSERT_EXPECTED_START(){\
											Dia::Core::DIA_ASSERT_FUNC temp = Dia::Core::g_pAssertFunc;\
											Dia::Core::g_pAssertFunc = UnitTestAsserts::AssertExpectedUnitTest;\
											bool asserted = false;\
											try\
											{	
											
#define UNIT_TEST_ASSERT_EXPECTED_END()		}\
											catch (ThrowAssertData rockCaught)\
											{\
												asserted = true;\
												rockCaught = rockCaught;\
											}\
											UNIT_TEST_POSITIVE(asserted, "Did Not Assert when expected to");\
										}

#define UNIT_TEST_BLOCK_START()	{\
									Dia::Core::DIA_ASSERT_FUNC temp = Dia::Core::g_pAssertFunc;\
									Dia::Core::g_pAssertFunc = UnitTestAsserts::AssertUnExpectedUnitTest;\
									try\
									{	

#define UNIT_TEST_BLOCK_END()		}\
									catch (ThrowAssertData rockCaught)\
									{\
										Dia::Core::Containers::String1024 str("Assert: %s(%d), %s, %s", rockCaught.mFilename.AsCStr(), rockCaught.mLineNumber, rockCaught.mExpression.AsCStr(), rockCaught.mString.AsCStr());\
										UNIT_TEST_POSITIVE(0, str.AsCStr());\
										rockCaught = rockCaught;\
									}\
								}
}

