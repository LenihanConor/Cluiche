#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaEditor/MVC/EditorViewController.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorViewControllerModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorViewControllerModule(Dia::Application::ProcessingUnit* pu);

			Dia::Editor::EditorViewController& GetController() { return mController; }

		private:
			Dia::Editor::EditorViewController mController;
		};
	}
}
