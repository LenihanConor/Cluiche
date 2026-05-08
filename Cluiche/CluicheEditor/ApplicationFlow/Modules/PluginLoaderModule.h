#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEditor/Plugin/EditorPluginContext.h>
#include <DiaEditor/Plugin/IPluginLoader.h>
#include <DiaEditor/Plugin/PluginServiceLocator.h>

namespace Dia { namespace Editor { class IEditorPlugin; class EditorView; class WebUIBridge; } }

namespace Cluiche
{
	namespace Editor
	{
		class PluginLoaderModule : public Dia::Application::Module, public Dia::Editor::IPluginLoader
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			PluginLoaderModule(Dia::Application::ProcessingUnit* pu);

			void SetBridge(Dia::Editor::WebUIBridge* bridge);
			void LoadBuiltInPlugins();
			void LoadManifest(const char* manifestPath);
			void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId) override;
			bool UnloadPlugin(const Dia::Core::StringCRC& typeId) override;
			bool IsPluginTypeLoaded(const Dia::Core::StringCRC& typeId) const override;
			bool IsPluginPinned(const Dia::Core::StringCRC& typeId) const override;
			void RegisterView(Dia::Editor::EditorView* view);

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			struct LoadedPluginEntry
			{
				Dia::Core::StringCRC typeId;
				Dia::Editor::IEditorPlugin* plugin;
			};

			Dia::Editor::EditorPluginContext mContext;
			Dia::Editor::PluginServiceLocator mServiceLocator;
			Dia::Editor::EditorView* mView;

			static const unsigned int kMaxPlugins = 16;
			Dia::Core::Containers::DynamicArrayC<LoadedPluginEntry, kMaxPlugins> mLoadedPlugins;
		};
	}
}
