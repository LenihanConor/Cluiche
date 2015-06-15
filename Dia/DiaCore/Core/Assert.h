#ifndef DIA_ASSERT_H
#define DIA_ASSERT_H

#include "DiaCore/Core/System.h"

namespace Dia
{
	namespace Core
	{
		typedef void (*DIA_ASSERT_FUNC)(char *pExp, char *pFileName, int iLineNumber, const char* pStr, ...); 
		
		void BREAKPOINT(); 

		extern DIA_ASSERT_FUNC g_pAssertFunc;

		void AssertDefault(char *pExp, char *pFileName, int iLineNumber, const char* pStr, ...);
	
		template<int> struct CompileTimeError;
		template<> struct CompileTimeError<true> {};
	}
}

#ifdef DEBUG	
	#define DIA_IMPLIES( a, b ) ( (!(a)) || (!(!(b))) )

	#define DIA_ASSERT_SUPPORT(exp) exp
	#define DIA_ASSERT(exp, ...) do { if(!(exp)){ Dia::Core::g_pAssertFunc(#exp, __FILE__, __LINE__, __VA_ARGS__);}} while (0)	
	#define DIA_ASSERT_ONCE(exp, ...) do {static bool done = false; if (!done) {DIA_ASSERT(exp, __VA_ARGS__); done = true;} } while (0)
	#define DIA_ASSERT_STATIC(expr, msg) {Dia::Core::CompileTimeError<expr> msg; UNUSED_PARAM(msg);}

	#define DIA_ASSERT_CONDITIONAL(condition, exp, ...) do { if(!(DIA_IMPLIES(condition, exp))){ Dia::Core::g_pAssertFunc(#exp, __FILE__, __LINE__, __VA_ARGS__);}} while (0)	
	#define DIA_ASSERT_ONCE_CONDITIONAL(condition, exp, ...) do {static bool done = false; if (!done) {DIA_ASSERT_CONDITIONAL(exp, __VA_ARGS__); done = true;} } while (0)

#else

	#define DIA_ASSERT_SUPPORT(exp) 
	#define DIA_ASSERT(exp, ...)
	#define DIA_ASSERT_ONCE(a, s)
	#define DIA_ASSERT_STATIC

	#define DIA_ASSERT_CONDITIONAL
	#define DIA_ASSERT_ONCE_CONDITIONAL

#endif	// DEBUG

#define RELEASE_DIA_ASSERT(exp, str)  do { if(!(exp)) { theConsole.WriteMessage("\nDIA_ASSERTION FAULT: %s %s\nFILE: %s\nLINE: %d\n\n", #exp, str, __FILE__, __LINE__); REALBREAKPOINT(); } else 0; } while (0)

#endif // DIA_ASSERT