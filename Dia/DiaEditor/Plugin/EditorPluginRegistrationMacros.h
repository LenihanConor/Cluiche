#pragma once

#include "DiaEditor/Plugin/EditorPluginRegistry.h"
#include <DiaCore/CRC/StringCRC.h>

// Registers an editor plugin for automatic discovery at startup.
// Usage in .cpp file:
//   REGISTER_EDITOR_PLUGIN(MyPlugin, "MyPlugin")
#define REGISTER_EDITOR_PLUGIN(ClassName, TypeName) \
	namespace { \
		struct ClassName##EditorFactory : public Dia::Editor::IEditorPluginFactory { \
			Dia::Editor::IEditorPlugin* Create() override { return new ClassName(); } \
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
