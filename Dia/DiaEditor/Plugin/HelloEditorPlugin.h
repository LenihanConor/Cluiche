#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>

namespace Dia
{
	namespace Editor
	{
		class HelloEditorPlugin : public IEditorPlugin
		{
		public:
			HelloEditorPlugin();

			const char* GetName() const override { return "HelloEditorPlugin"; }
			const char* GetVersion() const override { return "1.0"; }
			const char* GetDescription() const override { return "Demonstrates full plugin discovery pipeline"; }
			const char* GetUIPath() const override { return "dia://plugins/hello/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }

			void OnLoad(const EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

			bool IsLoaded() const { return mLoaded; }

		private:
			bool mLoaded;
		};
	}
}
