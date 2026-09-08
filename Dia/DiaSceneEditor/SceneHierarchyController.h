#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace SceneEditor
	{
		// Manages the left-panel hierarchy state: normalized item list, search filter,
		// and selection.  All state is held here; the UI reflects it via JSON payloads.
		class SceneHierarchyController
		{
		public:
			// Build full hierarchy (no filter) from a loaded .diascene root.
			// Returns { layers:[], cameras:[], lights:[], entities:[] } with each item
			// normalised to { id, label, type, enabled }.
			Json::Value BuildHierarchyJson(const Json::Value& sceneRoot) const;

			// Build filtered hierarchy: only items whose id or label contain `filter`
			// (case-insensitive).  Empty filter returns all items.
			Json::Value BuildFilteredHierarchyJson(const Json::Value& sceneRoot,
			                                       const char* filter) const;

			// Selection state — stored so the UI can round-trip without re-sending it.
			void        SetSelection(const char* type, const char* id);
			void        ClearSelection();
			Json::Value GetSelectionJson() const;

		private:
			// Normalise one section array; filter applied if non-null/non-empty.
			Json::Value NormaliseSection(const Json::Value& arr,
			                             const char* sectionType,
			                             const char* filter) const;

			// Extract the string value from either a plain string or {"value":"..."} wrapper.
			static const char* ExtractId(const Json::Value& val, char* buf, int bufSize);

			char mSelectionType[64] = {};
			char mSelectionId[256]  = {};
		};
	}
}
