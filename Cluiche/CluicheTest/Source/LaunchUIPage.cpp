#include "Source/LaunchUIPage.h"


namespace Cluiche
{
	LaunchUIPage::LaunchUIPage(Cluiche::LaunchUIPageExternalInterface* parentPhase)
		: Dia::UI::Page()
		, mPhaseInterface(parentPhase)
	{}

	LaunchUIPage::~LaunchUIPage()
	{
		mPhaseInterface = nullptr;
	}

	void LaunchUIPage::InitializePage()
	{
		Initialize(Dia::Core::FilePath("root", "BootStrap/", "bootscreen.html"));
			
		BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("Application_LaunchLevel",
				Dia::UI::BoundMethod::MethodPtr(this, &LaunchUIPage::LaunchLevel)));
	}

	void LaunchUIPage::LaunchLevel(const Dia::UI::BoundMethodArgs& arg)
	{
		DIA_ASSERT(arg.Size() == 1, "Unexpected inputs from UI");

		const Dia::Core::Containers::String64& levelName = arg.At(0).GetString();

		mPhaseInterface->RequestLaunchLevel(levelName);
	}
}