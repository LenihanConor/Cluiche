#pragma once

#include "DiaEditor/Plugin/EditorPluginRegistry.h"
#include <DiaCore/CRC/StringCRC.h>
#include <cstring>

// Registers an editor plugin for automatic discovery at startup.
// Usage in .cpp file:
//   REGISTER_EDITOR_PLUGIN(MyPlugin, "MyPlugin")
#define REGISTER_EDITOR_PLUGIN(ClassName, TypeName) \
	namespace { \
		struct ClassName##EditorFactory : public Dia::Editor::IEditorPluginFactory { \
			Dia::Editor::IEditorPlugin* Create() override { return new ClassName(); } \
			Dia::Editor::EditorPluginInfo GetPluginInfo() override { \
				if (!mInfoCached) { \
					ClassName* tmp = new ClassName(); \
					strncpy_s(mInfo.name, sizeof(mInfo.name), tmp->GetName(), _TRUNCATE); \
					strncpy_s(mInfo.version, sizeof(mInfo.version), tmp->GetVersion(), _TRUNCATE); \
					strncpy_s(mInfo.description, sizeof(mInfo.description), tmp->GetDescription(), _TRUNCATE); \
					mInfo.layoutMode = tmp->GetLayoutMode(); \
					delete tmp; \
					mInfoCached = true; \
				} \
				return mInfo; \
			} \
			bool mInfoCached = false; \
			Dia::Editor::EditorPluginInfo mInfo = {}; \
		}; \
		static ClassName##EditorFactory g_##ClassName##EditorFactory; \
		struct ClassName##EditorRegistrar { \
			ClassName##EditorRegistrar() { \
				Dia::Editor::EditorPluginRegistry::Instance().RegisterPlugin( \
					Dia::Core::StringCRC(TypeName), &g_##ClassName##EditorFactory); \
			} \
		}; \
		static ClassName##EditorRegistrar g_##ClassName##EditorRegistrar; \
	}
