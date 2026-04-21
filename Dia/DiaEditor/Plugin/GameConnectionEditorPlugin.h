#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>

namespace Dia
{
	namespace Editor
	{
		class GameConnectionEditorPlugin : public IEditorPlugin
		{
		public:
			const char* GetName() const override { return "Game Connection"; }
			const char* GetVersion() const override { return "1.0"; }
			const char* GetDescription() const override { return "Built-in game connection panel"; }
			const char* GetUIPath() const override { return "dia://editor/gameconnection/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }

			void OnLoad(const EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

		private:
			GameConnectionManager mManager;
			GameConnectionController mController;
		};
	}
}
