////////////////////////////////////////////////////////////////////////////////
// Filename: PythonBindings.cpp
// Description: Python bindings implementation
////////////////////////////////////////////////////////////////////////////////
#include "PythonBindings.h"
#include "DiaAPI/CommandRegistry/CommandRegistry.h"
#include "DiaAPI/Parser/ArgumentParser.h"
#include <DiaPython/DiaPython.h>
#include <DiaLogger/DiaLog.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <cstring>
#include <cstdio>

namespace Dia
{
	namespace API
	{
		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Convert PythonArgs to CommandArgs via cli-parser
			////////////////////////////////////////////////////////////////////////////////
			CommandArgs ConvertPythonArgs(const Dia::Python::PythonArgs& pyArgs)
			{
				char* argv[64];
				int argc = 0;

				static char progName[] = "dia_api";
				argv[argc++] = progName;

				static char cmdName[] = "python_command";
				argv[argc++] = cmdName;

				static char argBuffers[32][256];
				int bufferIndex = 0;

				int argCount = pyArgs.GetCount();

				for (int i = 0; i < argCount; i++)
				{
					Dia::Python::PythonObject arg = pyArgs.GetArg(i);

					if (bufferIndex >= 32)
					{
						break;
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

				ParseResult parseResult;
				ParseArguments(argc, argv, parseResult);

				return parseResult.args;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Wrapper function for a single command
			////////////////////////////////////////////////////////////////////////////////
			Dia::Python::PythonObject WrapCommand(
				const CommandInfo* cmdInfo,
				const Dia::Python::PythonArgs& pyArgs)
			{
				CommandArgs cppArgs = ConvertPythonArgs(pyArgs);

				int exitCode = cmdInfo->callback(cppArgs);

				return Dia::Python::ToPython(exitCode);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Initialize Python bindings for all registered commands
		////////////////////////////////////////////////////////////////////////////////
		void InitializePythonBindings()
		{
			if (!Dia::Python::IsInitialized())
			{
				DIA_LOG_WARNING("API", "InitializePythonBindings() called but Python is not initialized");
				return;
			}

			Dia::Python::Module* module = Dia::Python::CreateModule("dia_api");
			if (!module)
			{
				DIA_LOG_ERROR("API", "Failed to create 'dia_api' Python module");
				return;
			}

			auto commands = ListCommands();

			DIA_LOG_INFO("API", "Registering %d commands with Python module 'dia_api'",
				commands.Size());

			for (unsigned int i = 0; i < commands.Size(); i++)
			{
				const CommandInfo* cmdInfo = commands[i];

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

				Dia::Python::PythonCallback wrapper = [cmdInfo](const Dia::Python::PythonArgs& pyArgs)
				{
					return Internal::WrapCommand(cmdInfo, pyArgs);
				};

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
