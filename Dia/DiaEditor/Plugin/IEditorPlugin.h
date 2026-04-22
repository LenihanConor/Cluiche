#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>

namespace Dia
{
	namespace Editor
	{
		enum class LayoutMode
		{
			kFullScreen,
			kDockable
		};

		struct EditorPluginInfo
		{
			char name[64];
			char version[16];
			char description[256];
			LayoutMode layoutMode;
		};

		class IEditorPlugin
		{
		public:
			virtual ~IEditorPlugin() = default;

			virtual const char* GetName() const = 0;
			virtual const char* GetVersion() const = 0;
			virtual const char* GetDescription() const = 0;
			virtual const char* GetUIPath() const = 0;
			virtual LayoutMode GetLayoutMode() const = 0;

			virtual void OnLoad(const EditorPluginContext& context) = 0;
			virtual void OnUnload() = 0;
			virtual void OnUpdate(float deltaTime) = 0;
		};

		class IEditorPluginFactory
		{
		public:
			virtual ~IEditorPluginFactory() = default;
			virtual IEditorPlugin* Create() = 0;
			virtual EditorPluginInfo GetPluginInfo() { return EditorPluginInfo{}; }
		};
	}
}
