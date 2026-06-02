#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace BlueprintEditor
	{
		// Pure JSON-manipulation helpers for blueprint mutation operations.
		// Each method operates on an already-loaded blueprintRoot in memory.
		// The caller is responsible for load/save around these calls.
		class BlueprintMutator
		{
		public:
			// Patch fieldName on the given componentType. Returns false if topKey or
			// componentType are absent. errorOut is optional (may be nullptr).
			static bool UpdateField(Json::Value& blueprintRoot,
			                        const char* topKey,
			                        const char* componentType,
			                        const char* fieldName,
			                        const Json::Value& value,
			                        char* errorOut = nullptr,
			                        unsigned int errorCapacity = 0);

			// Append a new component with empty fields. Returns false if topKey is
			// absent or componentType is already present.
			static bool AddComponent(Json::Value& blueprintRoot,
			                         const char* topKey,
			                         const char* componentType,
			                         char* errorOut = nullptr,
			                         unsigned int errorCapacity = 0);

			// Remove a component by type. Returns false if topKey is absent or
			// componentType is not found. Preserves order of remaining components.
			static bool RemoveComponent(Json::Value& blueprintRoot,
			                             const char* topKey,
			                             const char* componentType,
			                             char* errorOut = nullptr,
			                             unsigned int errorCapacity = 0);
		};
	}
}
