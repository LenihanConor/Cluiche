// ===================================================================
// Assert.cpp
// Implementation of the Dia assertion system with call stack capture
// ===================================================================

#include "DiaCore/Core/Assert.h"

#include <stdio.h>
#include <assert.h>

#include "DiaCore/Core/Log.h"
#include "DiaCore/Core/CallStack.h"
#include "DiaCore/Strings/String1024.h"

// Custom StackWalker for assertion failures
// Captures and logs the call stack when an assertion triggers
class AssetStackWalker : public Dia::Core::StackWalker
{
public:
	AssetStackWalker() : StackWalker() {}
	AssetStackWalker(unsigned long dwProcessId, void* hProcess) : Dia::Core::StackWalker(dwProcessId, hProcess) {}

	// Override StackWalker callbacks to suppress verbose module loading info
	virtual void OnSymInit(const char* szSearchPath, unsigned long symOptions, const char* szUserName){}
	virtual void OnDbgHelpErr(const char* szFuncName, unsigned long gle, unsigned long long addr){}
	virtual void OnLoadModule(const char* img, const char* mod, unsigned long long baseAddr, unsigned long size, unsigned long result, const char* symType, const char* pdbName, unsigned long long fileVersion){}

	// Output call stack entries to the log
	virtual void OnOutput(const char* szText)
	{
		Dia::Core::Log::Output(szText);
	}
};

// Display the current call stack
static void DisplayStack()
{
	AssetStackWalker sw;
	sw.ShowCallstack();
}

namespace Dia
{
	namespace Core
	{
		// Global assert function pointer (can be overridden for custom behavior)
		DIA_ASSERT_FUNC g_pAssertFunc = AssertDefault;

#ifdef DEBUG

		// Trigger a debugger breakpoint
		// Causes the debugger to pause execution at this point
		void BREAKPOINT()
		{
			__debugbreak();
		}

		// Default assertion handler
		// Logs the assertion failure, captures call stack, and breaks into debugger
		void AssertDefault(char *pExp, char *pFileName, int iLineNumber, const char* pStr, ...)
		{
			// Format the user's message with printf-style arguments
			va_list va;
			va_start( va, pStr );

			Dia::Core::Containers::String1024 newsStr;

			if( pStr )
			{
				newsStr.Format(pStr, va);
			}

			va_end( va );

			// Log the assertion details
			char str[1024];
			sprintf_s(str, "\nDIA_ASSERTION FAULT: %s\n%s\n%s(%d)", pExp, newsStr.AsCStr(), pFileName, iLineNumber);
			Dia::Core::Log::OutputLine(str);

			// Capture and display the call stack
			Dia::Core::Log::OutputLine("\nCALLSTACK:\n");

			DisplayStack();

			// Break into debugger for investigation
			BREAKPOINT();
		}
#else
		// Release builds: no-op implementations (assertions compile out)
		void BREAKPOINT()
		{}

		void AssertDefault(char *pExp,  char *pFileName, int iLineNumber, ...)
		{}
#endif
	}
}