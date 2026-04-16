////////////////////////////////////////////////////////////////////////////////
// Filename: ArgumentParser.h
// Description: Command-line argument parser for DiaCLI
// Feature spec: docs/specs/features/dia/diacli/cli-parser.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCLI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace CLI
	{
		////////////////////////////////////////////////////////////////////////////////
		// Parse result structure
		////////////////////////////////////////////////////////////////////////////////
		struct ParseResult
		{
			Dia::Core::StringCRC commandName;  // Extracted from argv[1]
			CommandArgs args;                   // Parsed arguments
			int errorCode;                      // 0 = success, 2 = invalid arguments
			const char* errorMessage;           // Human-readable error (nullptr on success)
		};

		////////////////////////////////////////////////////////////////////////////////
		// Parse command-line arguments
		// Populates result with commandName, args, and error status
		//
		// Arguments:
		//   argc - Argument count (from main)
		//   argv - Argument vector (from main)
		//   result - Output parameter to populate
		//
		// Note: CommandArgs contains pointers into argv - caller must keep argv alive
		////////////////////////////////////////////////////////////////////////////////
		void ParseArguments(int argc, char* argv[], ParseResult& result);

		////////////////////////////////////////////////////////////////////////////////
		// Register short flag alias (e.g., -v → --verbose)
		//
		// Arguments:
		//   shortFlag - Short form with single dash (e.g., "-v")
		//   longFlag - Long form without dashes (e.g., "verbose")
		//
		// Built-in aliases registered during Initialize():
		//   -h → help
		//   -v → verbose
		////////////////////////////////////////////////////////////////////////////////
		void RegisterShortFlag(const char* shortFlag, const char* longFlag);
	}
}
