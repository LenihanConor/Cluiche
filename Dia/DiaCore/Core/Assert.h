#ifndef DIA_ASSERT_H
#define DIA_ASSERT_H

#include "DiaCore/Core/System.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Assertion System
		//
		// Provides runtime and compile-time assertion macros for debugging.
		//
		// FEATURES:
		//   - Runtime assertions with printf-style formatting
		//   - Call stack capture on assertion failure
		//   - Conditional assertions (DIA_IMPLIES)
		//   - One-time assertions (DIA_ASSERT_ONCE)
		//   - Compile-time assertions (DIA_ASSERT_STATIC)
		//   - Customizable assert handler function
		//
		// USAGE:
		//   DIA_ASSERT(ptr != nullptr, "Pointer must not be null");
		//   DIA_ASSERT(size > 0, "Size must be %d, got %d", requiredSize, size);
		//   DIA_ASSERT_ONCE(initialized, "System not initialized");
		//   DIA_ASSERT_STATIC(sizeof(int) == 4, IntSizeMustBe4Bytes);
		//
		// NOTE: All assertions compile out in Release builds except RELEASE_DIA_ASSERT
		//---------------------------------------------------------------------------------------------------------------------------------

		// Function pointer type for custom assert handlers
		typedef void (*DIA_ASSERT_FUNC)(const char *pExp, const char *pFileName, int iLineNumber, const char* pStr, ...);

		// Callback type for receiving formatted assert output (e.g. to route to logger sinks)
		typedef void (*AssertOutputCallback)(const char* formattedMessage);

		// Trigger debugger breakpoint
		void BREAKPOINT();

		// Global assert handler (can be replaced with custom implementation)
		extern DIA_ASSERT_FUNC g_pAssertFunc;

		// Default assert handler: logs failure, shows call stack, breaks into debugger
		void AssertDefault(const char *pExp, const char *pFileName, int iLineNumber, const char* pStr, ...);

		// Register/unregister callbacks that receive the formatted assert message.
		// Higher-level systems (e.g. DiaLogger) use this to route asserts to their sinks
		// without introducing a reverse dependency from DiaCore.
		static const unsigned int kMaxAssertOutputCallbacks = 8;
		void RegisterAssertOutputCallback(AssertOutputCallback callback);
		void UnregisterAssertOutputCallback(AssertOutputCallback callback);

		// Compile-time assertion helper (causes compile error if condition is false)
		template<int> struct CompileTimeError;
		template<> struct CompileTimeError<true> {};
	}
}

//=================================================================================================================================
// Assertion Macros (DEBUG builds only, except RELEASE_DIA_ASSERT)
//=================================================================================================================================

#ifdef DEBUG
	// Logical implication: (a implies b) = (!a || b)
	#define DIA_IMPLIES( a, b ) ( (!(a)) || (!(!(b))) )

	// Keep expression in debug builds (for side-effects in asserts)
	#define DIA_ASSERT_SUPPORT(exp) exp

	// Standard assertion: checks condition, triggers assert handler if false
	// Example: DIA_ASSERT(ptr != nullptr, "Pointer is null");
	#define DIA_ASSERT(exp, ...) do { if(!(exp)){ Dia::Core::g_pAssertFunc(#exp, __FILE__, __LINE__, __VA_ARGS__);}} while (0)

	// One-time assertion: only triggers once per execution, then remembers it fired
	// Useful for warnings that should only appear once
	#define DIA_ASSERT_ONCE(exp, ...) do {static bool done = false; if (!done) {DIA_ASSERT(exp, __VA_ARGS__); done = true;} } while (0)

	// Compile-time assertion: causes compile error if expression is false
	// Example: DIA_ASSERT_STATIC(sizeof(int) == 4, IntMustBe4Bytes);
	#define DIA_ASSERT_STATIC(expr, msg) {Dia::Core::CompileTimeError<expr> msg; UNUSED_PARAM(msg);}

	// Conditional assertion: asserts that (condition implies exp)
	// Only checks exp when condition is true
	#define DIA_ASSERT_CONDITIONAL(condition, exp, ...) do { if(!(DIA_IMPLIES(condition, exp))){ Dia::Core::g_pAssertFunc(#exp, __FILE__, __LINE__, __VA_ARGS__);}} while (0)

	// One-time conditional assertion
	#define DIA_ASSERT_ONCE_CONDITIONAL(condition, exp, ...) do {static bool done = false; if (!done) {DIA_ASSERT_CONDITIONAL(exp, __VA_ARGS__); done = true;} } while (0)

#else
	// Release builds: all assertions compile out (zero overhead)
	#define DIA_ASSERT_SUPPORT(exp)
	#define DIA_ASSERT(exp, ...)
	#define DIA_ASSERT_ONCE(a, s)
	#define DIA_ASSERT_STATIC

	#define DIA_ASSERT_CONDITIONAL
	#define DIA_ASSERT_ONCE_CONDITIONAL

#endif	// DEBUG

// Release build assertion: active in BOTH debug and release builds
// Use sparingly for critical runtime checks that must always be present
#define RELEASE_DIA_ASSERT(exp, str)  do { if(!(exp)) { theConsole.WriteMessage("\nDIA_ASSERTION FAULT: %s %s\nFILE: %s\nLINE: %d\n\n", #exp, str, __FILE__, __LINE__); REALBREAKPOINT(); } else 0; } while (0)

#endif // DIA_ASSERT