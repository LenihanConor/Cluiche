#pragma once

#include <DiaEditor/MVC/IEditorContext.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		// Tracks cross-cutting editor state (focus, edit target, selection) and
		// exposes it to the UI via the "app_editor.get_active_context" request handler.
		class AppEditorController
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;  // StringCRC("AppEditorController")

			AppEditorController();

			// Call after DiaEditorAPI is initialized. bridge and context must outlive this object.
			// api parameter is forward-declared for now (will be used in Task 3).
			void Initialize(WebUIBridge* bridge, IEditorContext* context);
			void Shutdown();

			// Called by plugins to update tracked state. All must be called from main thread only.
			void SetFocus(Dia::Core::StringCRC pluginId, const char* panel);
			void ClearFocus();
			void SetEditTarget(Dia::Core::StringCRC type, Dia::Core::StringCRC id, const char* name, bool dirty);
			void SetDirty(bool dirty);
			void SetSelection(Dia::Core::StringCRC type, Dia::Core::StringCRC id, const char* name);
			void ClearEditTarget();
			void ClearSelection();

			// Called by Task 2/3 once DiaEditorAPI exists — internal, not public API
			Json::Value HandleGetActiveContext(const Json::Value& params);

		private:
			static const unsigned int kMaxNameLength = 128;

			WebUIBridge*    mBridge  = nullptr;
			IEditorContext* mContext = nullptr;

			Dia::Core::StringCRC mFocusPluginId;
			char                 mFocusPanel[kMaxNameLength];

			Dia::Core::StringCRC mEditTargetType;
			Dia::Core::StringCRC mEditTargetId;
			char                 mEditTargetName[kMaxNameLength];
			bool                 mEditTargetDirty = false;

			Dia::Core::StringCRC mSelectionType;
			Dia::Core::StringCRC mSelectionId;
			char                 mSelectionName[kMaxNameLength];
		};

	} // Editor
} // Dia
