#include "ApplicationFlow/Modules/SimUIProxyModule.h"

#include <DiaUI/IUISystem.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaGraphics/Frame/FrameData.h>

namespace Cluiche
{
	const Dia::Core::StringCRC SimUIProxyModule::kUniqueId("SimUIProxyModule");

	SimUIProxyModule::SimUIProxyModule(Dia::Application::ProcessingUnit* associatedProcessingUnit, Dia::Graphics::FrameData* renderFrameBuffer)
		: Dia::Application::Module(associatedProcessingUnit, kUniqueId, Dia::Application::Module::RunningEnum::kUpdate)
		, mUISystem(nullptr)
		, mRenderFrameBuffer(renderFrameBuffer)
	{}

	void SimUIProxyModule::Attach(Dia::UI::IUISystem* ui)
	{
		DIA_ARRAY__(mUISystem == nullptr, "mUISystem is allocated already");
		DIA_ARRAY__(ui != nullptr, "Cannot assign a null pointer ui system");

		mUISystem = ui;
	}

	void SimUIProxyModule::Detach()
	{
		DIA_ARRAY__(mUISystem != nullptr, "mUISystem is null");

		mUISystem = nullptr;
	}

	void SimUIProxyModule::DoUpdate()
	{
		if (mUISystem && mUISystem->IsPageLoaded())
		{
			Dia::UI::UIDataBuffer uiBuffer;
			mUISystem->FetchUIDataBuffer(uiBuffer);
			mRenderFrameBuffer->RequestDrawUI(uiBuffer);
		}
	}

	bool SimUIProxyModule::IsUISystenAvailable()const
	{
		return mUISystem == nullptr;
	}

	const Dia::UI::IUISystem* SimUIProxyModule::GetUISystem()const
	{
		return mUISystem;
	}
}