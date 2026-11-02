#pragma once

#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/Notification/NotificationService.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/EditorAPI/EditorActionQueueService.h>
#include <DiaEditor/AppEditor/AppEditorController.h>
#include <DiaEditor/Project/GameSwitchController.h>

namespace Dia { namespace Editor { class IEditorPlugin; class EditorView; class WebUIBridge; } }

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule;
		class EditorViewModule;
		class EditorActionModule;

		class PluginLoaderModule : public Dia::ApplicationFlow::MainModule, public Dia::Editor::IPluginLoader
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit PluginLoaderModule(const Dia::Core::StringCRC& instanceId);

			void SetBridge(Dia::Editor::WebUIBridge* bridge);
			void LoadBuiltInPlugins();
			void RestoreLayoutPlugins();
			// Layout-restore core: for each saved panel name, load the matching registered plugin (LoadPlugin applies
			// the game-scope gate, so out-of-scope game plugins are skipped). Public so it can be tested without a UI.
			void RestoreLayoutPanels(const char* const* panelNames, unsigned int panelCount);
			// Game-scope listener: closes out-of-scope plugins when the active game changes. DoStart/DoStop call
			// these; public so tests can attach the real listener without the full editor application.
			void RegisterGameScopeListener();
			void UnregisterGameScopeListener();
			void LoadManifest(const char* manifestPath);
			void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId) override;
			bool UnloadPlugin(const Dia::Core::StringCRC& typeId) override;
			bool IsPluginTypeLoaded(const Dia::Core::StringCRC& typeId) const override;
			bool IsPluginPinned(const Dia::Core::StringCRC& typeId) const override;
			void RegisterView(Dia::Editor::EditorView* view);
			unsigned int CollectDirtyGameScoped(Dia::Editor::SwitchDirtyEntry* out, unsigned int max) override;
			bool SaveLoadedPlugin(const Dia::Core::StringCRC& typeId) override;

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			void PruneHeadlessPanels();
			void CloseOutOfScopePlugins();
			void CloseAllGameScopedPlugins();
			static void OnGameScopeChanged(void* userData, const Dia::Core::StringCRC& oldGame, const Dia::Core::StringCRC& newGame);

			struct LoadedPluginEntry
			{
				Dia::Core::StringCRC typeId;
				Dia::Editor::IEditorPlugin* plugin;
			};

			Dia::Editor::EditorPluginContext  mContext;
			Dia::Editor::PluginServiceLocator mServiceLocator;
			Dia::Editor::NotificationService  mNotificationService;
			Dia::Editor::EditorView*          mView;

			static const unsigned int kMaxPlugins = 16;
			Dia::Core::Containers::DynamicArrayC<LoadedPluginEntry, kMaxPlugins> mLoadedPlugins;

			Dia::ApplicationFlow::ModuleRef<EditorModelModule>  mModelRef;
			Dia::ApplicationFlow::ModuleRef<EditorViewModule>   mViewRef;
			Dia::ApplicationFlow::ModuleRef<EditorActionModule> mActionModuleRef;

			// Service wrappers — registered on mServiceLocator in SetBridge if EditorActionModule is present.
			Dia::Editor::EditorActionRegistryService* mRegistryService = nullptr;
			Dia::Editor::EditorActionQueueService*    mQueueService    = nullptr;

			// Cross-cutting context controller — wired in SetBridge, registered on service locator.
			Dia::Editor::AppEditorController mAppEditorController;

			// Game-switch bridge adapter — wired in SetBridge, registered on service locator.
			Dia::Editor::GameSwitchController mGameSwitchController;
		};
	}
}
