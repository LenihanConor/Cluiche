#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaEditor/Command/CommandHistory.h>

namespace Cluiche
{
	namespace Editor
	{
		class CommandHistoryModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CommandHistoryModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::CommandHistory& GetHistory() { return mHistory; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::CommandHistory mHistory;
		};
	}
}
