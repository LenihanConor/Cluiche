#include "UI\DummyUIPage.h"


namespace Cluiche
{
	namespace DummyLevel
	{
		DummyUIPage::DummyUIPage(DummyUIPageExternalInterface* parentPhase)
			: Dia::UI::Page()
			, mPhaseInterface(parentPhase)
		{}

		DummyUIPage::~DummyUIPage()
		{
			mPhaseInterface = nullptr;
		}

		void DummyUIPage::InitializePage()
		{
			Initialize(Dia::Core::FilePath("root", "DummyLevel/", "dummyLevel.html"));

			BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("Application_ExitLevel",
				Dia::UI::BoundMethod::MethodPtr(this, &DummyUIPage::RequestExitLevel)));
		}

		void DummyUIPage::RequestExitLevel(const Dia::UI::BoundMethodArgs& arg)
		{
			mPhaseInterface->RequestExitLevel();
		}
	}
}