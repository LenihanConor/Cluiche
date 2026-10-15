////////////////////////////////////////////////////////////////////////////////
// Filename: CommandLineParser.cpp
// Description: Console line tokenizer + ArgumentParser bridge.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/panel-console.md
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/CommandLineParser.h>

#include <DiaAPI/Parser/ArgumentParser.h>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				static const unsigned int kMaxArgv = kMaxCommandLineTokens + 2;	// "dia" + name + tokens

				bool IsSpace(char c)
				{
					return c == ' ' || c == '\t' || c == '\r' || c == '\n';
				}

				// Unquoted "key=value" with non-empty key and no leading '-'.
				bool IsBareNamedArg(const char* start, unsigned int length)
				{
					if (length < 2 || start[0] == '-' || start[0] == '=')
					{
						return false;
					}
					for (unsigned int i = 1; i < length; ++i)
					{
						if (start[i] == '=')
						{
							return true;
						}
					}
					return false;
				}

				// Appends characters to a fixed buffer, refusing to overflow.
				class StorageWriter
				{
				public:
					StorageWriter(char* buffer, unsigned int size)
						: mBuffer(buffer), mSize(size), mUsed(0)
					{}

					// Start of the next string, or nullptr if out of room.
					char* Begin() { return (mUsed < mSize) ? mBuffer + mUsed : nullptr; }

					bool Put(char c)
					{
						if (mUsed >= mSize)
						{
							return false;
						}
						mBuffer[mUsed++] = c;
						return true;
					}

					bool PutString(const char* s)
					{
						while (*s != '\0')
						{
							if (!Put(*s++))
							{
								return false;
							}
						}
						return true;
					}

				private:
					char* mBuffer;
					unsigned int mSize;
					unsigned int mUsed;
				};
			}

			////////////////////////////////////////////////////////////////////////////////
			ParsedCommand::ParsedCommand()
				: name()
				, args()
			{
				storage[0] = '\0';
			}

			////////////////////////////////////////////////////////////////////////////////
			const char* ParseErrorToString(ParseError error)
			{
				switch (error)
				{
				case ParseError::None:				return "none";
				case ParseError::Empty:				return "empty command";
				case ParseError::TooLong:			return "command too long";
				case ParseError::UnterminatedQuote:	return "unterminated quote";
				case ParseError::TooManyArgs:		return "too many arguments";
				case ParseError::InvalidArgument:	return "invalid argument";
				}
				return "unknown";
			}

			////////////////////////////////////////////////////////////////////////////////
			ParseError ParseCommandLine(const char* text, ParsedCommand& out)
			{
				out.name = Dia::Core::StringCRC();
				out.args.positionalArgs.RemoveAll();
				out.args.namedArgs.RemoveAll();
				out.args.flags.RemoveAll();
				out.storage[0] = '\0';

				if (text == nullptr)
				{
					return ParseError::Empty;
				}

				// Length check without reading past the limit.
				unsigned int length = 0;
				while (text[length] != '\0')
				{
					if (length >= kMaxCommandLineLength)
					{
						return ParseError::TooLong;
					}
					++length;
				}

				char* argv[kMaxArgv];
				int argc = 0;

				StorageWriter writer(out.storage, ParsedCommand::kStorageSize);
				argv[argc++] = writer.Begin();
				writer.PutString("dia");
				writer.Put('\0');

				bool endOfFlags = false;
				unsigned int pos = 0;
				while (pos < length)
				{
					while (pos < length && IsSpace(text[pos]))
					{
						++pos;
					}
					if (pos >= length)
					{
						break;
					}

					if (argc >= static_cast<int>(kMaxArgv))
					{
						return ParseError::TooManyArgs;
					}

					// Scan one token: quoted sections group, quotes are stripped.
					const unsigned int tokenStart = pos;
					bool quoted = false;
					bool inQuote = false;
					while (pos < length && (inQuote || !IsSpace(text[pos])))
					{
						if (text[pos] == '"')
						{
							inQuote = !inQuote;
							quoted = true;
						}
						++pos;
					}
					if (inQuote)
					{
						return ParseError::UnterminatedQuote;
					}

					const unsigned int tokenLength = pos - tokenStart;
					const bool isName = (argc == 1);
					const bool rewriteNamed = !isName && !quoted && !endOfFlags
						&& IsBareNamedArg(text + tokenStart, tokenLength);

					if (!isName && !quoted && tokenLength == 2 && text[tokenStart] == '-' && text[tokenStart + 1] == '-')
					{
						endOfFlags = true;
					}

					char* tokenOut = writer.Begin();
					if (tokenOut == nullptr)
					{
						return ParseError::TooLong;
					}
					if (rewriteNamed && !writer.PutString("--"))
					{
						return ParseError::TooLong;
					}
					for (unsigned int i = tokenStart; i < pos; ++i)
					{
						if (text[i] != '"' && !writer.Put(text[i]))
						{
							return ParseError::TooLong;
						}
					}
					if (!writer.Put('\0'))
					{
						return ParseError::TooLong;
					}

					if (isName && tokenOut[0] == '\0')
					{
						return ParseError::Empty;
					}
					argv[argc++] = tokenOut;
				}

				if (argc < 2)
				{
					return ParseError::Empty;
				}

				Dia::API::ParseResult result;
				result.errorCode = 0;
				result.errorMessage = nullptr;
				Dia::API::ParseArguments(argc, argv, result);
				if (result.errorCode != 0)
				{
					return ParseError::InvalidArgument;
				}

				out.name = result.commandName;
				out.args = result.args;
				return ParseError::None;
			}
		}
	}
}
