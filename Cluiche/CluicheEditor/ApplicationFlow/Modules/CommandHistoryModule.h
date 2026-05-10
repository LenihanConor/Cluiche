#pragma once

#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaEditor/Command/CommandHistory.h>

namespace Cluiche
{
	namespace Editor
	{
		class CommandHistoryModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CommandHistoryModule(Dia::Application::ProcessingUnit* pu);

			Dia::Editor::CommandHistory& GetHistory() { return mHistory; }

		protected:
			void DoStop() override;

		private:
			Dia::Editor::CommandHistory mHistory;
		};
	}
}
