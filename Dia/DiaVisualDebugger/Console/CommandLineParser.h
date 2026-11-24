////////////////////////////////////////////////////////////////////////////////
// Filename: CommandLineParser.h
// Description: Tokenizes a console line and parses it with the DiaAPI
//              ArgumentParser into a command name and CommandArgs.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaAPI/CommandRegistry/CommandRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			////////////////////////////////////////////////////////////////////////////////
			// Limits
			////////////////////////////////////////////////////////////////////////////////
			static const unsigned int kMaxCommandLineLength = 240;	// characters, excluding '\0'
			static const unsigned int kMaxCommandLineTokens = 32;	// tokens after the command name

			////////////////////////////////////////////////////////////////////////////////
			// Typed parse error
			//   InvalidArgument: the line tokenized but ArgumentParser rejected it
			//   (e.g. "---x", unknown short flag, name starting with '-').
			////////////////////////////////////////////////////////////////////////////////
			enum class ParseError
			{
				None,
				Empty,
				TooLong,
				UnterminatedQuote,
				TooManyArgs,
				InvalidArgument
			};

			const char* ParseErrorToString(ParseError error);

			////////////////////////////////////////////////////////////////////////////////
			// Parsed command
			//
			// args points into storage, so a ParsedCommand must not be copied or moved
			// after parsing. Copy and assignment are deleted to enforce this.
			////////////////////////////////////////////////////////////////////////////////
			struct ParsedCommand
			{
				static const unsigned int kStorageSize = 512;

				ParsedCommand();
				ParsedCommand(const ParsedCommand&) = delete;
				ParsedCommand& operator=(const ParsedCommand&) = delete;

				Dia::Core::StringCRC name;
				Dia::API::CommandArgs args;
				char storage[kStorageSize];		// owns the token characters args points into
			};

			////////////////////////////////////////////////////////////////////////////////
			// Parse a console line.
			//
			// Grammar:
			//   - Tokens are separated by spaces (and tabs).
			//   - Double quotes group a token; quotes are stripped; no backslash escapes.
			//   - First token is the command name.
			//   - Remaining tokens follow the DiaCLI grammar (ArgumentParser):
			//     --flag, --key=value, -v, "--" ends flags, anything else is positional.
			//   - Console shorthand: an unquoted token "key=value" (not starting with
			//     '-', non-empty key, before any "--") is passed on as --key=value.
			//
			// out is reset on every call. Never crashes on any input (nullptr = Empty).
			////////////////////////////////////////////////////////////////////////////////
			ParseError ParseCommandLine(const char* text, ParsedCommand& out);
		}
	}
}
