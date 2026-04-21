#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>

namespace Cluiche
{
	namespace Editor
	{
		class GameConnectionModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit GameConnectionModule(Dia::Application::ProcessingUnit* pu);

			Dia::Editor::GameConnectionManager& GetManager() { return mManager; }
			Dia::Editor::GameConnectionController& GetController() { return mController; }

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Editor::GameConnectionManager mManager;
			Dia::Editor::GameConnectionController mController;
		};
	}
}
