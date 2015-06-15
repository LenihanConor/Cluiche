#include "DiaCore/Core/Assert.h"

#include <stdio.h>
#include <assert.h>

#include "DiaCore/Core/Log.h"
#include "DiaCore/Core/CallStack.h"
#include "DiaCore/Strings/String1024.h"

class AssetStackWalker : public Dia::Core::StackWalker
{
public:
	AssetStackWalker() : StackWalker() {}
	AssetStackWalker(unsigned long dwProcessId, void* hProcess) : Dia::Core::StackWalker(dwProcessId, hProcess) {}
	virtual void OnSymInit(const char* szSearchPath, unsigned long symOptions, const char* szUserName){}
	virtual void OnDbgHelpErr(const char* szFuncName, unsigned long gle, unsigned long long addr){}
	virtual void OnLoadModule(const char* img, const char* mod, unsigned long long baseAddr, unsigned long size, unsigned long result, const char* symType, const char* pdbName, unsigned long long fileVersion){}
	virtual void OnOutput(const char* szText) 
	{ 
		Dia::Core::Log::Output(szText);
	}
};

static void DisplayStack()
{
	AssetStackWalker sw; 
	sw.ShowCallstack();
}

namespace Dia
{
	namespace Core
	{
		DIA_ASSERT_FUNC g_pAssertFunc = AssertDefault;

#ifdef DEBUG
	
		void BREAKPOINT()
		{
			__debugbreak();
		}

		void AssertDefault(char *pExp, char *pFileName, int iLineNumber, const char* pStr, ...)
		{ 
			va_list va;
			va_start( va, pStr );
			
			Dia::Core::Containers::String1024 newsStr;

			if( pStr )
			{
				newsStr.Format(pStr, va);
			}

			va_end( va );

			char str[1024];
			sprintf_s(str, "\nDIA_ASSERTION FAULT: %s\n%s\n%s(%d)", pExp, newsStr.AsCStr(), pFileName, iLineNumber); 
			Dia::Core::Log::OutputLine(str); 

			Dia::Core::Log::OutputLine("\nCALLSTACK:\n"); 

			DisplayStack();

			BREAKPOINT(); 
		}
#else

		void BREAKPOINT()
		{}
	
		void AssertDefault(char *pExp,  char *pFileName, int iLineNumber, ...)
		{}
#endif
	}
}