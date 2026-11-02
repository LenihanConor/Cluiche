#pragma once

#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/Plugin/EditorPluginRegistry.h>

namespace Dia
{
	namespace Editor
	{
		class PluginBrowserEditorPlugin : public EditorPluginBase
		{
		public:
			PluginBrowserEditorPlugin()
				: EditorPluginBase({
					"Plugin Browser",
					"1.0",
					"Browse and load available editor plugins",
					"dia://plugins/pluginbrowser/index.html",
					LayoutMode::kDockable,
					nullptr,
					"P",
					true
				})
			{}

			void OnUpdate(float deltaTime) override;

		protected:
			void OnPluginLoad() override;
			void OnPluginUnload() override;

		private:
			Json::Value BuildAvailableList() const;
			Json::Value HandleLoad(const Json::Value& data, const char* instanceSuffix);

			// Palette commands plugin.load.<id> / plugin.unload.<id>. Tracked in mPaletteTypes so they are
			// never double-registered or unregistered when absent.
			void RegisterPaletteCommands(const Dia::Core::StringCRC& typeId);
			void UnregisterPaletteCommands(const Dia::Core::StringCRC& typeId);
			bool HasPaletteCommands(const Dia::Core::StringCRC& typeId) const;
			void SyncPaletteCommandsToScope();
			void NotifyBrowserChanged();
			static void OnGameScopeChanged(void* userData, const Dia::Core::StringCRC& oldGame, const Dia::Core::StringCRC& newGame);

			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, EditorPluginRegistry::kMaxPlugins> mPaletteTypes;
			bool mGameScopeListenerAdded = false;
			bool mPaletteSyncPending = false;   // scope changed inside a command callback; applied in OnUpdate
		};
	}
}
