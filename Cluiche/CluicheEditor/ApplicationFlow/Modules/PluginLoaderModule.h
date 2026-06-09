#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/Notification/NotificationService.h>
#include <DiaEditor/EditorAPI/EditorActionRegistryService.h>
#include <DiaEditor/AppEditor/AppEditorController.h>

namespace Dia { namespace Editor { class IEditorPlugin; class EditorView; class WebUIBridge; } }

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule;
		class EditorViewModule;
		class EditorActionModule;

		class PluginLoaderModule : public Dia::ApplicationFlow::Module, public Dia::Editor::IPluginLoader
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit PluginLoaderModule(const Dia::Core::StringCRC& instanceId);

			void SetBridge(Dia::Editor::WebUIBridge* bridge);
			void LoadBuiltInPlugins();
			void RestoreLayoutPlugins();
			void LoadManifest(const char* manifestPath);
			void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId) override;
			bool UnloadPlugin(const Dia::Core::StringCRC& typeId) override;
			bool IsPluginTypeLoaded(const Dia::Core::StringCRC& typeId) const override;
			bool IsPluginPinned(const Dia::Core::StringCRC& typeId) const override;
			void RegisterView(Dia::Editor::EditorView* view);

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			void PruneHeadlessPanels();

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

			// Service wrapper — registered on mServiceLocator in SetBridge if EditorActionModule is present.
			Dia::Editor::EditorActionRegistryService* mRegistryService = nullptr;

			// Cross-cutting context controller — wired in SetBridge, registered on service locator.
			Dia::Editor::AppEditorController mAppEditorController;
		};
	}
}
