////////////////////////////////////////////////////////////////////////////////
// Filename: CommandRegistry.cpp
// Description: Command registration and discovery implementation
// Feature spec: docs/specs/features/dia/diaapi/command-registry.md
////////////////////////////////////////////////////////////////////////////////
#include "CommandRegistry.h"
#include "Events/EventSystem.h"
#include <DiaCore/Core/Log.h>
#include <cctype>
#include <cstring>

namespace Dia
{
	namespace API
	{
		namespace Internal
		{
			////////////////////////////////////////////////////////////////////////////////
			// Internal registry state
			////////////////////////////////////////////////////////////////////////////////
			struct RegistryState
			{
				bool isInitialized = false;
				std::unordered_map<unsigned int, CommandInfo*> commands;  // Key: StringCRC.Value(), Value: Heap-allocated CommandInfo
				Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> pendingRegistrations;    // Pre-init registrations
			};

			RegistryState gRegistryState;

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
					// Allow lowercase letters, digits, and hyphens
					if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
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
					Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Invalid command name format: '%s' (must be lowercase + hyphens only)",
						info.name.AsChar());
					return false;
				}

				// Validate description
				if (!info.description || info.description[0] == '\0')
				{
					Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Command '%s' has null or empty description", info.name.AsChar());
					return false;
				}

				// Validate category (StringCRC with empty string is valid, so just check it's not garbage)
				// Category validation removed - all StringCRC values are technically valid

				// Validate owner
				if (!info.owner || info.owner[0] == '\0')
				{
					Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Command '%s' has null or empty owner", info.name.AsChar());
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
				Dia::Core::Log::OutputLine("DiaCLI WARNING: Initialize() called but DiaCLI is already initialized");
				return;  // Idempotent
			}

			// Process pending registrations (commands registered before Initialize)
			for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size(); i++)
			{
				CommandInfo* cmdInfo = gRegistryState.pendingRegistrations[i];
				gRegistryState.commands[cmdInfo->name.Value()] = cmdInfo;

				// Fire event for each pending command
				FireCommandRegistered(cmdInfo->name, cmdInfo->description);
			}

			// Clear pending list (commands are now in main registry)
			gRegistryState.pendingRegistrations.RemoveAll();

			gRegistryState.isInitialized = true;

			Dia::Core::Log::OutputLine("DiaCLI INFO: DiaCLI command registry initialized");
		}

		////////////////////////////////////////////////////////////////////////////////
		// Shutdown the command registry
		////////////////////////////////////////////////////////////////////////////////
		void Shutdown()
		{
			using namespace Internal;

			if (!gRegistryState.isInitialized)
			{
				Dia::Core::Log::OutputLine("DiaCLI WARNING: Shutdown() called but DiaCLI is not initialized");
				return;
			}

			// Note: We don't delete CommandInfo structs - they live for application lifetime
			// This is acceptable as registry is singleton and commands are permanent
			gRegistryState.commands.clear();
			gRegistryState.pendingRegistrations.RemoveAll();

			gRegistryState.isInitialized = false;

			Dia::Core::Log::OutputLine("DiaCLI INFO: DiaCLI command registry shutdown");
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

			// Validate command info
			if (!ValidateCommandInfo(info))
			{
				return false;  // Error already logged
			}

			// Validate callback is not null
			if (!info.callback)
			{
				Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Command '%s' has null callback", info.name.AsChar());
				return false;
			}

			// Check for duplicate
			if (gRegistryState.commands.find(info.name.Value()) != gRegistryState.commands.end())
			{
				Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Command '%s' is already registered (duplicate)", info.name.AsChar());
				return false;
			}

			// Also check pending registrations for duplicates
			for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size(); i++)
			{
				if (gRegistryState.pendingRegistrations[i]->name == info.name)
				{
					Dia::Core::Log::OutputVaradicLine("DiaCLI ERROR: Command '%s' is already in pending registrations (duplicate)", info.name.AsChar());
					return false;
				}
			}

			// Heap-allocate CommandInfo (lives for application lifetime)
			CommandInfo* cmdInfo = new CommandInfo();
			*cmdInfo = info;  // Copy

			// If initialized, register immediately
			if (gRegistryState.isInitialized)
			{
				gRegistryState.commands[cmdInfo->name.Value()] = cmdInfo;
				FireCommandRegistered(cmdInfo->name, cmdInfo->description);
			}
			else
			{
				// Not initialized yet - add to pending list
				gRegistryState.pendingRegistrations.Add(cmdInfo);
			}

			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Get command info by name
		////////////////////////////////////////////////////////////////////////////////
		const CommandInfo* GetCommand(const Dia::Core::StringCRC& name)
		{
			using namespace Internal;

			auto it = gRegistryState.commands.find(name.Value());
			if (it != gRegistryState.commands.end())
			{
				return it->second;
			}

			return nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// List all registered commands
		////////////////////////////////////////////////////////////////////////////////
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> ListCommands()
		{
			using namespace Internal;

			Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> result;

			// Iterate all commands in registry
			for (auto& pair : gRegistryState.commands)
			{
				result.Add(pair.second);
			}

			// TODO: Sort alphabetically by name (optional polish)

			return result;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Get commands by category
		////////////////////////////////////////////////////////////////////////////////
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> GetCommandsByCategory(const Dia::Core::StringCRC& category)
		{
			using namespace Internal;

			Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> result;

			// Filter commands by category
			for (auto& pair : gRegistryState.commands)
			{
				const CommandInfo* cmdInfo = pair.second;
				if (cmdInfo->category == category)
				{
					result.Add(cmdInfo);
				}
			}

			// TODO: Sort alphabetically by name (optional polish)

			return result;
		}
	}
}
