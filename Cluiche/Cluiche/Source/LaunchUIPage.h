#pragma once

#include <DiaUI/IUISystem.h>
#include <DiaUI/Page.h>

#include <DiaCore/Strings/String64.h>

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// Interface for the UI to call back to the phase
	//
	////////////////////////////////////////////////////
	class LaunchUIPageExternalInterface
	{
	public:
		virtual void RequestLaunchLevel(const Dia::Core::Containers::String64& levelName) = 0;
	};

	////////////////////////////////////////////////////
	//
	// C++ Shell for the UI Launch Page
	//
	////////////////////////////////////////////////////
	class LaunchUIPage : public Dia::UI::Page
	{
	public:
		LaunchUIPage(Cluiche::LaunchUIPageExternalInterface* parentPhase);
		~LaunchUIPage();

		void InitializePage();

		void LaunchLevel(const Dia::UI::BoundMethodArgs& arg);

	private:
		LaunchUIPage() {}

		Cluiche::LaunchUIPageExternalInterface* mPhaseInterface;
	};
}
