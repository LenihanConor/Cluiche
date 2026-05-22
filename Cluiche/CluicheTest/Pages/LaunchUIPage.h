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
		virtual int GetNavigableStageCount() = 0;
		virtual Dia::Core::Containers::String64 GetNavigableStageName(int index) = 0;
		virtual Dia::Core::Containers::String64 GetStageStatus(int index) = 0;
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
		Dia::UI::BoundMethodValue GetStageCount(const Dia::UI::BoundMethodArgs& arg);
		Dia::UI::BoundMethodValue GetStageName(const Dia::UI::BoundMethodArgs& arg);
		Dia::UI::BoundMethodValue GetStageStatus(const Dia::UI::BoundMethodArgs& arg);

	private:
		LaunchUIPage(): mPhaseInterface(nullptr) {}

		Cluiche::LaunchUIPageExternalInterface* mPhaseInterface;
	};
}
