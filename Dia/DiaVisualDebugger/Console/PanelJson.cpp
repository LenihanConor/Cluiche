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

			bool BuildDomainState(IDebugDomain& domain, Json::Value& state, const ISchemaKeyResolver* resolver)
			{
				const char* displayName = domain.GetDisplayName();

				state = Json::Value(Json::objectValue);
				domain.GetJSONState(state);
				if (resolver != nullptr && resolver->IsAvailable()
					&& state.isMember("controls") && state["controls"].isArray() && !state.isMember("schemaVersion"))
					state["schemaVersion"] = resolver->Version();

				if (displayName == nullptr)
					return false;

				char accentHex[8] = {};
				FormatAccentHex(domain.GetAccentColour(), accentHex);

				// Registry id: commands are addressed by id (DiaDebugDomainRegistry::FindDomain), the state is keyed by display name.
				state["_id"]          = domain.GetDomainId().AsChar();
				state["_displayName"] = displayName;
				state["_group"]       = ResolveGroupName(domain.GetGroup());
				state["_description"] = domain.GetDescription() ? domain.GetDescription() : "";
				state["_accent"]      = accentHex;
				return true;
			}

			void BuildDomainStatesRoot(const DiaDebugDomainRegistry& registry, Json::Value& root,
									   const ISchemaKeyResolver* resolver)
			{
				registry.VisitAll([&root, resolver](IDebugDomain& domain)
				{
					Json::Value state;
					if (BuildDomainState(domain, state, resolver))
						root[domain.GetDisplayName()] = state;
				});
			}
		}
	}
}

#endif // DIA_DEBUG
