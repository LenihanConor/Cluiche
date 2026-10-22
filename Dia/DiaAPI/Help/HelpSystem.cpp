////////////////////////////////////////////////////////////////////////////////
// Filename: HelpSystem.cpp
// Description: Help system implementation
// Feature spec: docs/specs/features/dia/diacli/help-system.md
////////////////////////////////////////////////////////////////////////////////
#include "HelpSystem.h"
#include <DiaAPI/Events/EventSystem.h>
#include <DiaObservation/Log/DiaLog.h>
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

			if (cmd->params && cmd->paramCount > 0)
			{
				char section[4096];
				unsigned needed = FormatArgumentsSection(cmd->params, cmd->paramCount, section, sizeof(section));
				if (needed > 0)
				{
					printf("\n%s", section);
				}
			}

			return 0;
		}

		namespace
		{
			constexpr unsigned kDetailMax = 256;

			struct Writer
			{
				char* out;
				unsigned capacity;
				unsigned total = 0;

				void Put(char c)
				{
					if (out && capacity > 0 && total + 1 < capacity)
					{
						out[total] = c;
					}
					total++;
				}
				void Put(const char* s) { while (*s) Put(*s++); }
				void Pad(unsigned n) { while (n--) Put(' '); }
			};

			const char* OrQ(const char* s) { return s ? s : "?"; }

			void BuildDetail(const ValueParam& p, char* d)
			{
				d[0] = '\0';
				if (p.type && strcmp(p.type, "enum") == 0 && p.enumValues && p.enumValues[0])
				{
					snprintf(d, kDetailMax, "%s", p.enumValues);
					return;
				}
				bool hasUnit = p.unit && p.unit[0];
				if (hasUnit && p.hasRange)
					snprintf(d, kDetailMax, "%s, %g..%g", p.unit, (double)p.min, (double)p.max);
				else if (hasUnit)
					snprintf(d, kDetailMax, "%s", p.unit);
				else if (p.hasRange)
					snprintf(d, kDetailMax, "%g..%g", (double)p.min, (double)p.max);
			}
		}

		unsigned FormatArgumentsSection(const ValueParam* params, unsigned count, char* out, unsigned capacity)
		{
			if (out && capacity > 0) out[0] = '\0';
			if (!params || count == 0) return 0;

			unsigned nameW = 0, typeW = 0, detailW = 0;
			char detail[kDetailMax];
			for (unsigned i = 0; i < count; i++)
			{
				unsigned n = (unsigned)strlen(OrQ(params[i].name)) + 2;
				unsigned t = (unsigned)strlen(OrQ(params[i].type));
				BuildDetail(params[i], detail);
				unsigned d = (unsigned)strlen(detail);
				if (n > nameW) nameW = n;
				if (t > typeW) typeW = t;
				if (d > detailW) detailW = d;
			}

			Writer w{ out, capacity };
			w.Put("Arguments:\n");
			for (unsigned i = 0; i < count; i++)
			{
				const ValueParam& p = params[i];
				const char* name = OrQ(p.name);
				const char* type = OrQ(p.type);
				BuildDetail(p, detail);
				bool hasDesc = p.description && p.description[0];

				// Build line pieces, padding only when more text follows (no trailing spaces).
				w.Put("  ");
				w.Put(p.required ? '<' : '[');
				w.Put(name);
				w.Put(p.required ? '>' : ']');
				unsigned used = (unsigned)strlen(name) + 2;

				bool moreAfterName = typeW > 0;
				if (moreAfterName)
				{
					w.Pad(nameW - used + 2);
					w.Put(type);
					unsigned tl = (unsigned)strlen(type);
					bool moreAfterType = detailW > 0 || hasDesc;
					if (moreAfterType)
					{
						w.Pad(typeW - tl + 2);
						if (detailW > 0)
						{
							unsigned dl = (unsigned)strlen(detail);
							w.Put(detail);
							if (hasDesc)
							{
								w.Pad(detailW - dl + 3); // 3 spaces before description, matching the spec sample
							}
						}
						if (hasDesc) w.Put(p.description);
					}
				}
				w.Put('\n');
			}

			if (out && capacity > 0)
			{
				out[w.total < capacity ? w.total : capacity - 1] = '\0';
			}
			return w.total;
		}
	}
}
