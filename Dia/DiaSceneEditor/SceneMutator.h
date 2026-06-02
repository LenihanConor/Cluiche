#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		// Pure JSON mutation helpers for scene CRUD operations.
		// All methods operate on an in-memory sceneRoot and return true on success.
		// errBuf/errBufSize are optional; pass nullptr to skip error output.
		class SceneMutator
		{
		public:
			// T11: Append a new item to the given section array.
			// itemType: "entity" | "camera" | "light"
			// blueprintId: the blueprint asset id (will be stored as {"value":"..."})
			// Auto-generates id as "{blueprintId}_{N}" where N avoids collisions.
			static bool AddItem(Json::Value& sceneRoot,
			                    const char*  itemType,
			                    const char*  blueprintId,
			                    char* errBuf, int errBufSize);

			// T12: Deep copy an item; new id = "{oldId}_copy" (suffix _copy2, _copy3 if taken).
			// Offsets instance_data Transform2D.position by +50 on both axes if present.
			static bool DuplicateItem(Json::Value& sceneRoot,
			                          const char*  itemType,
			                          const char*  itemId,
			                          char* errBuf, int errBufSize);

			// T13: Remove an item from its array by id.
			static bool DeleteItem(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  itemId,
			                       char* errBuf, int errBufSize);

			// T13: Set the enabled flag on an item.
			static bool SetEnabled(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  itemId,
			                       bool         enabled,
			                       char* errBuf, int errBufSize);

			// T14: Rename an item (update its id field).
			// Validates: non-empty, alphanumeric+underscore, unique within section.
			static bool RenameItem(Json::Value& sceneRoot,
			                       const char*  itemType,
			                       const char*  oldId,
			                       const char*  newId,
			                       char* errBuf, int errBufSize);

		private:
			static const char* ArrayKey(const char* itemType);
			static const char* ExtractId(const Json::Value& val, char* buf, int bufSize);
			static bool        IdExists(const Json::Value& arr, const char* id);
			static bool        IsValidId(const char* id);
		};
	}
}
