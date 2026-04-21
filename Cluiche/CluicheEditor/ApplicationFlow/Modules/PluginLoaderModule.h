#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Editor { class IEditorPlugin; class EditorModel; class EditorView; } }

namespace Cluiche
{
	namespace Editor
	{
		class PluginLoaderModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			PluginLoaderModule(Dia::Application::ProcessingUnit* pu, Dia::Editor::EditorModel* model);

			void LoadBuiltInPlugins();
			void LoadManifest(const char* manifestPath);
			void LoadPlugin(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& instanceId);
			void RegisterView(Dia::Editor::EditorView* view);

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Editor::EditorModel* mModel;
			Dia::Editor::EditorView* mView;

			static const unsigned int kMaxPlugins = 16;
			Dia::Core::Containers::DynamicArrayC<Dia::Editor::IEditorPlugin*, kMaxPlugins> mLoadedPlugins;
		};
	}
}
