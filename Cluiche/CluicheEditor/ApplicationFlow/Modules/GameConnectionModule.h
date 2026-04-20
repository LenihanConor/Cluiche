#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>

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

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Editor::GameConnectionManager mManager;
		};
	}
}
