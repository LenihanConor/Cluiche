#pragma once

#include <DiaApplicationFlow/MainModule.h>
#include <DiaEditor/Command/CommandHistory.h>

namespace Cluiche
{
	namespace Editor
	{
		class CommandHistoryModule : public Dia::ApplicationFlow::MainModule
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CommandHistoryModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::CommandHistory& GetHistory() { return mHistory; }

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::CommandHistory mHistory;
		};
	}
}
