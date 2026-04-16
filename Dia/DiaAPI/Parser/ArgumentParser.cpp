////////////////////////////////////////////////////////////////////////////////
// Filename: ArgumentParser.cpp
// Description: Command-line argument parser implementation
// Feature spec: docs/specs/features/dia/diacli/cli-parser.md
////////////////////////////////////////////////////////////////////////////////
#include "ArgumentParser.h"
#include <DiaCore/Core/Log.h>
#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace CLI
	{
		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Internal parser state
			////////////////////////////////////////////////////////////////////////////////
			struct ParserState
			{
				std::unordered_map<unsigned int, unsigned int> shortFlagAliases;
				// Key: StringCRC("-v").Value(), Value: StringCRC("verbose").Value()
			};

			ParserState gParserState;

			////////////////////////////////////////////////////////////////////////////////
			// Check if string starts with prefix
			////////////////////////////////////////////////////////////////////////////////
			bool StartsWith(const char* str, const char* prefix)
			{
				if (!str || !prefix)
				{
					return false;
				}

				while (*prefix != '\0')
				{
					if (*str != *prefix)
					{
						return false;
					}
					str++;
					prefix++;
				}

				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Find '=' character in string
			////////////////////////////////////////////////////////////////////////////////
			const char* FindEquals(const char* str)
			{
				if (!str)
				{
					return nullptr;
				}

				while (*str != '\0')
				{
					if (*str == '=')
					{
						return str;
					}
					str++;
				}

				return nullptr;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Extract substring before '=' (allocates static buffer - NOT thread-safe)
			////////////////////////////////////////////////////////////////////////////////
			const char* ExtractKey(const char* str, const char* equalsPos)
			{
				static char keyBuffer[256];
				size_t length = equalsPos - str;

				if (length >= 256)
				{
					length = 255;
				}

				for (size_t i = 0; i < length; i++)
				{
					keyBuffer[i] = str[i];
				}
				keyBuffer[length] = '\0';

				return keyBuffer;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse named argument (--key=value)
			////////////////////////////////////////////////////////////////////////////////
			bool ParseNamedArg(const char* arg, ParseResult& result)
			{
				// Find '=' separator
				const char* equalsPos = FindEquals(arg);
				if (!equalsPos)
				{
					// Malformed: --key without =value
					result.errorCode = 2;
					result.errorMessage = "Invalid named argument format (expected --key=value)";
					Dia::Core::Log::OutputVaradicLine("DiaCLI WARNING: Invalid named argument format: %s (expected --key=value)", arg);
					return false;
				}

				// Extract key (skip "--" prefix)
				const char* key = ExtractKey(arg + 2, equalsPos);
				const char* value = equalsPos + 1;  // Value starts after '='

				// Add to named args (using std::unordered_map)
				result.args.namedArgs[Dia::Core::StringCRC(key).Value()] = value;
				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse flag (--flag)
			////////////////////////////////////////////////////////////////////////////////
			void ParseFlag(const char* arg, ParseResult& result)
			{
				// Skip "--" prefix
				const char* flagName = arg + 2;
				result.args.flags[Dia::Core::StringCRC(flagName).Value()] = true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse short flag (-v)
			////////////////////////////////////////////////////////////////////////////////
			bool ParseShortFlag(const char* arg, ParseResult& result)
			{
				Dia::Core::StringCRC shortFlagCRC(arg);

				// Look up alias
				auto it = gParserState.shortFlagAliases.find(shortFlagCRC.Value());
				if (it == gParserState.shortFlagAliases.end())
				{
					// Unknown short flag
					result.errorCode = 2;
					result.errorMessage = "Unknown short flag";
					Dia::Core::Log::OutputVaradicLine("DiaCLI WARNING: Unknown short flag: %s", arg);
					return false;
				}

				// Add long flag to flags (it->second is the long flag StringCRC value)
				result.args.flags[it->second] = true;
				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Initialize short flag alias map (called during DiaCLI::Initialize)
			////////////////////////////////////////////////////////////////////////////////
			void InitializeShortFlagAliases()
			{
				// Register built-in short flags
				gParserState.shortFlagAliases[Dia::Core::StringCRC("-h").Value()] = Dia::Core::StringCRC("help").Value();
				gParserState.shortFlagAliases[Dia::Core::StringCRC("-v").Value()] = Dia::Core::StringCRC("verbose").Value();
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Parse command-line arguments
		////////////////////////////////////////////////////////////////////////////////
		void ParseArguments(int argc, char* argv[], ParseResult& result)
		{
			using namespace Internal;

			result.errorCode = 0;
			result.errorMessage = nullptr;

			// Initialize short flag aliases if not already done
			if (gParserState.shortFlagAliases.empty())
			{
				InitializeShortFlagAliases();
			}


			// Check for minimum arguments (argv[0] = executable, argv[1] = command)
			if (argc < 2)
			{
				result.errorCode = 2;
				result.errorMessage = "No command specified";
				Dia::Core::Log::OutputLine("DiaCLI WARNING: No command specified");
				return;
			}

			// Extract command name from argv[1]
			const char* commandArg = argv[1];

			// Special case: --help with no command
			if (std::strcmp(commandArg, "--help") == 0)
			{
				// Empty command, set help flag
				result.commandName = Dia::Core::StringCRC("");
				result.args.flags[Dia::Core::StringCRC("help").Value()] = true;
				return;
			}

			// Check if command name starts with "-" (invalid)
			if (commandArg[0] == '-')
			{
				result.errorCode = 2;
				result.errorMessage = "Command name cannot start with '-'";
				Dia::Core::Log::OutputVaradicLine("DiaCLI WARNING: Command name cannot start with '-': %s", commandArg);
				return;
			}

			// Valid command name
			result.commandName = Dia::Core::StringCRC(commandArg);

			// Parse remaining arguments
			bool endOfFlags = false;

			for (int i = 2; i < argc; i++)
			{
				const char* arg = argv[i];

				// Check for end-of-flags marker
				if (std::strcmp(arg, "--") == 0)
				{
					endOfFlags = true;
					continue;
				}

				// After --, everything is positional
				if (endOfFlags)
				{
					result.args.positionalArgs.Add(arg);
					continue;
				}

				// Check for named arg (--key=value)
				if (StartsWith(arg, "--"))
				{
					// Validate no triple-dash
					if (arg[2] == '-')
					{
						result.errorCode = 2;
						result.errorMessage = "Invalid flag format (too many dashes)";
						Dia::Core::Log::OutputVaradicLine("DiaCLI WARNING: Invalid flag format: %s (too many dashes)", arg);
						return;
					}

					// Check if it's --key=value or --flag
					if (FindEquals(arg))
					{
						if (!ParseNamedArg(arg, result))
						{
							return;  // Error already set
						}
					}
					else
					{
						ParseFlag(arg, result);
					}
				}
				// Check for short flag (-v)
				else if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-')
				{
					if (!ParseShortFlag(arg, result))
					{
						return;  // Error already set
					}
				}
				// Positional argument
				else
				{
					result.args.positionalArgs.Add(arg);
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Register short flag alias
		////////////////////////////////////////////////////////////////////////////////
		void RegisterShortFlag(const char* shortFlag, const char* longFlag)
		{
			using namespace Internal;

			// Initialize if not already
			if (gParserState.shortFlagAliases.empty())
			{
				InitializeShortFlagAliases();
			}

			gParserState.shortFlagAliases[Dia::Core::StringCRC(shortFlag).Value()] = Dia::Core::StringCRC(longFlag).Value();
		}
	}
}
