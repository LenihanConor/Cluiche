#pragma once

// DescribeSession.h: the Main-facing half of describe mode (no JSON / schema dependencies, so an app's Main can include
// it without extra include paths). See DescribeMode.h for the design note.

#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String512.h>

namespace Dia::ValueSchema
{
	namespace DescribeExit
	{
		constexpr int kOk = 0;
		constexpr int kWriteFailed = 2;
		constexpr int kAborted = 3;
		constexpr int kNotRun = 4;
		constexpr int kTimedOut = 5;
	}

	struct DescribeRequest
	{
		bool									mActive = false;
		Dia::Core::Containers::String512		mPath;
		Dia::Core::Containers::String64			mStage;		// empty = the initial stage
		Dia::Core::Containers::String64			mApp;		// header.app
		Dia::Core::Containers::String64			mConfig;	// header.config
	};

	// Scans argv for `--describe-schema <path>` and `--stage <name>` (--stage alone is ignored). Returns false on a
	// malformed use (missing/overlong value); `out` is left inactive then. Absent flag: true with mActive == false.
	bool ParseDescribeArgs(int argc, const char* const argv[], DescribeRequest& out);

	// Main's one-call entry: parses argv, stamps header app/config (config from the build flavour) and, when the flag is
	// present, DescribeSession::Begin()s the request. False on malformed arguments (message already printed; Main returns 1).
	bool BeginDescribeFromArgs(int argc, const char* const argv[], const char* appName);


	// Process-level handoff between an app's Main and ValueSchemaModule (see design note).
	class DescribeSession
	{
	public:
		static void					Begin(const DescribeRequest& request);
		static bool					IsActive();
		static const DescribeRequest& GetRequest();
		static void					Complete(int exitCode);
		static bool					IsComplete();
		// 0 when inactive; the recorded code when complete; DescribeExit::kNotRun (with a printf message) when active and never completed.
		static int					FinalExitCode();
		static void					Reset();	// tests
	};
}
