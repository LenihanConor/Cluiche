#pragma once

#include <DiaApplicationFlow/ApplicationModule.h>
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

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;

		private:
			Dia::Editor::EditorViewController mController;
		};
	}
}
