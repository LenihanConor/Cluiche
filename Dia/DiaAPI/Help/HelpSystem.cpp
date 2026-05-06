////////////////////////////////////////////////////////////////////////////////
// Filename: HelpSystem.cpp
// Description: Help system implementation
// Feature spec: docs/specs/features/dia/diacli/help-system.md
////////////////////////////////////////////////////////////////////////////////
#include "HelpSystem.h"
#include <DiaAPI/Events/EventSystem.h>
#include <DiaLogger/DiaLog.h>
#include <cstdio>
#include <cstring>

namespace Dia
{
	namespace API
	{
		////////////////////////////////////////////////////////////////////////////////
		// Check if help was requested
		////////////////////////////////////////////////////////////////////////////////
		bool IsHelpRequested(const CommandArgs& args, const Dia::Core::StringCRC& commandName)
		{
			return args.HasFlag(Dia::Core::StringCRC("help").Value());
		}

		////////////////////////////////////////////////////////////////////////////////
		// Show global help (all commands grouped by category)
		////////////////////////////////////////////////////////////////////////////////
		int ShowGlobalHelp()
		{
			// Fire OnHelpRequested event
			Internal::FireHelpRequested(Dia::Core::StringCRC(""), true);

			printf("DiaAPI - Command registration and execution API\n\n");
			printf("Usage: DiaAPI.RegisterCommand(...) or dia_api.command_name()\n\n");

			// Get all registered commands
			auto commands = ListCommands();

			if (commands.Size() == 0)
			{
				printf("No commands registered.\n\n");
				return 0;
			}

			printf("Available commands (grouped by category):\n\n");

			// Group commands by category
			// We'll iterate multiple times - once per unique category
			// First, collect unique categories
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> categories;

			for (unsigned int i = 0; i < commands.Size(); i++)
			{
				const CommandInfo* cmd = commands[i];
				bool found = false;

				// Check if category already in list
				for (unsigned int j = 0; j < categories.Size(); j++)
				{
					if (categories[j] == cmd->category)
					{
						found = true;
						break;
					}
				}

				// Add category if not found
				if (!found)
				{
					categories.Add(cmd->category);
				}
			}

			// Now display commands grouped by category
			for (unsigned int catIdx = 0; catIdx < categories.Size(); catIdx++)
			{
				Dia::Core::StringCRC category = categories[catIdx];

				// Print category header (uppercase)
				printf("%s:\n", category.AsChar());

				// Display all commands in this category
				for (unsigned int cmdIdx = 0; cmdIdx < commands.Size(); cmdIdx++)
				{
					const CommandInfo* cmd = commands[cmdIdx];

					if (cmd->category == category)
					{
						// Print command name and description
						printf("  %-20s  %s", cmd->name.AsChar(), cmd->description);

						// Print version if available
						if (cmd->version && cmd->version[0] != '\0')
						{
							printf(" [%s]", cmd->version);
						}

						printf("\n");

						// Print owner
						if (cmd->owner && cmd->owner[0] != '\0')
						{
							printf("    Owner: %s\n", cmd->owner);
						}

						// Print example
						if (cmd->example && cmd->example[0] != '\0')
						{
							printf("    Example: %s\n", cmd->example);
						}

						printf("\n");
					}
				}
			}

			printf("Use 'dia_api.help(\"<command>\")' for command-specific help.\n");

			return 0;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Show help for specific command
		////////////////////////////////////////////////////////////////////////////////
		int ShowCommandHelp(const Dia::Core::StringCRC& commandName)
		{
			// Fire OnHelpRequested event
			Internal::FireHelpRequested(commandName, false);

			// Get command from registry
			const CommandInfo* cmd = GetCommand(commandName);

			if (!cmd)
			{
				printf("Command not found: %s\n", commandName.AsChar());
				DIA_LOG_WARNING("API", "Command not found: %s", commandName.AsChar());
				return 3;
			}

			// Display command-specific help
			printf("DiaAPI %s - %s\n\n", cmd->name.AsChar(), cmd->description);

			printf("Category: %s\n", cmd->category.AsChar());

			if (cmd->owner && cmd->owner[0] != '\0')
			{
				printf("Owner:    %s\n", cmd->owner);
			}

			if (cmd->version && cmd->version[0] != '\0')
			{
				printf("Version:  %s\n", cmd->version);
			}

			printf("\n");

			if (cmd->example && cmd->example[0] != '\0')
			{
				printf("Usage:\n");
				printf("  %s\n\n", cmd->example);
			}

			printf("Description:\n");
			printf("  %s\n", cmd->description);

			return 0;
		}
	}
}
