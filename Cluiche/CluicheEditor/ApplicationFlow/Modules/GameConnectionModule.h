#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule;

		class GameConnectionModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit GameConnectionModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::GameConnectionManager& GetConnectionManager() { return mConnectionManager; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::GameConnectionManager mConnectionManager;

			Dia::ApplicationFlow::ModuleRef<EditorModelModule> mModelRef;
		};
	}
}
