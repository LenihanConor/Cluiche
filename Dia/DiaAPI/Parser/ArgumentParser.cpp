////////////////////////////////////////////////////////////////////////////////
// Filename: ArgumentParser.cpp
// Description: Command-line argument parser implementation
// Feature spec: docs/specs/features/dia/diaapi/cli-parser.md
////////////////////////////////////////////////////////////////////////////////
#include "ArgumentParser.h"
#include <DiaLogger/DiaLog.h>
#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace API
	{
		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Internal parser state
			////////////////////////////////////////////////////////////////////////////////
			struct ShortFlagAlias
			{
				unsigned int shortCRC;
				unsigned int longCRC;
			};

			struct ParserState
			{
				Dia::Core::Containers::DynamicArrayC<ShortFlagAlias, 16> shortFlagAliases;
				bool initialized = false;
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
			// Extract key from --key=value into caller-provided buffer
			////////////////////////////////////////////////////////////////////////////////
			void ExtractKey(const char* str, const char* equalsPos, char* outBuffer, unsigned int bufferSize)
			{
				size_t length = equalsPos - str;

				if (length >= bufferSize)
				{
					length = bufferSize - 1;
				}

				for (size_t i = 0; i < length; i++)
				{
					outBuffer[i] = str[i];
				}
				outBuffer[length] = '\0';
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse named argument (--key=value)
			////////////////////////////////////////////////////////////////////////////////
			bool ParseNamedArg(const char* arg, ParseResult& result)
			{
				const char* equalsPos = FindEquals(arg);
				if (!equalsPos)
				{
					result.errorCode = 2;
					result.errorMessage = "Invalid named argument format (expected --key=value)";
					DIA_LOG_ERROR("API", "Invalid named argument format: %s (expected --key=value)", arg);
					return false;
				}

				char keyBuffer[256];
				ExtractKey(arg + 2, equalsPos, keyBuffer, 256);
				const char* value = equalsPos + 1;

				result.args.SetNamedArg(Dia::Core::StringCRC(keyBuffer).Value(), value);
				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse flag (--flag)
			////////////////////////////////////////////////////////////////////////////////
			void ParseFlag(const char* arg, ParseResult& result)
			{
				const char* flagName = arg + 2;
				result.args.SetFlag(Dia::Core::StringCRC(flagName).Value(), true);
			}

			////////////////////////////////////////////////////////////////////////////////
			// Parse short flag (-v)
			////////////////////////////////////////////////////////////////////////////////
			bool ParseShortFlag(const char* arg, ParseResult& result)
			{
				Dia::Core::StringCRC shortFlagCRC(arg);

				for (unsigned int i = 0; i < gParserState.shortFlagAliases.Size(); i++)
				{
					if (gParserState.shortFlagAliases[i].shortCRC == shortFlagCRC.Value())
					{
						result.args.SetFlag(gParserState.shortFlagAliases[i].longCRC, true);
						return true;
					}
				}

				result.errorCode = 2;
				result.errorMessage = "Unknown short flag";
				DIA_LOG_ERROR("API", "Unknown short flag: %s", arg);
				return false;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Initialize short flag alias map
			////////////////////////////////////////////////////////////////////////////////
			void InitializeShortFlagAliases()
			{
				if (gParserState.initialized)
				{
					return;
				}

				ShortFlagAlias hAlias;
				hAlias.shortCRC = Dia::Core::StringCRC("-h").Value();
				hAlias.longCRC = Dia::Core::StringCRC("help").Value();
				gParserState.shortFlagAliases.Add(hAlias);

				ShortFlagAlias vAlias;
				vAlias.shortCRC = Dia::Core::StringCRC("-v").Value();
				vAlias.longCRC = Dia::Core::StringCRC("verbose").Value();
				gParserState.shortFlagAliases.Add(vAlias);

				gParserState.initialized = true;
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

			if (!gParserState.initialized)
			{
				InitializeShortFlagAliases();
			}

			if (argc < 2)
			{
				result.errorCode = 2;
				result.errorMessage = "No command specified";
				DIA_LOG_ERROR("API", "No command specified");
				return;
			}

			const char* commandArg = argv[1];

			if (std::strcmp(commandArg, "--help") == 0)
			{
				result.commandName = Dia::Core::StringCRC("");
				result.args.SetFlag(Dia::Core::StringCRC("help").Value(), true);
				return;
			}

			if (commandArg[0] == '-')
			{
				result.errorCode = 2;
				result.errorMessage = "Command name cannot start with '-'";
				DIA_LOG_ERROR("API", "Command name cannot start with '-': %s", commandArg);
				return;
			}

			result.commandName = Dia::Core::StringCRC(commandArg);

			bool endOfFlags = false;

			for (int i = 2; i < argc; i++)
			{
				const char* arg = argv[i];

				if (std::strcmp(arg, "--") == 0)
				{
					endOfFlags = true;
					continue;
				}

				if (endOfFlags)
				{
					result.args.positionalArgs.Add(arg);
					continue;
				}

				if (StartsWith(arg, "--"))
				{
					if (arg[2] == '-')
					{
						result.errorCode = 2;
						result.errorMessage = "Invalid flag format (too many dashes)";
						DIA_LOG_ERROR("API", "Invalid flag format: %s (too many dashes)", arg);
						return;
					}

					if (FindEquals(arg))
					{
						if (!ParseNamedArg(arg, result))
						{
							return;
						}
					}
					else
					{
						ParseFlag(arg, result);
					}
				}
				else if (arg[0] == '-' && arg[1] != '\0' && arg[1] != '-')
				{
					if (!ParseShortFlag(arg, result))
					{
						return;
					}
				}
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

			if (!gParserState.initialized)
			{
				InitializeShortFlagAliases();
			}

			ShortFlagAlias alias;
			alias.shortCRC = Dia::Core::StringCRC(shortFlag).Value();
			alias.longCRC = Dia::Core::StringCRC(longFlag).Value();
			gParserState.shortFlagAliases.Add(alias);
		}
	}
}
