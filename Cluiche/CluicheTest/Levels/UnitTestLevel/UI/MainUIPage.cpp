#include "UI\MainUIPage.h"


namespace Cluiche
{
	namespace UnitTestLevel
	{
		MainUIPage::MainUIPage(MainUIPageExternalInterface* parentPhase)
			: Dia::UI::Page()
			, mPhaseInterface(parentPhase)
		{}

		MainUIPage::~MainUIPage()
		{
			mPhaseInterface = nullptr;
		}

		void MainUIPage::InitializePage()
		{
			Initialize(Dia::Core::FilePath("root", "UnitTestLevel/", "main.html"));

			BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("Application_ExitLevel",
				Dia::UI::BoundMethod::MethodPtr(this, &MainUIPage::RequestExitLevel)));
		}

		void MainUIPage::RequestExitLevel(const Dia::UI::BoundMethodArgs& arg)
		{
			mPhaseInterface->RequestExitLevel();
		}
	}
}