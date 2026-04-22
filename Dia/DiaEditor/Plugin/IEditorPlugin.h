#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <cstring>

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

		struct EditorToolbarItem
		{
			char label[32];
			char iconChar[4];
			bool pinned;

			EditorToolbarItem() : pinned(false)
			{
				label[0] = '\0';
				iconChar[0] = '\0';
			}
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

			virtual EditorToolbarItem GetToolbarItem() const
			{
				EditorToolbarItem item;
				const char* name = GetName();
				if (name != nullptr && name[0] != '\0')
				{
					item.iconChar[0] = name[0];
					item.iconChar[1] = '\0';
					strncpy_s(item.label, sizeof(item.label), name, _TRUNCATE);
				}
				item.pinned = false;
				return item;
			}
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
