////////////////////////////////////////////////////////////////////////////////
// Filename: PythonBindings.cpp
// Description: Python bindings implementation
////////////////////////////////////////////////////////////////////////////////
#include "PythonBindings.h"
#include "../CommandRegistry/CommandRegistry.h"
#include "../Parser/ArgumentParser.h"
#include <DiaPython/DiaPython.h>
#include <DiaLogger/DiaLog.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String64.h>
#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace API
	{
		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Convert PythonArgs to CommandArgs
			// Parses argv-style strings ("--key=value", "--flag") using cli-parser
			////////////////////////////////////////////////////////////////////////////////
			CommandArgs ConvertPythonArgs(const Dia::Python::PythonArgs& pyArgs)
			{
				// Build argv-style string array (need char**, not const char**)
				static char* argv[64];
				int argc = 0;

				// Add dummy program name (required by ParseArguments)
				static char progName[] = "dia_api";
				argv[argc++] = progName;

				// Add dummy command name
				static char cmdName[] = "python_command";
				argv[argc++] = cmdName;

				// Convert all Python args to strings (using static buffers)
				static char argBuffers[32][256];  // 32 args, max 256 chars each
				int bufferIndex = 0;

				int argCount = pyArgs.GetCount();

				for (int i = 0; i < argCount; i++)
				{
					Dia::Python::PythonObject arg = pyArgs.GetArg(i);

					if (bufferIndex >= 32)
					{
						break;  // Max 32 arguments
					}

					if (arg.IsString())
					{
						const char* str = Dia::Python::ToString(arg);
						strncpy_s(argBuffers[bufferIndex], 256, str, 255);
						argv[argc++] = argBuffers[bufferIndex];
						bufferIndex++;
					}
					else if (arg.IsInt())
					{
						int value = Dia::Python::ToInt(arg);
						sprintf_s(argBuffers[bufferIndex], 256, "%d", value);
						argv[argc++] = argBuffers[bufferIndex];
						bufferIndex++;
					}
					else if (arg.IsFloat())
					{
						float value = Dia::Python::ToFloat(arg);
						sprintf_s(argBuffers[bufferIndex], 256, "%f", value);
						argv[argc++] = argBuffers[bufferIndex];
						bufferIndex++;
					}
				}

				// Parse using cli-parser
				static ParseResult parseResult;  // Static to avoid HashTable copy/move issues
				ParseArguments(argc, argv, parseResult);

				// Return directly (avoid copying HashTable which has buggy copy constructor)
				return std::move(parseResult.args);
			}

			////////////////////////////////////////////////////////////////////////////////
			// Wrapper function for a single command
			// Converts Python args to CommandArgs, calls command callback, returns exit code
			////////////////////////////////////////////////////////////////////////////////
			Dia::Python::PythonObject WrapCommand(
				const CommandInfo* cmdInfo,
				const Dia::Python::PythonArgs& pyArgs)
			{
				// Convert Python args to CommandArgs
				CommandArgs cppArgs = ConvertPythonArgs(pyArgs);

				// Execute command callback
				int exitCode = 0;
				try
				{
					exitCode = cmdInfo->callback(cppArgs);
				}
				catch (const std::exception& ex)
				{
					DIA_LOG_ERROR("API", "Command '%s' threw exception: %s",
						cmdInfo->name.AsChar(), ex.what());
					exitCode = 1;
				}
				catch (...)
				{
					DIA_LOG_ERROR("API", "Command '%s' threw unknown exception",
						cmdInfo->name.AsChar());
					exitCode = 1;
				}

				// Convert exit code to Python
				return Dia::Python::ToPython(exitCode);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Initialize Python bindings for all registered commands
		////////////////////////////////////////////////////////////////////////////////
		void InitializePythonBindings()
		{
			// Check if Python is initialized
			if (!Dia::Python::IsInitialized())
			{
				DIA_LOG_WARNING("API", "InitializePythonBindings() called but Python is not initialized");
				return;
			}

			// Create dia_api module
			Dia::Python::Module* module = Dia::Python::CreateModule("dia_api");
			if (!module)
			{
				DIA_LOG_ERROR("API", "Failed to create 'dia_api' Python module");
				return;
			}

			// Enumerate all registered commands
			auto commands = ListCommands();

			DIA_LOG_INFO("API", "Found %d commands", commands.Size());

			DIA_LOG_INFO("API", "Registering %d commands with Python module 'dia_api'",
				commands.Size());

			// Register each command as a Python function
			for (unsigned int i = 0; i < commands.Size(); i++)
			{
				const CommandInfo* cmdInfo = commands[i];

				// Convert command name to Python function name (replace hyphens with underscores)
				const char* cliName = cmdInfo->name.AsChar();
				char pythonName[64];
				int destIdx = 0;

				for (int srcIdx = 0; cliName[srcIdx] != '\0' && destIdx < 63; srcIdx++)
				{
					if (cliName[srcIdx] == '-')
					{
						pythonName[destIdx] = '_';
					}
					else
					{
						pythonName[destIdx] = cliName[srcIdx];
					}
					destIdx++;
				}
				pythonName[destIdx] = '\0';

				// Create wrapper lambda
				Dia::Python::PythonCallback wrapper = [cmdInfo](const Dia::Python::PythonArgs& pyArgs)
				{
					return Internal::WrapCommand(cmdInfo, pyArgs);
				};

				// Register with DiaPython
				Dia::Python::AddFunction(
					module,
					pythonName,
					wrapper,
					cmdInfo->description
				);

				DIA_LOG_INFO("API", "Registered Python function: dia_api.%s()",
					pythonName);
			}

			DIA_LOG_INFO("API", "Python bindings initialized successfully");
		}
	}
}
