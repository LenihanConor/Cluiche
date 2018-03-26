#pragma once

#include <DiaUI/IUISystem.h>
#include <DiaUI/Page.h>

#include <DiaCore/Strings/String64.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		////////////////////////////////////////////////////
		//
		// Interface for the UI to call back to the phase
		//
		////////////////////////////////////////////////////
		class DummyUIPageExternalInterface
		{
		public:
			virtual void RequestExitLevel() = 0;
		};

		////////////////////////////////////////////////////
		//
		// C++ Shell for the UI Launch Page
		//
		////////////////////////////////////////////////////
		class DummyUIPage : public Dia::UI::Page
		{
		public:
			DummyUIPage(DummyUIPageExternalInterface* parentPhase);
			~DummyUIPage();

			void InitializePage();

			void LaunchLevel(const Dia::UI::BoundMethodArgs& arg);

		private:
			DummyUIPage() {}

			void RequestExitLevel(const Dia::UI::BoundMethodArgs& arg);

			DummyUIPageExternalInterface* mPhaseInterface;
		};
	}
}
