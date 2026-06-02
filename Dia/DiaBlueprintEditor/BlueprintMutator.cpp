#include "DiaBlueprintEditor/BlueprintMutator.h"
#include <cstring>

namespace Dia
{
	namespace BlueprintEditor
	{
		static void SetError(char* errorOut, unsigned int cap, const char* msg)
		{
			if (errorOut && cap > 0)
				strncpy_s(errorOut, cap, msg, _TRUNCATE);
		}

		bool BlueprintMutator::UpdateField(Json::Value& blueprintRoot,
		                                   const char* topKey,
		                                   const char* componentType,
		                                   const char* fieldName,
		                                   const Json::Value& value,
		                                   char* errorOut,
		                                   unsigned int errorCapacity)
		{
			if (!blueprintRoot.isMember(topKey))
			{
				SetError(errorOut, errorCapacity, "blueprint root key not found");
				return false;
			}

			Json::Value& components = blueprintRoot[topKey]["components"];
			for (unsigned int i = 0; i < components.size(); ++i)
			{
				if (components[i]["type"].asString() == componentType)
				{
					components[i]["fields"][fieldName] = value;
					return true;
				}
			}

			SetError(errorOut, errorCapacity, "component type not found");
			return false;
		}

		bool BlueprintMutator::AddComponent(Json::Value& blueprintRoot,
		                                    const char* topKey,
		                                    const char* componentType,
		                                    char* errorOut,
		                                    unsigned int errorCapacity)
		{
			if (!blueprintRoot.isMember(topKey))
			{
				SetError(errorOut, errorCapacity, "blueprint root key not found");
				return false;
			}

			Json::Value& components = blueprintRoot[topKey]["components"];
			for (unsigned int i = 0; i < components.size(); ++i)
			{
				if (components[i]["type"].asString() == componentType)
				{
					SetError(errorOut, errorCapacity, "component type already present");
					return false;
				}
			}

			Json::Value newComp;
			newComp["type"]   = componentType;
			newComp["fields"] = Json::Value(Json::objectValue);
			components.append(newComp);
			return true;
		}

		bool BlueprintMutator::RemoveComponent(Json::Value& blueprintRoot,
		                                        const char* topKey,
		                                        const char* componentType,
		                                        char* errorOut,
		                                        unsigned int errorCapacity)
		{
			if (!blueprintRoot.isMember(topKey))
			{
				SetError(errorOut, errorCapacity, "blueprint root key not found");
				return false;
			}

			Json::Value& components = blueprintRoot[topKey]["components"];
			Json::Value  newComponents(Json::arrayValue);

			bool found = false;
			for (unsigned int i = 0; i < components.size(); ++i)
			{
				if (components[i]["type"].asString() == componentType)
					found = true;
				else
					newComponents.append(components[i]);
			}

			if (!found)
			{
				SetError(errorOut, errorCapacity, "component type not found");
				return false;
			}

			blueprintRoot[topKey]["components"] = newComponents;
			return true;
		}
	}
}
