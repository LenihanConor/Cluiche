#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class IPluginLoader;

		class PluginBrowserEditorPlugin : public IEditorPlugin
		{
		public:
			const char* GetName() const override { return "Plugin Browser"; }
			const char* GetVersion() const override { return "1.0"; }
			const char* GetDescription() const override { return "Browse and load available editor plugins"; }
			const char* GetUIPath() const override { return "dia://editor/pluginbrowser/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }

			EditorToolbarItem GetToolbarItem() const override
			{
				EditorToolbarItem item;
				strncpy_s(item.label, sizeof(item.label), "Plugin Browser", _TRUNCATE);
				item.iconChar[0] = 'P'; item.iconChar[1] = '\0';
				item.pinned = true;
				return item;
			}

			void OnLoad(const EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

		private:
			WebUIBridge* mBridge;
			IPluginLoader* mPluginLoader;
		};
	}
}
