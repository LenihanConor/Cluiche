////////////////////////////////////////////////////////////////////////////////
// Filename: PanelJson.cpp
// Description: See PanelJson.h.
////////////////////////////////////////////////////////////////////////////////
#include <DiaVisualDebugger/Console/PanelJson.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>

#include <cstdio>

namespace Dia
{
	namespace VisualDebugger
	{
		namespace Console
		{
			namespace
			{
				// Canonical group keys from the DiaDebugDomain spec. IDebugDomain::GetGroup()
				// returns a StringCRC, which cannot be reversed to text, so the panel-facing
				// group label is recovered by matching against this table.
				const char* const kGroups[] =
				{
					"CoreDebug",
					"Physics",
					"Animation",
					"Navigation",
					"Rendering",
					"Spatial",
					"Entity",
					"AIBehavior",
				};

				const char* ResolveGroupName(Dia::Core::StringCRC groupId)
				{
					for (const char* name : kGroups)
					{
						if (groupId == Dia::Core::StringCRC(name))
							return name;
					}
					return "CoreDebug";
				}

				// "#rrggbb" - the panel sets each card's --accent CSS variable from this.
				void FormatAccentHex(const Dia::Core::RGBA& colour, char (&out)[8])
				{
					snprintf(out, sizeof(out), "#%02x%02x%02x",
							 static_cast<unsigned int>(colour.R()),
							 static_cast<unsigned int>(colour.G()),
							 static_cast<unsigned int>(colour.B()));
				}
			}

			void BuildDomainStatesRoot(const DiaDebugDomainRegistry& registry, Json::Value& root)
			{
				registry.VisitAll([&root](IDebugDomain& domain)
				{
					Json::Value state(Json::objectValue);
					domain.GetJSONState(state);

					const char* displayName = domain.GetDisplayName();
					if (displayName == nullptr)
						return;

					char accentHex[8] = {};
					FormatAccentHex(domain.GetAccentColour(), accentHex);

					state["_displayName"] = displayName;
					state["_group"]       = ResolveGroupName(domain.GetGroup());
					state["_description"] = domain.GetDescription() ? domain.GetDescription() : "";
					state["_accent"]      = accentHex;

					root[displayName] = state;
				});
			}
		}
	}
}

#endif // DIA_DEBUG
