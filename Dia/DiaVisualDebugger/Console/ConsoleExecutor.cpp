////////////////////////////////////////////////////////////////////////////////
// Filename: ConsoleExecutor.cpp
// Description: Console line -> ParsedCommand -> Dia::API::ExecuteCommand.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/ConsoleExecutor.h>

#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			ConsoleResult ExecuteConsoleCommand(const char* text)
			{
				ConsoleResult result;
				result.exitCode = kConsoleExitOk;
				result.parseError = ParseError::None;

				ParsedCommand command;
				const ParseError error = ParseCommandLine(text, command);
				if (error != ParseError::None)
				{
					result.exitCode = kConsoleExitBadArguments;
					result.parseError = error;
					DIA_LOG_INFO("Debug", "Console: parse failed (%s)", ParseErrorToString(error));
					return result;
				}

				result.exitCode = Dia::API::ExecuteCommand(command.name, command.args);
				if (result.exitCode != kConsoleExitOk)
				{
					DIA_LOG_INFO("Debug", "Console: '%s' exited with %d", command.name.AsChar(), result.exitCode);
				}
				return result;
			}
		}
	}
}
