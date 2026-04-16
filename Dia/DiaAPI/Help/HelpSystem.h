////////////////////////////////////////////////////////////////////////////////
// Filename: HelpSystem.h
// Description: Auto-generated help system for DiaAPI
// Feature spec: docs/specs/features/dia/diacli/help-system.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace API
	{
		////////////////////////////////////////////////////////////////////////////////
		// Show global help (all commands, grouped by category)
		//
		// Queries command registry, groups commands by category, and displays
		// formatted help text to stdout.
		//
		// Returns:
		//   0 - Success (help is informational, not error)
		////////////////////////////////////////////////////////////////////////////////
		int ShowGlobalHelp();

		////////////////////////////////////////////////////////////////////////////////
		// Show help for specific command
		//
		// Arguments:
		//   commandName - Command to show help for
		//
		// Returns:
		//   0 - Success (help displayed)
		//   3 - Command not found
		////////////////////////////////////////////////////////////////////////////////
		int ShowCommandHelp(const Dia::Core::StringCRC& commandName);

		////////////////////////////////////////////////////////////////////////////////
		// Check if help was requested in parsed arguments
		//
		// Arguments:
		//   args - Parsed command arguments
		//   commandName - Command name (may be empty)
		//
		// Returns:
		//   true if --help flag is present
		////////////////////////////////////////////////////////////////////////////////
		bool IsHelpRequested(const CommandArgs& args, const Dia::Core::StringCRC& commandName);
	}
}
