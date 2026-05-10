#pragma once

#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaEditor/MVC/EditorView.h>

namespace Dia
{
	namespace Window { class IWindow; }
	namespace UI { class IUISystem; }
}

namespace Cluiche
{
	namespace Editor
	{
		class EditorViewModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorViewModule(Dia::Application::ProcessingUnit* pu);
			~EditorViewModule();

			Dia::Editor::EditorView& GetView() { return mView; }

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Editor::EditorView mView;
			Dia::Window::IWindow*   mWindow;
			Dia::UI::IUISystem*     mUISystem;
		};
	}
}
