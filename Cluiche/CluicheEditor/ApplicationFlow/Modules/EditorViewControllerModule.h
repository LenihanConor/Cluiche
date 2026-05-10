#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaEditor/MVC/EditorViewController.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule;
		class CommandHistoryModule;

		class EditorViewControllerModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorViewControllerModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::EditorViewController& GetController() { return mController; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::EditorViewController mController;

			Dia::ApplicationFlow::ModuleRef<EditorModelModule>     mModelRef;
			Dia::ApplicationFlow::ModuleRef<CommandHistoryModule>  mHistoryRef;
		};
	}
}
