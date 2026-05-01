#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>

namespace Dia
{
	namespace Editor
	{
		class OutputConsoleEditorPlugin : public IEditorPlugin
		{
		public:
			const char* GetName() const override { return "Output Console"; }
			const char* GetVersion() const override { return "1.0"; }
			const char* GetDescription() const override { return "Built-in output console panel"; }
			const char* GetUIPath() const override { return "dia://plugins/outputconsole/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }

			EditorToolbarItem GetToolbarItem() const override
			{
				EditorToolbarItem item;
				strncpy_s(item.label, sizeof(item.label), "Output Console", _TRUNCATE);
				item.iconChar[0] = 'C'; item.iconChar[1] = '\0';
				item.pinned = true;
				return item;
			}

			void OnLoad(const EditorPluginContext& /*context*/) override {}
			void OnUnload() override {}
			void OnUpdate(float /*deltaTime*/) override {}
		};
	}
}
