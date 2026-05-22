#include "Pages/LaunchUIPage.h"


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
		Initialize(Dia::Core::FilePath("ui_common", "BootStrap/", "bootscreen.html"));

		BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("Application_LaunchLevel",
				Dia::UI::BoundMethod::MethodPtr(this, &LaunchUIPage::LaunchLevel)));
		BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("Application_GetStageCount",
				Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &LaunchUIPage::GetStageCount)));
		BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("Application_GetStageName",
				Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &LaunchUIPage::GetStageName)));
		BindMethod(Dia::UI::BoundMethod::CreateBoundMethodWithRetVal("Application_GetStageStatus",
				Dia::UI::BoundMethod::MethodPtrWithRetVal(this, &LaunchUIPage::GetStageStatus)));
	}

	void LaunchUIPage::LaunchLevel(const Dia::UI::BoundMethodArgs& arg)
	{
		DIA_ASSERT(arg.Size() == 1, "Unexpected inputs from UI");

		const Dia::Core::Containers::String64& levelName = arg.At(0).GetString();

		mPhaseInterface->RequestLaunchLevel(levelName);
	}

	Dia::UI::BoundMethodValue LaunchUIPage::GetStageCount(const Dia::UI::BoundMethodArgs&)
	{
		return Dia::UI::BoundMethodValue(mPhaseInterface->GetNavigableStageCount());
	}

	Dia::UI::BoundMethodValue LaunchUIPage::GetStageName(const Dia::UI::BoundMethodArgs& arg)
	{
		DIA_ASSERT(arg.Size() == 1, "Expected index argument");
		int index = static_cast<int>(arg.At(0).GetDouble());
		return Dia::UI::BoundMethodValue(mPhaseInterface->GetNavigableStageName(index));
	}

	Dia::UI::BoundMethodValue LaunchUIPage::GetStageStatus(const Dia::UI::BoundMethodArgs& arg)
	{
		DIA_ASSERT(arg.Size() == 1, "Expected index argument");
		int index = static_cast<int>(arg.At(0).GetDouble());
		return Dia::UI::BoundMethodValue(mPhaseInterface->GetStageStatus(index));
	}
}