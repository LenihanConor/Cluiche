#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/MVC/EditorModel.h>

namespace Dia
{
	namespace Window { class IWindow; class Win32Window; }
	namespace UICEF  { class CEFUISystem; }
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

			void SetModel(Dia::Editor::EditorModel* model) { mModel = model; }

			Dia::Editor::EditorView& GetView() { return mView; }

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Editor::EditorView mView;
			Dia::Editor::EditorModel* mModel;
			Dia::Window::Win32Window* mWindow;
			Dia::UICEF::CEFUISystem* mCEFSystem;
		};
	}
}
