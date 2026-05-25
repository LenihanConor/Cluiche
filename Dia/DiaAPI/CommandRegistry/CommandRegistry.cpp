////////////////////////////////////////////////////////////////////////////////
// Filename: CommandRegistry.cpp
// Description: Command registration and discovery implementation
// Feature spec: docs/specs/features/dia/diaapi/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#include "CommandRegistry.h"
#include "Events/EventSystem.h"
#include "Parser/ArgumentParser.h"
#include "Help/HelpSystem.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Core/Assert.h>
#include <cctype>
#include <cstring>

namespace Dia
{
	namespace API
	{
		////////////////////////////////////////////////////////////////////////////////
		// CommandArgs accessor implementations
		////////////////////////////////////////////////////////////////////////////////
		const char* CommandArgs::GetNamedArg(unsigned int crcValue) const
		{
			for (unsigned int i = 0; i < namedArgs.Size(); i++)
			{
				if (namedArgs[i].key == crcValue)
				{
					return namedArgs[i].value;
				}
			}
			return nullptr;
		}

		bool CommandArgs::HasFlag(unsigned int crcValue) const
		{
			for (unsigned int i = 0; i < flags.Size(); i++)
			{
				if (flags[i].key == crcValue)
				{
					return flags[i].value;
				}
			}
			return false;
		}

		void CommandArgs::SetNamedArg(unsigned int crcValue, const char* value)
		{
			for (unsigned int i = 0; i < namedArgs.Size(); i++)
			{
				if (namedArgs[i].key == crcValue)
				{
					namedArgs[i].value = value;
					return;
				}
			}
			NamedArgEntry entry;
			entry.key = crcValue;
			entry.value = value;
			namedArgs.Add(entry);
		}

		void CommandArgs::SetFlag(unsigned int crcValue, bool value)
		{
			for (unsigned int i = 0; i < flags.Size(); i++)
			{
				if (flags[i].key == crcValue)
				{
					flags[i].value = value;
					return;
				}
			}
			FlagEntry entry;
			entry.key = crcValue;
			entry.value = value;
			flags.Add(entry);
		}

		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Internal registry state
			////////////////////////////////////////////////////////////////////////////////
			struct RegistryState
			{
				bool isInitialized = false;
				Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> commands;
				Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> pendingRegistrations;
				Dia::Core::Containers::DynamicArrayC<CommandInfoJson*, 64> jsonCommands;
			};

			RegistryState gRegistryState;

			CommandInfoJson* FindJsonCommandInternal(const Dia::Core::StringCRC& name)
			{
				for (unsigned int i = 0; i < gRegistryState.jsonCommands.Size(); i++)
				{
					if (gRegistryState.jsonCommands[i]->name == name)
						return gRegistryState.jsonCommands[i];
				}
				return nullptr;
			}

			CommandInfo* FindCommandInternal(unsigned int crcValue)
			{
				for (unsigned int i = 0; i < gRegistryState.commands.Size(); i++)
				{
					if (gRegistryState.commands[i]->name.Value() == crcValue)
					{
						return gRegistryState.commands[i];
					}
				}
				return nullptr;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Validate command name format: lowercase + hyphens only [a-z0-9-]+
			////////////////////////////////////////////////////////////////////////////////
			bool ValidateCommandName(const char* name)
			{
				if (!name || name[0] == '\0')
				{
					return false;
				}

				for (const char* p = name; *p != '\0'; ++p)
				{
					char c = *p;
					// Allow lowercase letters, digits, hyphens, and dots
					if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.'))
					{
						return false;
					}
				}

				return true;
			}

			////////////////////////////////////////////////////////////////////////////////
			// Validate required command metadata fields
			////////////////////////////////////////////////////////////////////////////////
			bool ValidateCommandInfo(const CommandInfo& info)
			{
				// Validate command name format
				if (!ValidateCommandName(info.name.AsChar()))
				{
					DIA_LOG_ERROR("API", "Invalid command name format: '%s' (must be lowercase + hyphens only)",
						info.name.AsChar());
					return false;
				}

				// Validate description
				if (!info.description || info.description[0] == '\0')
				{
					DIA_LOG_ERROR("API", "Command '%s' has null or empty description", info.name.AsChar());
					return false;
				}

				// Validate category (StringCRC with empty string is valid, so just check it's not garbage)
				// Category validation removed - all StringCRC values are technically valid

				// Validate owner
				if (!info.owner || info.owner[0] == '\0')
				{
					DIA_LOG_ERROR("API", "Command '%s' has null or empty owner", info.name.AsChar());
					return false;
				}

				// Callback is checked by RegisterCommand (can't be null)

				return true;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Initialize the command registry
		////////////////////////////////////////////////////////////////////////////////
		void Initialize()
		{
			using namespace Internal;

			if (gRegistryState.isInitialized)
			{
				DIA_LOG_WARNING("API", "Initialize() called but DiaAPI is already initialized");
				return;
			}

			// Process pending registrations (commands registered before Initialize)
			for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size(); i++)
			{
				CommandInfo* cmdInfo = gRegistryState.pendingRegistrations[i];
				gRegistryState.commands.Add(cmdInfo);
				FireCommandRegistered(cmdInfo->name, cmdInfo->description);
			}

			gRegistryState.pendingRegistrations.RemoveAll();
			gRegistryState.isInitialized = true;

			DIA_LOG_INFO("API", "DiaAPI command registry initialized");
		}

		////////////////////////////////////////////////////////////////////////////////
		// Shutdown the command registry
		////////////////////////////////////////////////////////////////////////////////
		void Shutdown()
		{
			using namespace Internal;

			if (!gRegistryState.isInitialized)
			{
				// Still clean up JSON commands even when not initialized — they are
				// registered without going through Initialize() and may hold captured
				// Application pointers that need releasing.
				for (unsigned int i = 0; i < gRegistryState.jsonCommands.Size(); i++)
					delete gRegistryState.jsonCommands[i];
				gRegistryState.jsonCommands.RemoveAll();
				DIA_LOG_WARNING("API", "Shutdown() called but DiaAPI is not initialized");
				return;
			}

			// Free heap-allocated CLI command entries
			for (unsigned int i = 0; i < gRegistryState.commands.Size(); i++)
				delete gRegistryState.commands[i];
			gRegistryState.commands.RemoveAll();

			for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size(); i++)
				delete gRegistryState.pendingRegistrations[i];
			gRegistryState.pendingRegistrations.RemoveAll();

			// Free heap-allocated JSON command entries (and release captured lambdas)
			for (unsigned int i = 0; i < gRegistryState.jsonCommands.Size(); i++)
				delete gRegistryState.jsonCommands[i];
			gRegistryState.jsonCommands.RemoveAll();

			gRegistryState.isInitialized = false;

			DIA_LOG_INFO("API", "DiaAPI command registry shutdown");
		}

		////////////////////////////////////////////////////////////////////////////////
		// Check if registry is initialized
		////////////////////////////////////////////////////////////////////////////////
		bool IsInitialized()
		{
			return Internal::gRegistryState.isInitialized;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Register a new command
		////////////////////////////////////////////////////////////////////////////////
		bool RegisterCommand(const CommandInfo& info)
		{
			using namespace Internal;

			if (!ValidateCommandInfo(info))
			{
				return false;
			}

			if (!info.callback)
			{
				DIA_LOG_ERROR("API", "Command '%s' has null callback", info.name.AsChar());
				return false;
			}

			// Check for duplicate in main registry
			if (FindCommandInternal(info.name.Value()) != nullptr)
			{
				DIA_LOG_ERROR("API", "Command '%s' is already registered (duplicate)", info.name.AsChar());
				return false;
			}

			// Check pending registrations for duplicates
			for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size(); i++)
			{
				if (gRegistryState.pendingRegistrations[i]->name == info.name)
				{
					DIA_LOG_ERROR("API", "Command '%s' is already in pending registrations (duplicate)", info.name.AsChar());
					return false;
				}
			}

			CommandInfo* cmdInfo = new CommandInfo();
			*cmdInfo = info;

			if (gRegistryState.isInitialized)
			{
				gRegistryState.commands.Add(cmdInfo);
				FireCommandRegistered(cmdInfo->name, cmdInfo->description);
			}
			else
			{
				gRegistryState.pendingRegistrations.Add(cmdInfo);
			}

			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute a registered command by name
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteCommand(const Dia::Core::StringCRC& commandName, const CommandArgs& args)
		{
			using namespace Internal;

			DIA_ASSERT(gRegistryState.isInitialized, "ExecuteCommand called before Initialize()");

			const CommandInfo* cmd = GetCommand(commandName);
			if (!cmd)
			{
				DIA_LOG_ERROR("API", "Command not found: %s", commandName.AsChar());
				FireCommandError(commandName, "Command not found", 3);
				return 3;
			}

			FireCommandExecuting(commandName, &args);

			int exitCode = cmd->callback(args);

			if (exitCode != 0)
			{
				FireCommandError(commandName, "Command returned non-zero exit code", exitCode);
			}

			FireCommandExecuted(commandName, exitCode, 0.0f);

			return exitCode;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Main CLI entry point: parse args, dispatch command
		////////////////////////////////////////////////////////////////////////////////
		int RunCLI(int argc, char* argv[])
		{
			ParseResult parseResult;
			ParseArguments(argc, argv, parseResult);

			if (parseResult.errorCode != 0)
			{
				DIA_LOG_ERROR("API", "Argument parse error: %s", parseResult.errorMessage);
				return parseResult.errorCode;
			}

			// Check for help
			if (IsHelpRequested(parseResult.args, parseResult.commandName))
			{
				if (parseResult.commandName.AsChar()[0] == '\0')
				{
					return ShowGlobalHelp();
				}
				return ShowCommandHelp(parseResult.commandName);
			}

			return ExecuteCommand(parseResult.commandName, parseResult.args);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Get command info by name
		////////////////////////////////////////////////////////////////////////////////
		const CommandInfo* GetCommand(const Dia::Core::StringCRC& name)
		{
			return Internal::FindCommandInternal(name.Value());
		}

		////////////////////////////////////////////////////////////////////////////////
		// List all registered commands
		////////////////////////////////////////////////////////////////////////////////
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> ListCommands()
		{
			using namespace Internal;

			Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> result;

			for (unsigned int i = 0; i < gRegistryState.commands.Size(); i++)
			{
				result.Add(gRegistryState.commands[i]);
			}

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Get commands by category
		////////////////////////////////////////////////////////////////////////////////
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> GetCommandsByCategory(const Dia::Core::StringCRC& category)
		{
			using namespace Internal;

			Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> result;

			for (unsigned int i = 0; i < gRegistryState.commands.Size(); i++)
			{
				const CommandInfo* cmdInfo = gRegistryState.commands[i];
				if (cmdInfo->category == category)
				{
					result.Add(cmdInfo);
				}
			}

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// RegisterCommandJson
		////////////////////////////////////////////////////////////////////////////////
		bool RegisterCommandJson(const CommandInfoJson& info)
		{
			using namespace Internal;

			if (!ValidateCommandName(info.name.AsChar()))
			{
				DIA_LOG_ERROR("API", "RegisterCommandJson: invalid name '%s'", info.name.AsChar());
				return false;
			}
			if (!info.description || info.description[0] == '\0')
			{
				DIA_LOG_ERROR("API", "RegisterCommandJson: command '%s' has empty description", info.name.AsChar());
				return false;
			}
			if (!info.owner || info.owner[0] == '\0')
			{
				DIA_LOG_ERROR("API", "RegisterCommandJson: command '%s' has empty owner", info.name.AsChar());
				return false;
			}
			if (!info.callback)
			{
				DIA_LOG_ERROR("API", "RegisterCommandJson: command '%s' has null callback", info.name.AsChar());
				return false;
			}
			if (FindJsonCommandInternal(info.name) != nullptr)
			{
				DIA_LOG_ERROR("API", "RegisterCommandJson: command '%s' already registered", info.name.AsChar());
				return false;
			}
			// Warn if a CLI command with the same name also exists
			if (FindCommandInternal(info.name.Value()) != nullptr)
			{
				DIA_LOG_WARNING("API", "RegisterCommandJson: command '%s' also exists as a CLI command", info.name.AsChar());
			}

			CommandInfoJson* entry = new CommandInfoJson();
			*entry = info;
			gRegistryState.jsonCommands.Add(entry);
			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		// ExecuteCommandJson
		////////////////////////////////////////////////////////////////////////////////
		Json::Value ExecuteCommandJson(const Dia::Core::StringCRC& name, const Json::Value& params)
		{
			using namespace Internal;

			Json::Value response;
			const CommandInfoJson* cmd = FindJsonCommandInternal(name);
			if (!cmd)
			{
				response["success"] = false;
				response["error"]   = "command not found";
				return response;
			}

			Json::Value data = cmd->callback(params);
			if (data.isObject() && data.isMember("error"))
			{
				response["success"] = false;
				response["error"]   = data["error"];
			}
			else
			{
				response["success"] = true;
				response["data"]    = data;
			}
			return response;
		}
	}
}
