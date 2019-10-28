#pragma once

#include <DiaUI/IUISystem.h>
#include <DiaUI/Page.h>

#include <DiaCore/Strings/String64.h>

namespace Cluiche
{
	namespace UnitTestLevel
	{
		////////////////////////////////////////////////////
		//
		// Interface for the UI to call back to the phase
		//
		////////////////////////////////////////////////////
		class MainUIPageExternalInterface
		{
		public:
			virtual void RequestExitLevel() = 0;
		};

		////////////////////////////////////////////////////
		//
		// C++ Shell for the UI Launch Page
		//
		////////////////////////////////////////////////////
		class MainUIPage : public Dia::UI::Page
		{
		public:
			MainUIPage(MainUIPageExternalInterface* parentPhase);
			~MainUIPage();

			void InitializePage();

			void LaunchLevel(const Dia::UI::BoundMethodArgs& arg);

		private:
			MainUIPage(): mPhaseInterface(nullptr) {}

			void RequestExitLevel(const Dia::UI::BoundMethodArgs& arg);

			MainUIPageExternalInterface* mPhaseInterface;
		};
	}
}
