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
		// Format the "Arguments:" section for a command's ValueParam list.
		//
		// Writes a NUL-terminated, newline-terminated block into out (truncated to
		// capacity-1 chars). Required params print as <name>, optional as [name];
		// then type, a detail column (enum values, or unit and/or range), then the
		// description. Columns are padded to the widest entry; trailing whitespace
		// is trimmed. Null name or type prints "?". Detail text is capped at 255 chars.
		//
		// Returns the full length the section needs (snprintf-style, excluding NUL);
		// 0 and an empty out when params is null or count is 0.
		////////////////////////////////////////////////////////////////////////////////
		unsigned FormatArgumentsSection(const ValueParam* params, unsigned count, char* out, unsigned capacity);

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
