////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleExecutor.h
// Description: Parses a console line and executes it through the DiaAPI
//              command registry, returning a typed result.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaVisualDebugger/Console/CommandLineParser.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			static const int kConsoleExitOk = 0;
			static const int kConsoleExitBadArguments = 2;
			static const int kConsoleExitNotFound = 3;

			struct ConsoleResult
			{
				int exitCode;				// 0 ok, 3 not found, 2 bad arguments, else command-specific
				ParseError parseError;		// None unless the line failed to parse (exitCode is then 2)
			};

			////////////////////////////////////////////////////////////////////////////////
			// Parse text with ParseCommandLine and run it with Dia::API::ExecuteCommand.
			//   - Parse error: returns { 2, error } and does not execute anything.
			//   - Unknown command: returns { 3, None }.
			//   - Otherwise: returns the command's exit code.
			// Must be called on the thread that is allowed to run commands (Sim PU in
			// the baseline); it follows the registry's threading rules.
			////////////////////////////////////////////////////////////////////////////////
			ConsoleResult ExecuteConsoleCommand(const char* text);
		}
	}
}
