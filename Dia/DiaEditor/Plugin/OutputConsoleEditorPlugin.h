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
			const char* GetUIPath() const override { return "dia://editor/outputconsole/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }
			void OnLoad(const EditorPluginContext& /*context*/) override {}
			void OnUnload() override {}
			void OnUpdate(float /*deltaTime*/) override {}
		};
	}
}
