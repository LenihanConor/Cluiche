	
#include "UnitTestMacros.h"

#include "UnitTests/Infrastructure/UnitTestManager.h"

#include <DiaCore/Core/Log.h>
#include <DiaCore/Strings/String1024.h>

namespace UnitTests
{
	void UnitTestAsserts::AssertDefaultUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...)
	{
		va_list va;
		va_start( va, pStr );

		Dia::Core::Containers::String1024 newsStr;

		if( pStr )
		{
			newsStr.Format(pStr, va);
		}

		va_end( va );

		UnitTestManager::GetInstance().CurrentUnitTest()->RecordFail(pExp, newsStr.AsCStr(), pFileName, iLineNumber);
	}

	void UnitTestAsserts::AssertExpectedUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...)
	{
		va_list va;
		va_start( va, pStr );

		Dia::Core::Containers::String1024 newsStr;

		if( pStr )
		{
			newsStr.Format(pStr, va);
		}

		va_end( va );

		ThrowAssertData rockToBeThrown;
		rockToBeThrown.mExpression.Append(pExp, strlen(pExp));
		rockToBeThrown.mString.Append(newsStr.AsCStr(), newsStr.Length());
		rockToBeThrown.mFilename.Append(pFileName, strlen(pFileName));
		rockToBeThrown.mLineNumber = iLineNumber;

		throw rockToBeThrown;
	}

	void UnitTestAsserts::AssertUnExpectedUnitTest(char *pExp, char *pFileName, int iLineNumber, const char *pStr, ...)
	{
		va_list va;
		va_start( va, pStr );

		Dia::Core::Containers::String1024 newsStr;

		if( pStr )
		{
			newsStr.Format(pStr, va);
		}

		va_end( va );

		ThrowAssertData rockToBeThrown;
		rockToBeThrown.mExpression.Append(pExp, strlen(pExp));
		rockToBeThrown.mString.Append(newsStr.AsCStr(), newsStr.Length());
		rockToBeThrown.mFilename.Append(pFileName, strlen(pFileName));
		rockToBeThrown.mLineNumber = iLineNumber;

		throw rockToBeThrown;
	}
}
