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
#include <mutex>
#include <shared_mutex>

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
				Dia::Core::Containers::DynamicArrayC<CommandInfoJson*, 256> jsonCommands;
			};

			RegistryState gRegistryState;

			////////////////////////////////////////////////////////////////////////////////
			// Locking (command-unregister spec, Design > Locking)
			//
			// gRegistryMutex guards every field of gRegistryState.
			//   - Queries take the shared lock.
			//   - Register*/Unregister*/Initialize/Shutdown take the exclusive lock.
			//   - Execute* takes the shared lock and holds it across the callback, so an
			//     Unregister on another thread waits for in-flight callbacks to return.
			//
			// tCallbackDepth > 0 means this thread is inside an Execute* and already holds
			// the shared lock. Nested queries/executes then run WITHOUT re-acquiring it:
			// a recursive shared acquire can deadlock behind a queued writer (SRWLOCK).
			// Mutations are rejected while it is set (they would self-deadlock).
			//
			// Events (Fire*) are never fired while the exclusive lock is held, so an
			// observer may query the registry. DIA_LOG_* goes to the DiaObservation
			// logger, which never calls back into DiaAPI, so logging under a lock is safe.
			////////////////////////////////////////////////////////////////////////////////
			std::shared_mutex gRegistryMutex;
			thread_local int tCallbackDepth = 0;

			using ReadLock = std::shared_lock<std::shared_mutex>;
			using WriteLock = std::unique_lock<std::shared_mutex>;

			// Shared lock, unless this thread already holds it via an in-flight callback.
			ReadLock AcquireReadLock()
			{
				if (tCallbackDepth > 0)
					return ReadLock(gRegistryMutex, std::defer_lock);
				return ReadLock(gRegistryMutex);
			}

			struct CallbackDepthScope
			{
				CallbackDepthScope() { ++tCallbackDepth; }
				~CallbackDepthScope() { --tCallbackDepth; }
				CallbackDepthScope(const CallbackDepthScope&) = delete;
				CallbackDepthScope& operator=(const CallbackDepthScope&) = delete;
			};

			// AC-9: returns true (and asserts in Debug) if called from inside a command
			// callback. Checked before touching the lock, so it cannot deadlock.
			bool RejectMutationInsideCallback(const char* operation, const char* name)
			{
				if (tCallbackDepth == 0)
					return false;

				DIA_ASSERT(false, "%s('%s') called from inside a command callback; registry mutation is not allowed there",
					operation, name);
				DIA_LOG_ERROR("API", "%s('%s') rejected: called from inside a command callback", operation, name);
				return true;
			}

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
					// Allow lowercase letters, digits, hyphens, dots, and underscores
					if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_'))
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

			// Name/description pair captured under the lock so events can fire after it is released
			struct RegisteredEventData
			{
				Dia::Core::StringCRC name;
				const char* description;
			};
		}

		////////////////////////////////////////////////////////////////////////////////
		// Initialize the command registry
		////////////////////////////////////////////////////////////////////////////////
		void Initialize()
		{
			using namespace Internal;

			if (RejectMutationInsideCallback("Initialize", ""))
				return;

			Dia::Core::Containers::DynamicArrayC<RegisteredEventData, 64> toFire;
			{
				WriteLock lock(gRegistryMutex);

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
					RegisteredEventData data;
					data.name = cmdInfo->name;
					data.description = cmdInfo->description;
					toFire.Add(data);
				}

				gRegistryState.pendingRegistrations.RemoveAll();
				gRegistryState.isInitialized = true;
			}

			for (unsigned int i = 0; i < toFire.Size(); i++)
				FireCommandRegistered(toFire[i].name, toFire[i].description);

			DIA_LOG_INFO("API", "DiaAPI command registry initialized");
		}

		////////////////////////////////////////////////////////////////////////////////
		// Shutdown the command registry
		////////////////////////////////////////////////////////////////////////////////
		void Shutdown()
		{
			using namespace Internal;

			if (RejectMutationInsideCallback("Shutdown", ""))
				return;

			// Entries are detached under the lock and deleted after it is released, so a
			// captured lambda's destructor may safely call back into the registry.
			Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> commands;
			Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> pending;
			Dia::Core::Containers::DynamicArrayC<CommandInfoJson*, 256> jsonCommands;
			bool wasInitialized = false;
			{
				WriteLock lock(gRegistryMutex);
				wasInitialized = gRegistryState.isInitialized;

				// JSON commands are cleaned up even when not initialized: they are
				// registered without going through Initialize() and may hold captured
				// Application pointers that need releasing.
				jsonCommands = gRegistryState.jsonCommands;
				gRegistryState.jsonCommands.RemoveAll();

				if (wasInitialized)
				{
					commands = gRegistryState.commands;
					gRegistryState.commands.RemoveAll();
					pending = gRegistryState.pendingRegistrations;
					gRegistryState.pendingRegistrations.RemoveAll();
					gRegistryState.isInitialized = false;
				}
			}

			for (unsigned int i = 0; i < commands.Size(); i++)
				delete commands[i];
			for (unsigned int i = 0; i < pending.Size(); i++)
				delete pending[i];
			for (unsigned int i = 0; i < jsonCommands.Size(); i++)
				delete jsonCommands[i];

			if (!wasInitialized)
			{
				DIA_LOG_WARNING("API", "Shutdown() called but DiaAPI is not initialized");
				return;
			}

			DIA_LOG_INFO("API", "DiaAPI command registry shutdown");
		}

		////////////////////////////////////////////////////////////////////////////////
		// Check if registry is initialized
		////////////////////////////////////////////////////////////////////////////////
		bool IsInitialized()
		{
			Internal::ReadLock lock = Internal::AcquireReadLock();
			return Internal::gRegistryState.isInitialized;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Register a new command
		////////////////////////////////////////////////////////////////////////////////
		bool RegisterCommand(const CommandInfo& info)
		{
			using namespace Internal;

			if (RejectMutationInsideCallback("RegisterCommand", info.name.AsChar()))
			{
				return false;
			}

			if (!ValidateCommandInfo(info))
			{
				return false;
			}

			if (!info.callback)
			{
				DIA_LOG_ERROR("API", "Command '%s' has null callback", info.name.AsChar());
				return false;
			}

			bool fireRegistered = false;
			{
				WriteLock lock(gRegistryMutex);

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
					fireRegistered = true;
				}
				else
				{
					gRegistryState.pendingRegistrations.Add(cmdInfo);
				}
			}

			if (fireRegistered)
			{
				FireCommandRegistered(info.name, info.description);
			}

			return true;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Unregister a command (registered or pending)
		////////////////////////////////////////////////////////////////////////////////
		bool UnregisterCommand(const Dia::Core::StringCRC& name)
		{
			using namespace Internal;

			if (RejectMutationInsideCallback("UnregisterCommand", name.AsChar()))
			{
				return false;
			}

			CommandInfo* removed = nullptr;
			{
				// Waits for any in-flight ExecuteCommand (shared lock holder) to finish (AC-8)
				WriteLock lock(gRegistryMutex);

				for (unsigned int i = 0; i < gRegistryState.commands.Size() && !removed; i++)
				{
					if (gRegistryState.commands[i]->name == name)
					{
						removed = gRegistryState.commands[i];
						gRegistryState.commands.RemoveAt(i);
					}
				}

				for (unsigned int i = 0; i < gRegistryState.pendingRegistrations.Size() && !removed; i++)
				{
					if (gRegistryState.pendingRegistrations[i]->name == name)
					{
						removed = gRegistryState.pendingRegistrations[i];
						gRegistryState.pendingRegistrations.RemoveAt(i);
					}
				}
			}

			delete removed;
			return removed != nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Unregister a JSON command
		////////////////////////////////////////////////////////////////////////////////
		bool UnregisterCommandJson(const Dia::Core::StringCRC& name)
		{
			using namespace Internal;

			if (RejectMutationInsideCallback("UnregisterCommandJson", name.AsChar()))
			{
				return false;
			}

			CommandInfoJson* removed = nullptr;
			{
				WriteLock lock(gRegistryMutex);

				for (unsigned int i = 0; i < gRegistryState.jsonCommands.Size(); i++)
				{
					if (gRegistryState.jsonCommands[i]->name == name)
					{
						removed = gRegistryState.jsonCommands[i];
						gRegistryState.jsonCommands.RemoveAt(i);
						break;
					}
				}
			}

			delete removed;
			return removed != nullptr;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Execute a registered command by name
		////////////////////////////////////////////////////////////////////////////////
		int ExecuteCommand(const Dia::Core::StringCRC& commandName, const CommandArgs& args)
		{
			using namespace Internal;

			int exitCode = 0;
			{
				// Held for the whole callback (AC-8); skipped when nested on this thread
				ReadLock lock = AcquireReadLock();

				DIA_ASSERT(gRegistryState.isInitialized, "ExecuteCommand called before Initialize()");

				const CommandInfo* cmd = FindCommandInternal(commandName.Value());
				if (!cmd)
				{
					if (lock.owns_lock())
						lock.unlock();
					DIA_LOG_ERROR("API", "Command not found: %s", commandName.AsChar());
					FireCommandError(commandName, "Command not found", 3);
					return 3;
				}

				CallbackDepthScope depthScope;
				FireCommandExecuting(commandName, &args);
				exitCode = cmd->callback(args);
			}

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
			Internal::ReadLock lock = Internal::AcquireReadLock();
			return Internal::FindCommandInternal(name.Value());
		}

		////////////////////////////////////////////////////////////////////////////////
		// List all registered commands
		////////////////////////////////////////////////////////////////////////////////
		Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> ListCommands()
		{
			using namespace Internal;

			Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> result;

			ReadLock lock = AcquireReadLock();
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

			ReadLock lock = AcquireReadLock();
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

			if (RejectMutationInsideCallback("RegisterCommandJson", info.name.AsChar()))
			{
				return false;
			}

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

			WriteLock lock(gRegistryMutex);

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
			Json::Value data;
			{
				// Held for the whole callback (AC-8); skipped when nested on this thread
				ReadLock lock = AcquireReadLock();

				const CommandInfoJson* cmd = FindJsonCommandInternal(name);
				if (!cmd)
				{
					response["success"] = false;
					response["error"]   = "command not found";
					return response;
				}

				CallbackDepthScope depthScope;
				data = cmd->callback(params);
			}

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
